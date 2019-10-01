
#include "socketconnection.h"
#include "serversocket.h"
#include "threadutils.h"
#if !(defined(WIN32) || defined(__WIN32) || defined(__WIN32__) || defined(FORCE_WIN32))
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#else
#include <winsock2.h>
typedef SSIZE_T ssize_t;
#endif

SocketConnection::SocketConnection( SocketConnectionOwner* owner, PCQueue<Packet*>* input_buffer_ptr)
 : SocketConnection_Base(owner, input_buffer_ptr), active(false)
{
    DEBUG_REPORT_LOCATION;
}


SocketConnection::~SocketConnection()
{
    DEBUG_REPORT_LOCATION;
}


void SocketConnection::Activate()
{
    Lock();
    DEBUG_REPORT_LOCATION;
    if(!active)
    {
        //start the reader/writer threads.
        pthread_create(&writer_id,NULL,SocketConnection::Writer,this);
        pthread_create(&reader_id,NULL,SocketConnection::Reader,this);
        active = true;
    }
    Unlock();
}


/*
    This needs to be modified so we can deactivate politely.  That is,
    we should be able to send out a final packet that tells the client
    "goodbye," rather than just hanging up on them.

    Currently, we can put a message in the output buffer, but there is
    no way to ensure it is flushed before we actually deactivate.  The
    best way to do this is probably to take over the reader thread,
    push out a farewell, then close the connection.  I'm indifferent
    as to whether we flush in packets that may still be in the output buffer.
*/
void SocketConnection::Deactivate()
{
    int rv = 0;
    Lock();
    DEBUG_REPORT_LOCATION;
    do
    {
        if(!active)
            break;

        //close threads
        if(!pthread_equal(pthread_self(), reader_id))
        {
            LOG_DEBUG_OUT("cancelling Reader thread.");
            rv = pthread_cancel(reader_id);
            pthread_join(reader_id, NULL);
            LOG_DEBUG_OUT("cancelled Reader thread.");
        }
        else
        {
            LOG_DEBUG_OUT("Reader thread refused to cancel itself.");
            pthread_detach(reader_id);
        }

        if(!pthread_equal(pthread_self(), writer_id))
        {
            LOG_DEBUG_OUT("cancelling Writer thread.");
            rv = pthread_cancel(writer_id);
            pthread_join(writer_id, NULL);
            LOG_DEBUG_OUT("cancelled Writer thread.");
        }
        else
        {
            LOG_DEBUG_OUT("Writer thread refused to cancel itself.");
            pthread_detach(writer_id);
        }

#if !(defined(WIN32) || defined(__WIN32) || defined(__WIN32__) || defined(FORCE_WIN32))
        close(GetDescriptor());
#else
        closesocket(GetDescriptor());
#endif

        /*
            Clear the output buffer. This assumes we don't want
            to store data that was intended for a particular client,
            then send it after it reconnects.
        */
        string* temp;
        while((temp = output_buffer.pop_front()))
        {
            delete temp;
            DEBUG_REPORT_LOCATION;
        }

        input_buffer->Producer(NULL); /* Tell everyone above us that we died.  This is okay because
                                         because every socket connection has its own packet handler
                                         thread.  However, if we ever implemented a server where
                                         incoming packets were placed into a common queue, then
                                         the death of one socket connection would incorrectly signal
                                         the death of others.

                                         TODO: Should this happen at a level above us, since this
                                            is a contract for calling Deactivate?

                                         TODO: Also this code is largely duplicated between SocketConnection
                                            and TLSSocketConnection.
                                      */

        DEBUG_REPORT_LOCATION;

        active = false;
    } while(0);

    DEBUG_REPORT_LOCATION;
    Unlock();
}


