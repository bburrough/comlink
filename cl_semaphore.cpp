/*
 *  cl_semaphore.c
 *  comlink
 *
 *  Created by Bob Burrough on 2/12/08.
 *  Copyright 2008 Bob Burrough. All rights reserved.
 *
 */

#include "cl_semaphore.h"

#ifdef __APPLE__
int sem_init(sem_t *sem, int pshared, int value) {
    int ret;
    if(pshared)
    {
        printf("sem_init with pshared set");
        exit(1);
    }
    sem->value = value;

    ret = pthread_mutex_init(&sem->mutex,NULL);
    if(ret < 0) return -1;
    ret = pthread_cond_init(&sem->cond,NULL);
    if(ret < 0) return -1;
    return 0;
}


int sem_post(sem_t *sem)
{
    int retval = 0;
    int oldtype = 0;
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &oldtype);
    do
    {
        if(pthread_mutex_lock(&sem->mutex) < 0)
        {
            retval = -1;
            break;
        }

        sem->value++;

        if(pthread_cond_signal(&sem->cond) < 0) {
            pthread_mutex_unlock(&sem->mutex);
            retval = -1;
            break;
        }

        if(pthread_mutex_unlock(&sem->mutex) < 0)
        {
            retval = -1;
            break;
        }
    } while(0);

    int temp = 0;
    pthread_setcancelstate(oldtype, &temp);
    return retval;
}

void cleanup(void* arg)
{
    sem_t* sem = (sem_t*)arg;
    pthread_mutex_unlock(&sem->mutex);
}

int sem_wait(sem_t *sem)
{
    int retval = 0;
    int oldtype = 0;
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &oldtype);
    pthread_testcancel();
    do
    {
        if(pthread_mutex_lock(&sem->mutex) < 0)
        {
            retval = -1;
            break;
        }

        while(sem->value == 0) {
            pthread_cleanup_push(cleanup, sem);
            pthread_cond_wait(&sem->cond,&sem->mutex);
            pthread_cleanup_pop(0);
        }
        sem->value--;

        if(pthread_mutex_unlock(&sem->mutex) < 0)
        {
            retval = -1;
            break;
        }

    } while(0);

    int temp = 0;
    pthread_setcancelstate(oldtype, &temp);
    return retval;
}

int sem_trywait(sem_t *sem)
{
    int retval = 0;
    int oldtype = 0;
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &oldtype);
    pthread_testcancel();
    do
    {
        if(pthread_mutex_lock(&sem->mutex) < 0)
        {
            retval = -1;
            break;
        }
        
        if(sem->value == 0)
        {
            retval = -1;
        }
        else
        {
            sem->value--;
        }
        
        if(pthread_mutex_unlock(&sem->mutex) < 0)
        {
            retval = -1;
            break;
        }
        
    } while(0);
    
    int temp = 0;
    pthread_setcancelstate(oldtype, &temp);
    return retval;
}

int sem_destroy(sem_t *sem) {
    int ret;
    ret = pthread_cond_destroy(&sem->cond);
    if(ret < 0) return -1;
    ret = pthread_mutex_destroy(&sem->mutex);
    if(ret < 0) return -1;
    return 0;
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
