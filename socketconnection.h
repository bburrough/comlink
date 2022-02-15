#ifndef _SOCKET_CONNECTION_H_
#define _SOCKET_CONNECTION_H_

#include "packet.h"
#include "cl_semaphore.h"
#include "pcqueue.h"
#include "socketconnectionowner.h"
#include "socketconnection_base.h"
#include <string>

using namespace std;


class SocketConnection : public SocketConnection_Base
{
public:
    /*
        SocketConnection

        Incoming packets will be placed in the buffer pointed to by input_buffer_ptr.

        Outgoing packets will be placed in the output buffer then sent at next opportunity.
    */
    SocketConnection(SocketConnectionOwner* owner, PacketPtrSet* input_buffer_ptr);

    virtual ~SocketConnection();


    /*
        Activate

        Used to initialize the threads used for reading to and
        writing from the socket.
    */
    void Activate();

    virtual void Deactivate();

    virtual void PrepareServerConnection() {}
    virtual void PrepareClientConnection() {}

    void IncrementPacketsOut();

    bool DecrementPacketsOut();

    bool GetActive() const;

private:
    /*
        Reader

        Thread responsible for listening for incoming packets.
        Blocks based on select of descriptor and if maximum input_buffer
        size is met.
    */
    static void* Reader(void* void_arg);

    /* Thread responsible for sending packets from output_buffer at next opportunity.  Blocks if nothing is in output_buffer and select of descriptor. */
    static void* Writer(void* void_arg);

    pthread_t reader_id;
    pthread_t writer_id;
    bool active;
};


// Service Functions - to be implemented by server/client implementor
SocketConnection* NewSocketConnection(SocketConnectionOwner* owner, int fd_arg, PacketPtrSet* ppsp_arg);
void Delete(SocketConnection* sc_arg);

#endif // _SOCKET_CONNECTION_H_

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
