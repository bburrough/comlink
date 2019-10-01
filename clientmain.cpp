#include "clientsocket.h"
#include "threadutils.h"
#include <iostream>
#include <cstring>


using namespace std;

int main()
{
    int err = 0;
    try
    {
        ClientSocket my_sock;
        
        if (!my_sock.Connect("127.0.0.1", 7257))
        {
            cerr << "Failed my_sock.Connect()" << endl;
            return 1;
        }

        const char buf[] = "This is the string that was transmitted.";

        int i(0);
        for(i=0;i<500;i++)
        {
            const Packet pkt(Packet::DATA_LOG_MESSAGE, strlen(buf), buf);

            if(my_sock.Write(pkt))
            {
                /*
                cout << "Wrote packet. ";
                pkt.DebugString();
#if !(defined(WIN32) || defined(__WIN32) || defined(__WIN32__) || defined(FORCE_WIN32))
                usleep(250000);
#else
                Sleep(250);
#endif
                */
            }
            else
            {
                cout << "Packet write failed." << endl;
                break;
            }
        }
        //block until all packets have been written out instead of sleep(1) above
#if !(defined(WIN32) || defined(__WIN32) || defined(__WIN32__) || defined(FORCE_WIN32))
        sleep(3);
#else
        Sleep(3000);
#endif
    }
    catch (const char* arg)
    {
        cout << "Exception: " << arg << endl;
        return 1;
    }
    catch ( ... )
    {
        cerr << "An unrecoverable error occurred." << endl;
        return 1;
    }
    return 0;
}


Packet* NewPacket(SocketConnection_Base* sc_arg, const PacketType& type_arg, const PacketDataLength& data_length_arg, char* data_arg, bool copy)
{
    try
    {
        return new Packet(sc_arg, type_arg, data_length_arg, data_arg, copy);
    }
    catch (const char* e)
    {
        cerr << "Error: " << e << endl;
    }
    CATCHALL
    {
        cerr << "An unexpected error occurred." << endl;
    }
    return NULL;
}


void Delete(Packet* pkt)
{
    delete pkt;
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