void* SocketConnection::Reader(void* void_arg)
{
    if(sizeof(PacketDataLength) != sizeof(uint32_t))
        throw("major problem.  PacketDataLength does not equal sizeof(uint32_t), which breaks ntohl");

    const unsigned int cbuf_size = sizeof(PacketType) + sizeof(PacketDataLength); //size of the type and length fields of a packet
    char buf[cbuf_size];
    SocketConnection* sc_arg = (SocketConnection*)void_arg;
    int fd = sc_arg->GetDescriptor();
    fd_set sc_fd_set;           //Declare the file descriptor set.
    int select_ret = 0;
    ssize_t read_length = 0;
    int accum_count = 0;

    DEBUG_REPORT_LOCATION;

    /*
        PTHREAD_CANCEL_DEFERRED will ensure that the threads
        are killed only when they're blocked.  This is much
        safer than just killing the threads at random.
    */
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);

    FD_ZERO(&sc_fd_set);            //initialize the file descriptor set.
    FD_SET(fd, &sc_fd_set);

    while((select_ret = select(FD_SETSIZE, &sc_fd_set, NULL, NULL, NULL)))
    {
        if(select_ret == -1)
        {
            break;
        }
        else
        {
#if !(defined(WIN32) || defined(__WIN32) || defined(__WIN32__) || defined(FORCE_WIN32))
            read_length = read(fd, buf + accum_count, cbuf_size - accum_count);
#else
            read_length = recv(fd, buf + accum_count, cbuf_size - accum_count, NULL);
#endif
            accum_count += read_length;
            if(read_length < 1)
            {
                break;
            }
            else
            {
                if(accum_count < cbuf_size)
                {
                    continue;
                }
                else
                {
                    PacketType type = buf[0];
                    PacketDataLength data_length = 0;
                    char* bytePtr = (char*)&data_length;
                    for(int i=0;i<sizeof(PacketDataLength);i++)
                        bytePtr[i] = buf[i+1];

                    data_length = ntohl(data_length);

                    char* packetDataBuffer = NULL;
                    char* seek_position = NULL;
                    if(data_length > 0)
                    {
                        packetDataBuffer = new char[data_length];
                        if(packetDataBuffer == NULL)
                        {
                            // colossal error
                            break;
                        }
                        seek_position = packetDataBuffer;
                        ssize_t data_read_length = 0;
                        do
                        {
#if !(defined(WIN32) || defined(__WIN32) || defined(__WIN32__) || defined(FORCE_WIN32))
                            data_read_length = read(fd, seek_position, data_length - (seek_position - packetDataBuffer));
#else
                            data_read_length = recv(fd, seek_position, data_length - (seek_position - packetDataBuffer), NULL);
#endif
                            seek_position += data_read_length;
                        }
                        while((data_read_length > 0) && ((size_t)(seek_position - packetDataBuffer) < data_length));
                    }

                    if(type == Packet::ERR_DISCONNECTED)
                        break;

                    try
                    {
                        Packet* new_pkt = NewPacket( (SocketConnection_Base*)sc_arg, type, (PacketDataLength)(seek_position - packetDataBuffer), packetDataBuffer, false);
                        if(new_pkt == NULL)
                        {
                            LOG_ERROR_OUT("Failed to instantiate an incoming packet (section 1). ");
                        }
                        else
                        {
                            sc_arg->input_buffer->Producer(new_pkt); // if so, create a packet and stick it in the input_buffer
                        }
                    }
                    CATCHALL
                    {
                        LOG_ERROR_OUT("Failed to instantiate an incoming packet (section 2).");
                    }
                    accum_count = 0;
                }
            }
        }
    }

    SocketConnectionOwner* owner = sc_arg->GetOwner();
    if(owner)
        owner->DeleteSocketConnection((SocketConnection_Base*)sc_arg);
    else
        sc_arg->Deactivate(); // TODO: This is a hack.  Client sockets need to clean up properly.
    pthread_exit((void*)1);
    return NULL;
}


