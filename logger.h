#ifndef _LOGGER_INCLUDE_H_
#define _LOGGER_INCLUDE_H_

#include <iostream>
#include <string>
#include "threadutils.h"

using namespace std;


/*
    
*/
typedef enum
{
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_INFO,
    LOG_LEVEL_ERROR
} LogLevel;

#define ENABLE_DEBUG false
#define ENABLE_INFO true
#define ENABLE_ERROR true

static class Logger
{
public:
    Logger();
    virtual ~Logger();

    //void OutputLocation();

    /*
        The const char* implementation of Hexdump
        should only be used if the msg is known
        to be a valid cstring, properly null terminated.
    */
    void Hexdump(const char* msg);

    /*
        The string implementation of Hexdump will
        output byte arrays containing any values,
        including many null values.
    */
    void Hexdump(const string& str);
    
    void Enable(const LogLevel& level);
    void Disable(const LogLevel& level);

    void Lock();
    void Unlock();

    bool level_debug_enabled;
    bool level_info_enabled;
    bool level_error_enabled;

private:
    static const int width = 16;
    int pthread_cancel_oldtype;

    void SetEnable(const LogLevel& level, bool state);
    void HexumpLine(const char* buf, size_t len, int row_num);

} logger;


#ifdef PTW32_VERSION_STRING
#define THREAD_ID pthread_self().p
#else
#define THREAD_ID pthread_self()
#endif

#if ENABLE_DEBUG
#define LOG_DEBUG_OUT(a) do { logger.Lock(); if(logger.level_debug_enabled) { cerr << hex << "0x" << THREAD_ID << dec << ":"<< __FILE__ << ":" << __LINE__<< ": " << a << endl; } logger.Unlock(); } while(0)
#else
#define LOG_DEBUG_OUT(a) /* a */
#endif
//#define DEBUG_OUT(a) LOG_DEBUG_OUT(a)
#define LOG_REPORT_LOCATION LOG_DEBUG_OUT("")
#define DEBUG_REPORT_LOCATION LOG_REPORT_LOCATION


#if ENABLE_INFO
#define LOG_INFO_OUT(a) do { logger.Lock(); if(logger.level_info_enabled) { cerr << hex << "0x" << THREAD_ID << dec << ":"<< __FILE__ << ":" << __LINE__<< ": " << a << endl; } logger.Unlock(); } while(0)
#else
#define LOG_INFO_OUT(a) /* a */
#endif

#if ENABLE_ERROR
#define LOG_ERROR_OUT(a) do { logger.Lock(); if(logger.level_error_enabled) { cerr << hex << "0x" << THREAD_ID << dec << ":"<< __FILE__ << ":" << __LINE__<< ": " << a << endl; } logger.Unlock(); } while(0)
#else
#define LOG_ERROR_OUT(a) /* a */
#endif

#endif //_LOGGER_INCLUDE_H_

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
