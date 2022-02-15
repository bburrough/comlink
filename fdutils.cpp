
#include "fdutils.h"

#if !(defined(WIN32) || defined(__WIN32) || defined(__WIN32__) || defined(FORCE_WIN32))
#include <unistd.h>
#include <fcntl.h>
#endif


bool operator==(const timeval& x, const timeval& y)
{
//#if !(defined(WIN32) || defined(__WIN32) || defined(__WIN32__) || defined(FORCE_WIN32))
//    return (x.tv_sec == y.tv_sec && x.tv_nsec == y.tv_nsec);
//#else
    return (x.tv_sec == y.tv_sec && x.tv_usec == y.tv_usec);
//#endif
}


int WaitUntilReadableWithTimeval(int file_descriptor, timeval* t)
{
    fd_set sc_fd_set;
    FD_ZERO(&sc_fd_set);
    FD_SET(file_descriptor, &sc_fd_set);

    return select(FD_SETSIZE, &sc_fd_set, NULL, NULL, t);
}


int WaitUntilReadable(int file_descriptor)
{
    return WaitUntilReadableWithTimeval(file_descriptor, NULL);
}


int WaitUntilWritableWithTimeval(int file_descriptor, timeval* t)
{
    fd_set sc_fd_set;
    FD_ZERO(&sc_fd_set);
    FD_SET(file_descriptor, &sc_fd_set);

    return select(FD_SETSIZE, NULL, &sc_fd_set, NULL, t);
}


int WaitUntilWritable(int file_descriptor)
{
    return WaitUntilWritableWithTimeval(file_descriptor, NULL);
}


int WaitUntilReadableOrWritableWithTimeval(int file_descriptor, timeval* t)
{
    fd_set sc_fd_set;
    FD_ZERO(&sc_fd_set);
    FD_SET(file_descriptor, &sc_fd_set);

    return select(FD_SETSIZE, &sc_fd_set, &sc_fd_set, NULL, t);
}


int WaitUntilReadableOrWritable(int file_descriptor)
{
    return WaitUntilReadableOrWritableWithTimeval(file_descriptor, NULL);
}


void _timevalMilliseconds(timeval* ts, uint32_t milliseconds)
{
    ts->tv_sec = milliseconds / 1000;
    ts->tv_usec = (milliseconds % 1000) * 1000;
}


int WaitUntilWritableOrTimeout(int file_descriptor, uint32_t milliseconds)
{
    timeval t;
    _timevalMilliseconds(&t, milliseconds);
    return WaitUntilWritableWithTimeval(file_descriptor, &t);
}


int WaitUntilReadableOrTimeout(int file_descriptor, uint32_t milliseconds)
{
    timeval t;
    _timevalMilliseconds(&t, milliseconds);
    return WaitUntilReadableWithTimeval(file_descriptor, &t);
}


int WaitUntilReadableOrWritableOrTimeout(int file_descriptor, uint32_t milliseconds)
{
    timeval t;
    _timevalMilliseconds(&t, milliseconds);
    return WaitUntilReadableOrWritableWithTimeval(file_descriptor, &t);
}


void CloseDescriptor(int file_descriptor)
{
#if !(defined(WIN32) || defined(__WIN32) || defined(__WIN32__) || defined(FORCE_WIN32))
    close(file_descriptor);
#else
    closesocket(file_descriptor);
#endif
}


bool SetNonBlockingMode(int file_descriptor)
{
#if !(defined(WIN32) || defined(__WIN32) || defined(__WIN32__) || defined(FORCE_WIN32))
    int flags = fcntl(file_descriptor, F_GETFL, 0);
    if(-1 == fcntl(file_descriptor, F_SETFL, flags | O_NONBLOCK))
    {
        return false;
    }
#else
    u_long yes = 1;
    if (0 != ioctlsocket(file_descriptor, FIONBIO, &yes))
    {
        return false;
    }
#endif
    return true;
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