void* SocketConnection::Writer(void* void_arg)
{
    DEBUG_REPORT_LOCATION;
    int select_ret = 0;
    fd_set sc_fd_set;
    SocketConnection* sc_arg = (SocketConnection*)void_arg;

    //Writer has no way of returning errors.  is that ok?

    /*
        PTHREAD_CANCEL_DEFERRED will ensure that the threads
        are killed only when they're blocked.  This is much
        safer than just killing the threads at random.
    */
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED,NULL);

    FD_ZERO( &sc_fd_set );
    FD_SET( sc_arg->GetDescriptor(), &sc_fd_set);

    try
    {
        while((select_ret = select(FD_SETSIZE,NULL,&sc_fd_set,NULL,NULL)))
        {
            LOG_DEBUG_OUT("entered writer's select loop.");
            if(select_ret == -1)
            {
                LOG_ERROR_OUT("select error.  interrupted?");
                break;
            }
            string* temp = sc_arg->output_buffer.Consumer();
            if(temp == NULL)
            {
                /*
                    This should never happen, since interruption
                    should only be triggered by pthread_cancel().
                    If pthread_cancel were called, the thread would
                    be terminated before we got here.
                */
                LOG_DEBUG_OUT("Consumer was interrupted.");
                break;
            }

#if !(defined(WIN32) || defined(__WIN32) || defined(__WIN32__) || defined(FORCE_WIN32))
            ssize_t write_length = write(sc_arg->GetDescriptor(), temp->c_str(), temp->length());
#else
            ssize_t write_length = send(sc_arg->GetDescriptor(), temp->c_str(), temp->length(), NULL);
#endif


            LOG_DEBUG_OUT("Successfully wrote: " << dec << write_length << " bytes: ");
#ifdef DEBUG
            //logger.Hexdump(*temp);
#endif

            // TODO: Write is not guaranteed to send the whole buffer out.  If write only sends part of our data, we need to loop around and send the rest.

            delete temp;
            if(write_length == -1) //write error.
            {
                break;
            }

            LOG_DEBUG_OUT("end writer's loop");
        }
        LOG_DEBUG_OUT("Exiting writer thread.");
    }
    catch(const char* e)
    {
        LOG_ERROR_OUT("Exception at " << __FILE__ << ":" << __LINE__ << " - " << e);
    }
    CATCHALL
    {
        LOG_ERROR_OUT("Unknown exception at " << __FILE__ << ":" << __LINE__);
    }

    SocketConnectionOwner* owner = sc_arg->GetOwner();
    if(owner)
        owner->DeleteSocketConnection((SocketConnection_Base*)sc_arg);
    else
        sc_arg->Deactivate(); // TODO: This is a hack.  Client sockets need to clean up properly.
    pthread_exit((void*)1);
    return NULL;
}


bool SocketConnection::GetActive() const
{
    return active;
}

/*
------------------------------------------------------------------------------
This software is available under 2 licenses -- choose whichever you prefer.
------------------------------------------------------------------------------
ALTERNATIVE A - MIT License
Copyright (c) 2003-2019 Bobby G. Burrough
Permission is hereby granted, free of charge, to any person obtaining a copy of 
this software and associated documentation files (the "Software"), to deal in 
the Software without restriction, including without limitation the rights to 
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies 
of the Software, and to permit persons to whom the Software is furnished to do 
so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all 
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, 
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE 
SOFTWARE.
------------------------------------------------------------------------------
ALTERNATIVE B - Public Domain (www.unlicense.org)
This is free and unencumbered software released into the public domain.
Anyone is free to copy, modify, publish, use, compile, sell, or distribute this 
software, either in source code form or as a compiled binary, for any purpose, 
commercial or non-commercial, and by any means.
In jurisdictions that recognize copyright laws, the author or authors of this 
software dedicate any and all copyright interest in the software to the public 
domain. We make this dedication for the benefit of the public at large and to 
the detriment of our heirs and successors. We intend this dedication to be an 
overt act of relinquishment in perpetuity of all present and future rights to 
this software under copyright law.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN 
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION 
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
------------------------------------------------------------------------------
*/
