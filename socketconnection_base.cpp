
#include "socketconnection_base.h"
#include "threadutils.h"
#include "fdutils.h"
#if !(defined(WIN32) || defined(__WIN32) || defined(__WIN32__) || defined(FORCE_WIN32))
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#else
#include <winsock2.h>
typedef SSIZE_T ssize_t;
#endif

SocketConnection_Base::SocketConnection_Base(SocketConnectionOwner* owner, PCQueue<Packet*>* input_buffer_ptr)
    : _owner(owner), descriptor(0), input_buffer(input_buffer_ptr), packets_out(0)
{
    DEBUG_REPORT_LOCATION;
    sem_init(&mutex,0,1);
}


SocketConnection_Base::~SocketConnection_Base()
{
    DEBUG_REPORT_LOCATION;
    sem_destroy(&mutex);
}


/*
void SocketConnection_Base::IncrementPacketsOut()
{
    DEBUG_REPORT_LOCATION;
    packets_out++;
}


bool SocketConnection_Base::DecrementPacketsOut()
{
    Lock();
    DEBUG_REPORT_LOCATION;
    packets_out--;
    if(!packets_out && !active)
        return true; //this intentionally leaves the SocketConnection locked as it's dead.  It now needs to be deleted.
    Unlock();
    return false;
}
*/

void SocketConnection_Base::Lock()
{
    DEBUG_REPORT_LOCATION;
    if(0 != sem_wait(&mutex))
        throw("sem_wait failed.");
    DEBUG_REPORT_LOCATION;
}


void SocketConnection_Base::Unlock()
{
    DEBUG_REPORT_LOCATION;
    if(0 != sem_post(&mutex))
        throw("sem_post failed.");
    DEBUG_REPORT_LOCATION;
}


SocketConnectionOwner* SocketConnection_Base::GetOwner() const
{
    return _owner;
}



bool SocketConnection_Base::Write(const char* cstring_arg)
{
    // not allowed
    return false;

    bool ret_val = false;
    string* temp;
    Lock();
    DEBUG_REPORT_LOCATION;
    temp = new string(cstring_arg);
    ret_val = output_buffer.Producer( temp );
    Unlock();

    return ret_val;
}


bool SocketConnection_Base::Write(const string& string_arg)
{
#if 1
    // not allowed
    return false;
#else
    bool ret_val = false;
    string* temp;
    Lock();
    DEBUG_REPORT_LOCATION;
    temp = new string(string_arg);
    ret_val = output_buffer.Producer( temp );
    Unlock();

    return ret_val;
#endif
}


bool SocketConnection_Base::Write(const PacketType& type_arg, const PacketDataLength& data_length_arg, const char* data_arg)
{
    DEBUG_REPORT_LOCATION;
    if(sizeof(PacketDataLength) != sizeof(uint32_t))
        throw("major problem.  PacketDataLength does not equal sizeof(uint32_t), which breaks htonl");

    bool ret_val = false;
    string* temp = NULL;
    char* bytePtr = NULL;
    Lock();
    DEBUG_REPORT_LOCATION;
    temp = new string();
    temp->push_back((unsigned char)type_arg);
    PacketDataLength corrected_data_length = htonl(data_length_arg);
    bytePtr = (char*)&corrected_data_length;
    for(int i=0;i<sizeof(PacketDataLength);i++)
    {
        temp->push_back(bytePtr[i] & 0x000000FF);
    }
    for(PacketDataLength i=0;i<data_length_arg;i++)
        temp->push_back(data_arg[i]);
    ret_val = output_buffer.Producer( temp );
    Unlock();

    return ret_val;
}


bool SocketConnection_Base::Write(const Packet& pkt)
{
    return SocketConnection_Base::Write(pkt.GetType(), pkt.GetDataLength(), pkt.GetData());
}


int SocketConnection_Base::GetDescriptor() const
{
    DEBUG_REPORT_LOCATION;
    return descriptor;
}


void SocketConnection_Base::SetDescriptor(int arg)
{
    DEBUG_REPORT_LOCATION;
    descriptor = arg;
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
