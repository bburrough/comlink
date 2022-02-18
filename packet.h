#ifndef _PACKET_H_
#define _PACKET_H_

#include <cstdint>
#include <stdlib.h>
#include <string>

class SocketConnection_Base;

typedef uint8_t PacketType;
typedef uint32_t PacketDataLength;

class Packet
{
public:
    /*
        We have application level data being mixed with protocol level data.  These need
        to be separated.  Application level data should be implemented in classes which
        inherit from the Packet classes.  Then those objects can be transmitted by
        SocketConnection's.
    */
    enum Types
    {
        DATA_LOG_MESSAGE = 0,
        DATA_CONNECTION_REQUESTED,
        DATA_CONNECTION_ACCEPTED,
        DATA_CONNECTION_REFUSED,
        ERR,
        ERR_INTERRUPTED,
        ERR_DISCONNECTED,
        BASE_TYPES_END
    };

    //Packet(SocketConnection* sc_ptr, const char* data_arg); // prevent raw data from being transmitted
    Packet(SocketConnection_Base* sc_ptr, const PacketType& type_arg, const PacketDataLength& data_length_arg, char* data_arg, bool copy = true);
    Packet(const PacketType& type_arg, const PacketDataLength& data_length_arg, const char* data_arg);
    virtual ~Packet();

    /*
        Retrieve the data associated with this packet.
    */
    const char* GetData() const;

    void SetData(const PacketDataLength& data_length_arg, const char* data_arg);

    /*
        Used for replying to the socket that originated this packet.

        Will return true upon success or false upon failure.  Will fail
        if the socket has been disconnected and/or its SocketConnection
        was deleted.
    */
    bool Reply(const char* msg);
    bool Reply(const PacketType& type_arg, const PacketDataLength& data_length_arg, const char* data);

    /* Add a callback to be notified when this object is deleted. */
    //void PushDestructionCallback(const Callback& cb);

    SocketConnection_Base* GetOrigin() const;

    void SetType(const PacketType& arg);
    PacketType GetType() const;

    void SetDataLength(const PacketDataLength& data_length_arg);
    PacketDataLength GetDataLength() const;

    void DebugString() const;
    std::string ToString() const;

private:
    /*
        Problem: when someone uses the origin pointer
        to make a Write (or other) call into a SocketConnection,
        they have no way to check whether the SocketConnection is
        valid.  Packets will definitely exist that point to
        SocketConnections that have been removed.  So, we need
        some way of checking to see whether the SocketConnection
        is valid before calling orign->Write("hi");
    */
    SocketConnection_Base* origin;

    // the packet data
    PacketType type;
    PacketDataLength data_length;
    char* data;
};


// Service Functions - to be implemented by server/client implementor
Packet* NewPacket(SocketConnection_Base* sc_arg, const PacketType& type_arg, const PacketDataLength& data_length_arg, char* data_arg, bool copy = true);
void Delete(Packet* pkt);

#endif // _PACKET_H_

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
