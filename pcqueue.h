#ifndef _PCQUEUE_H
#define _PCQUEUE_H

#include <errno.h>
#include "cl_semaphore.h"
#include "safelist.h"
#include "logger.h"

/**
  *@author Bob Burrough
  */

template<class T>
class PCQueue
{
public:
    PCQueue();
    ~PCQueue();

    bool Producer(const T& event_arg );
    T Consumer();
    T TryConsumer();
    T pop_front();

private:
    void Lock();
    void Unlock();

    SafeList<T> event_set;

    sem_t   prod_sem;
    sem_t   cons_sem;
};

template<class T>
PCQueue<T>::PCQueue()
{
    DEBUG_REPORT_LOCATION;

    int err = sem_init(&prod_sem, 0, 10000);
    if(err)
    {
        LOG_ERROR_OUT("Failed to init semaphore.");
        throw("Failed to init semaphore");
    }
    err = sem_init(&cons_sem, 0, 0);
    if(err)
    {
        LOG_ERROR_OUT("Failed to init semaphore.");
        throw("Failed to init semaphore");
    }
}

template<class T>
PCQueue<T>::~PCQueue()
{
    DEBUG_REPORT_LOCATION;
    sem_destroy( &prod_sem );
    sem_destroy( &cons_sem );
}

template<class T>
bool PCQueue<T>::Producer(const T& event_arg)
{
    DEBUG_REPORT_LOCATION;
    sem_wait( &prod_sem );
    event_set.push_back(event_arg);
    sem_post( &cons_sem );
    return true;
}

template<class T>
T PCQueue<T>::Consumer()
{
    DEBUG_REPORT_LOCATION;
    sem_wait( &cons_sem );
    T event = event_set.pop_front();
    sem_post( &prod_sem );
    return event;
}

template<class T>
T PCQueue<T>::TryConsumer()
{
    DEBUG_REPORT_LOCATION;
    if(0 != sem_trywait( &cons_sem ))
        return (T)NULL;
    T event = event_set.pop_front();
    sem_post( &prod_sem );
    return event;
}


/*
    This will remove an object from the buffer and
    return it, without modifying the producer/consumer state.

    !!THIS INVALIDATES THE CURRENT PRODUCER/CONSUMER STATE!!

    If the buffer is empty, NULL will be returned.
*/
template<class T>
T PCQueue<T>::pop_front()
{
    DEBUG_REPORT_LOCATION;

    return event_set.pop_front();
}

#endif

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
