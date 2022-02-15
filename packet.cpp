#include "packet.h"
#include "logger.h"
#include "socketconnection_base.h"
#include <string>
#include <sstream>

using namespace std;


Packet::Packet(const PacketType& type_arg, const PacketDataLength& data_length_arg, const char* data_arg)
 : origin(NULL), type(type_arg), data_length(data_length_arg), data(NULL)
{
    DEBUG_REPORT_LOCATION;
    if(data_arg && data_length_arg)
    {
        data = new char[data_length];
        for(size_t i=0; i<data_length; i++)
            data[i] = data_arg[i];
    }
//    if(origin)
//        origin->IncrementPacketsOut();
}


Packet::Packet(SocketConnection_Base* sc_ptr, const PacketType& type_arg, const PacketDataLength& data_length_arg, char* data_arg, bool copy)
 : origin(sc_ptr), type(type_arg), data_length(data_length_arg), data(NULL)
{
    DEBUG_REPORT_LOCATION;
    if(copy && data_arg && data_length_arg)
    {
        data = new char[data_length];
        for(size_t i=0; i<data_length; i++)
            data[i] = data_arg[i];
    }
    else
    {
        data = data_arg;
    }
//    if(origin)
//        origin->IncrementPacketsOut();
}


Packet::~Packet()
{
    DEBUG_REPORT_LOCATION;
/*
    if(origin && origin->DecrementPacketsOut())
    {
        //delete origin; // This is totally wrong.  The packet didn't create the socket connection, so it most certainly shouldn't be deleting it.
    }
*/
    delete [] data;
}


const char* Packet::GetData() const
{
    DEBUG_REPORT_LOCATION;
    return data;
}


void Packet::SetData(const PacketDataLength& data_length_arg, const char* data_arg)
{
    if(data == data_arg)
        return;

    if(data)
        delete [] data;

    data_length = data_length_arg;
    data = new char[data_length_arg];
    for(size_t i=0; i<data_length_arg; i++)
        data[i] = data_arg[i];
}

void Packet::SetType(const PacketType& arg)
{
    type = arg;
}


PacketType Packet::GetType() const
{
    return type;
}


void Packet::SetDataLength(const PacketDataLength& data_length_arg)
{
    data_length = data_length_arg;
}


PacketDataLength Packet::GetDataLength() const
{
    return data_length;
}


string Packet::ToString() const
{
    stringstream ss;
    ss << "Packet - Origin: 0x" << hex << GetOrigin() << dec << "  Type: " << (int)GetType() << "  Length: " << GetDataLength();
    return ss.str();
}


void Packet::DebugString() const
{
    LOG_DEBUG_OUT(ToString());
#ifdef DEBUG
    logger.Hexdump(string(data, GetDataLength()));
#endif
}


bool Packet::Reply(const char* msg)
{
    DEBUG_REPORT_LOCATION;
#if 1
    // disabled
    return false;
#else
     bool ret_val = false;
     if(origin != NULL)
     {
      ret_val = origin->Write(msg);
     }
     return ret_val;
#endif
}


bool Packet::Reply(const PacketType& type_arg, const PacketDataLength& data_length_arg, const char* data_arg)
{
    DEBUG_REPORT_LOCATION;
    bool ret_val = false;
    if(origin != NULL)
    {
        ret_val = origin->Write(type_arg, data_length_arg, data_arg);
    }
    return ret_val;
}


SocketConnection_Base* Packet::GetOrigin() const
{
    return origin;
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
