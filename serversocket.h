#ifndef _SERVER_SOCKET_H_
#define _SERVER_SOCKET_H_

#include "safelist.h"
#include "pcqueue.h"
#include "socketconnectionowner.h"
#include "threadutils.h"
#include <string>

using namespace std;

class SocketConnection_Base;
class Packet;

class ServerSocket : public SocketConnectionOwner
{
public:
    /*
        ServerSocket

        ip_address is a cstring representation of the IP address on which to listen
        for incoming connections.

        port is an integer representation of the port on which to listen for incoming connections.
    */
    ServerSocket(const string& ip_address, int port);
    virtual ~ServerSocket();

    /*
        NewPacket

        NewPacket picks up the first packet in packet_set and returns it.

        If there are no packets in the packet_set, Read will block until one is
        inserted into the packet_set.

        When finished with the packet, you must call DeletePacket().
    */
    Packet* NewPacket();

    void DeletePacket(Packet* pkt);

    bool WriteAll(const Packet* pkt);
    bool WriteAllExceptOrigin(const Packet* pkt);

    void Run() const;

    void DeleteSocketConnection(SocketConnection_Base* sc_ptr);

private:
    /*
        AcceptThread

        AcceptThread will be spawned as a thread upon construction of a ServerSocket.

        AcceptThread is responsible for listening for incoming connections, then inserting
        the connection into the connection_set upon connection.
    */
    static void* AcceptThread(void* void_arg);

    static void* HealthMonitor(void* arg);

    void RemoveSocketConnection(SocketConnection_Base* sc_ptr);

    //data
    SafeList<SocketConnection_Base*> connection_set;
    PCQueue<Packet*> packet_set;
    int server_descriptor;
    pthread_t _accept_thread_id;
    pthread_t _health_monitor_thread_id;

    // disable this
    bool WriteAll(char* cstring_arg);
};

#endif //_SERVER_SOCKET_H_

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
