
#include "serversocket.h"

#include "socketconnection_base.h"
#include "tlssocketconnection.h" // TODO: remove this
#include "fdutils.h"
#if !(defined(WIN32) || defined(__WIN32) || defined(__WIN32__) || defined(FORCE_WIN32))
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#else
#include <winsock2.h>
//include <windows.h>
//#include <basetsd.h>
typedef SSIZE_T ssize_t;
#endif
//include <time.h>
//#include <sstream>
//#include <cstring>
#include <iostream>

using namespace std;

ServerSocket::ServerSocket(const string& ip_address, int port)
	: server_descriptor(0)
{
    DEBUG_REPORT_LOCATION;

    //listen
    sockaddr_in server_sock;
    int i(0);
    int err = 0;

#if defined(WIN32) || defined(__WIN32) || defined(__WIN32__) || defined(FORCE_WIN32)
    WORD wVersionRequested;
    WSADATA wsaData;
    wVersionRequested = MAKEWORD(2, 2);
    err = WSAStartup(wVersionRequested, &wsaData);
    if (err != 0) {
        throw("WSAStartup failed.");
    }
#endif

    server_sock.sin_family = AF_INET;
    server_sock.sin_port = htons(port);
    server_sock.sin_addr.s_addr = inet_addr(ip_address.c_str());

    for (i = 0; i < sizeof(server_sock.sin_zero); i++)
        server_sock.sin_zero[i] = 0;
    server_descriptor = socket(PF_INET, SOCK_STREAM,0);
	if(-1 == server_descriptor)
	{
		throw("socket() failed.");
	}

#if !(defined(WIN32) || defined(__WIN32) || defined(__WIN32__) || defined(FORCE_WIN32))
	int yes(1);
#else
	char yes = 1;
#endif
	err = setsockopt(server_descriptor, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
	if(err)
	{
		throw("setsockopt() failed.");
	}

    err = ::bind(server_descriptor, (const struct sockaddr*)&server_sock, sizeof(struct sockaddr_in));
	if(err)
	{
        throw("bind() failed.");
	}

    i = listen(server_descriptor, SOMAXCONN);
    if (i != 0)
    {
		throw("listen() failed.");
    }
    //Start AcceptThread thread.
    pthread_create(&_accept_thread_id,NULL,ServerSocket::AcceptThread,this);
    pthread_create(&_health_monitor_thread_id,NULL,ServerSocket::HealthMonitor,this);
}


ServerSocket::~ServerSocket()
{
    DEBUG_REPORT_LOCATION;

    //clean up buffers?!

    CloseDescriptor(server_descriptor);

#if (defined(WIN32) || defined(__WIN32) || defined(__WIN32__) || defined(FORCE_WIN32))
    WSACleanup(); // TODO: This is actually wrong in the event that there are multiple ServerSocket's.  This should be in a static dealloc function.
#endif

    pthread_cancel(_accept_thread_id);
    pthread_join(_accept_thread_id,NULL);

    pthread_cancel(_health_monitor_thread_id);
    pthread_join(_health_monitor_thread_id,NULL);

    //destroy all SocketConnections
    SocketConnection_Base* sc = NULL;
    while((sc = connection_set.pop_front()))
    {
        sc->Deactivate();
        Delete(sc);
    }

    //clean up anything that might be left over in the packet_set
    Packet* temp;
    while((temp = packet_set.pop_front()))
    {
        delete temp;
    }
}


void* ServerSocket::HealthMonitor(void* arg)
{
    ServerSocket* ss = (ServerSocket*)arg;
    while(1)
    {
        size_t size = ss->connection_set.size();
        //DEBUG_OUT("Number of socket connections: " << size);
        cout << "Number of socket connections: " << size << endl;
#if !(defined(WIN32) || defined(__WIN32) || defined(__WIN32__) || defined(FORCE_WIN32))
		sleep(5);
#else
		Sleep(5000);
#endif
    }
}


void ServerSocket::Run() const
{
    pthread_join(_accept_thread_id, NULL);
}


void ServerSocket::DeleteSocketConnection(SocketConnection_Base* sc_ptr)
{
    sc_ptr->Deactivate();
    RemoveSocketConnection(sc_ptr);
    Delete(sc_ptr);
}


void ServerSocket::RemoveSocketConnection(SocketConnection_Base* sc_ptr)
{
    DEBUG_REPORT_LOCATION;

    /*
        Need to add:  Error check for RemoveSocketConnection called
        when SocketConnection that doesn't exist in this list.
    */
    connection_set.remove(sc_ptr);
}


Packet* ServerSocket::NewPacket()
{
    DEBUG_REPORT_LOCATION;
    return packet_set.Consumer();   //Will block if there is nothing in the packet_set;
}


void ServerSocket::DeletePacket(Packet* pkt)
{
    Delete(pkt);
}


bool ServerSocket::WriteAll(const Packet* pkt)
{
    DEBUG_REPORT_LOCATION;

    bool write_success = true;
    auto visitor = [pkt, &write_success](SocketConnection_Base* connection_ptr) -> bool
    {
        write_success = connection_ptr->Write(*pkt); // Write returns a bool that is intended to indicate whether the write succeeded, but there are no circumstances where false is ever returned.        
        return write_success;
    };

    connection_set.visit_all(visitor);
    return write_success;
}



/* 
    This is essentially a broadcast function. Where, by receiving an incoming packet,
    the server calls this, that same packet is copied sent out to all connections except
    the one that originated it.
*/
bool ServerSocket::WriteAllExceptOrigin(const Packet* pkt)
{
    DEBUG_REPORT_LOCATION;

    bool write_success = true;
    auto visitor = [pkt, &write_success](SocketConnection_Base* connection_ptr) -> bool
    {
        if (connection_ptr != pkt->GetOrigin())
        {
            write_success = connection_ptr->Write(*pkt); // Write returns a bool that is intended to indicate whether the write succeeded, but there are no circumstances where false is ever returned.
        }
        return write_success;
    };

    connection_set.visit_all(visitor);
    return write_success;
}


void* ServerSocket::AcceptThread(void* void_arg)
{
    DEBUG_REPORT_LOCATION;

    /*
        PTHREAD_CANCEL_DEFERRED will ensure that the threads
        are killed only when they're blocked.  This is much
        safer than just killing the threads at random.
    */
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED,NULL);

    try
    {
        ServerSocket* my_socket = (ServerSocket*)void_arg;
        int client_descriptor;
        // the following will block until an incoming connection exists on the socket.
        while ((client_descriptor = accept(my_socket->server_descriptor, NULL, NULL))) //Currently ignoring the client socket addresses.
        {
            if (client_descriptor == -1)
            {
                LOG_ERROR_OUT("accept() failed.");
                break;
            }

            try
            {
                TLSSocketConnection* temp = (TLSSocketConnection*)NewSocketConnection(my_socket, &my_socket->packet_set);
		if(temp == NULL)
			throw("Failed to allocated new socket connection.");
                temp->SetDescriptor(client_descriptor);
                temp->PrepareServerConnection();
                my_socket->connection_set.push_back(temp);
                temp->Activate();
            }
            catch (const char* str)
            {
                cerr << "Failed to instantiate a TLSSocketConnection. Error: " << str << endl;
            }
        }
    }
    catch(const char* str)
    {
        LOG_ERROR_OUT("Exception in AcceptThread: " << str);
    }
    CATCHALL
    {
        LOG_ERROR_OUT("Unknown exception in AcceptThread.");
    }

    DEBUG_REPORT_LOCATION;
    pthread_exit((void*)1);
    return NULL;
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
