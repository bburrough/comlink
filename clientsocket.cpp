#include "clientsocket.h"

#include "socketconnection_base.h"
#include "tlssocketconnection.h"  // TODO: Remove this
#include "fdutils.h"
#if !(defined(WIN32) || defined(__WIN32) || defined(__WIN32__) || defined(FORCE_WIN32))
#include <unistd.h>
#include <netdb.h>
#else
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#endif
#include <iostream>
#include <sstream>
#include <cstring>

using namespace std;

ClientSocket::ClientSocket()
    : input_buffer(), connection(NULL)
{
    DEBUG_REPORT_LOCATION;
    connection = new TLSSocketConnection(NULL, &input_buffer); // TODO: Also, this assumes clients will never implement NewSocketConnection(), which might not be correct.
}


ClientSocket::~ClientSocket()
{
    DEBUG_REPORT_LOCATION;

#if (defined(WIN32) || defined(__WIN32) || defined(__WIN32__) || defined(FORCE_WIN32))
    WSACleanup(); // TODO: This needs to be static.
#endif

    connection->Deactivate(); // stop the threads

    //CloseDescriptor(connection->GetDescriptor());  // deactivate already does this

    delete connection;

    DEBUG_REPORT_LOCATION;

    //clean up anything left in the input buffer
    Packet* temp;
    while((temp = input_buffer.pop_front()))
    {
        delete temp;
    }

    DEBUG_REPORT_LOCATION;
}


addrinfo* NewResolvedAddress(const string& addr, int port)
{
    ostringstream port_stream;
    port_stream << port;

    struct addrinfo* result;
    struct addrinfo hints;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = 0;
    hints.ai_protocol = 0;
    hints.ai_canonname = NULL;
    hints.ai_addr = NULL;
    hints.ai_next = NULL;

    int retval = getaddrinfo(addr.c_str(), port_stream.str().c_str(), &hints, &result);
    if(retval)
    {
        return NULL;
    }

    return result;
}


void DeleteResolvedAddress(addrinfo* a)
{
    freeaddrinfo(a);
}


bool ClientSocket::Connect(const string& ip_address, int port)
{
    DEBUG_REPORT_LOCATION;

#if defined(WIN32) || defined(__WIN32) || defined(__WIN32__) || defined(FORCE_WIN32)
    WORD wVersionRequested;
    WSADATA wsaData;
    wVersionRequested = MAKEWORD(2, 2);
    int err = WSAStartup(wVersionRequested, &wsaData);
    if (err != 0) {
        return false;
    }
#endif

    connection->SetDescriptor( socket(PF_INET, SOCK_STREAM, 0) );
    if (connection->GetDescriptor() == -1)  //if true, an error has occurred.
    {
        LOG_ERROR_OUT("Failed to SetDescriptor()");
        return false;
    }

    addrinfo* addr = NewResolvedAddress(ip_address, port);
    if (addr == NULL)
    {
        LOG_ERROR_OUT("Failed to allocate an addrinfo");
        return false;
    }

    int error_ret = connect(connection->GetDescriptor(), addr->ai_addr, addr->ai_addrlen);
    DeleteResolvedAddress(addr);
    if (error_ret == -1)
    {
        LOG_ERROR_OUT("Connect failed.  errno: " << errno << " - " << strerror(errno));
        return false;
    }

    connection->PrepareClientConnection();
    
    connection->Activate();
    return true;
}


Packet* ClientSocket::NewPacket()
{
    return input_buffer.Consumer();
}

void ClientSocket::DeletePacket(Packet* pkt) const
{
    Delete(pkt);
}


/*
Packet* ClientSocket::TryRead()
{
    return input_buffer.TryConsumer();
}
*/

bool ClientSocket::Write(const char* cstring_arg)
{
    DEBUG_REPORT_LOCATION;
    // disabled
    return false;

    //return connection->Write(cstring_arg);
}


bool ClientSocket::Write(const string& string_arg)
{
    DEBUG_REPORT_LOCATION;
    // disabled
    return false;

    //return connection->Write(string_arg);
}


bool ClientSocket::Write(const PacketType& type_arg, const PacketDataLength& data_length_arg, const char* data_arg)
{
    DEBUG_REPORT_LOCATION;
    return connection->Write(type_arg, data_length_arg, data_arg);
}


bool ClientSocket::Write(const Packet& pkt)
{
    return ClientSocket::Write(pkt.GetType(), pkt.GetDataLength(), pkt.GetData());
}

void ClientSocket::DeleteSocketConnection(SocketConnection_Base* sc)
{
    //TODO: implement proper cleanup
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
