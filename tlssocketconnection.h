#ifndef _TLS_SOCKET_CONNECTION_H_
#define _TLS_SOCKET_CONNECTION_H_

#include "socketconnection_base.h"
#include "threadutils.h"
#include <openssl/ssl.h>

using namespace std;

class TLSSocketConnection : public SocketConnection_Base
{
public:
    /*
        TLSSocketConnection

        Incoming packets will be placed in the buffer pointed to by input_buffer_ptr.

        Outgoing packets will be placed in the output buffer then sent at next opportunity.
    */
    TLSSocketConnection(SocketConnectionOwner* owner, PacketPtrSet* input_buffer_ptr);
    virtual ~TLSSocketConnection();

    virtual void Activate();
    virtual void Deactivate();
    virtual void PrepareServerConnection();
    virtual void PrepareClientConnection();
    bool GetActive() const;
    void SetSSLHandle(SSL* ssl);
    bool SSLConnect();

private:
    /*
        ReaderWriter

        Thread responsible for processing both incoming and outgoing data.
    */
    //static void* ReaderWriter(void* void_arg);
    static void* Reader(void* void_arg);
    static void* Writer(void* void_arg);

    SSL* GetSSLHandle() const;

    //pthread_t reader_writer_id;
    pthread_t reader_id;
    pthread_t writer_id;
    pthread_mutex_t ssl_handle_mutex;

    SSL* _sslHandle;
    SSL_CTX* _sslContext;
    
    static SSL_CTX* _server_ssl_context;
    static SSL_CTX* _client_ssl_context;

    bool active;
    bool _client_vs_server_protect;

    static void StaticInit();
    static void StaticDeinit();
};

#endif // _TLS_SOCKET_CONNECTION_H_

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
