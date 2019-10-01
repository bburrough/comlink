#include "logger.h"


#include <iomanip>
#include <cstring>
#include <errno.h>

using namespace std;

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

Logger::Logger()
    : pthread_cancel_oldtype(0), level_debug_enabled(ENABLE_DEBUG), level_info_enabled(ENABLE_INFO), level_error_enabled(ENABLE_ERROR)
{}


Logger::~Logger()
{}


/*
void Logger::OutputLocation()
{
    Lock();

    Unlock();
}
*/


void Logger::HexumpLine(const char* buf, size_t len, int row_num)
{
    cout.fill('0');
    for(int i=row_num*width; i < row_num*width+width; i++)
    {
        if(i >= len)
            break;

        cout << setw(2) << hex << (int)(buf[i] & 0x000000FF) << " ";
    }

    cout << dec;
    for(int i=row_num*width; i < row_num*width+width; i++)
    {
        if(i >= len)
            break;

        cout << (isprint((unsigned char)buf[i]) ? buf[i] : '.') << " "; //TODO: windows is passing non-ascii characters to this
    }

    cout << endl;
}


void Logger::Hexdump(const string& str)
{
    Lock();
    cout.fill('0');
    for(int i=0; i<=str.length()/width; i++)
    {
        HexumpLine(str.c_str(), str.length(), i);
    }
    cout << endl << dec;
    Unlock();
}


void Logger::Hexdump(const char* msg)
{
    Hexdump(string(msg));
}


void Logger::Lock()
{
    pthread_setcanceltype(PTHREAD_CANCEL_DISABLE,&pthread_cancel_oldtype);
    pthread_mutex_lock(&mutex);
}


void Logger::Unlock()
{
    pthread_mutex_unlock(&mutex);
    int temp;
    pthread_setcanceltype(pthread_cancel_oldtype,&temp);
}


void Logger::SetEnable(const LogLevel& level, bool state)
{
    switch (level)
    {
    case LOG_LEVEL_DEBUG:
        level_debug_enabled = state;
        break;
    case LOG_LEVEL_INFO:
        level_info_enabled = state;
        break;
    case LOG_LEVEL_ERROR:
        level_error_enabled = state;
        break;
    }
}


void Logger::Enable(const LogLevel& level)
{
    SetEnable(level, true);
}


void Logger::Disable(const LogLevel& level)
{
    SetEnable(level, false);
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
