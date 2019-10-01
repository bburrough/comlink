#ifndef _THREAD_UTILS_H_
#define _THREAD_UTILS_H_

/*
    Discussion of Thread Management

    Thread Shutdown

    When any thread calls pthread_cancel(), it can assume
    that the thread which is cancelled executes *no additional*
    code after pthread_cancel() is called.  The cancelled thread
    is not expected to do any cleanup or anything else.  It
    is expected to just die.

    In various places, we call pthread_join() after calling
    pthread_cancel(), this is perhaps unnecessary, but we are
    doing it in order to ensure clean and orderly shutdown.
    For example, we don't want to cancel a thread, then fall
    completely out to terminate(), where our process is killed
    and the operating system is left to cleanup a thread that
    perhaps has been given a chance to cancel.  This is in
    contrast to the paragraph above, which says that no code
    should be executed after a thread is cancelled, but, just
    the same, we choose to shut down in an orderly fashion
    rather than detonate.


    Catching Exceptions

    It is often necessary to catch exceptions in order to deal
    with problems.  It is typical to implement a default
    exception handler to deal with exception types that were
    not expected.  However, this can be catastrophic in the
    context of pthreads if you do not rethrow the pthread
    cancellation excpetion type (ptw32_exception on Windows,
    abi::__forced_unwind on libstdc++).  Therefore, this
    header provides a CATCHALL macro that does this appropriately
    regardless of platform.

    This macro should be used within the try/catch block
    of a thread that has been launced by pthread_create(), but
    only if you intend to write a catch(...) default exception
    handler.  If you catch exceptions of specific types only,
    this is not necessary.  This is also not necessary if
    you are not within the context of a thread that has not been
    launched by pthread_create().


    Exception Handling Guideline

    When writing a try/catch block, keep the try block reasonably
    tight around the intended functionality.  That way, when an
    exception is thrown, you have a good understanding as to
    what has not occurred successfully.  You should not write
    a try block that is so broad that you don't know what failed.
    Based on this, it is okay to write a default exception handler
    that catches any failure, and continues to run.

    If you tried to apply a different policy, like either not
    writing default exception handlers, or rethrowing unexpected
    exceptions, then exceptions thrown will cause the program
    to terminate.  By trying to manage failures, we are writing
    code that is more robust, and makes a best effort to keep
    the program running.
*/


#if !(defined(WIN32) || defined(__WIN32) || defined(__WIN32__) || defined(FORCE_WIN32))
#include <cxxabi.h>
#else
// this prevents pthread.h on windows from including winsock.h
#include <WinSock2.h>
#endif

#include <pthread.h>

// sanity checks
#if defined(PtW32CatchAll) && defined(_CXXABI_H)
#error pthread exception handlers are defined for both windows and libstdc++
#elif defined(PtW32CatchAll)
#define CATCHALL \
    catch( const ptw32_exception& e ) { throw; } \
    catch( ... )
#elif defined(_CXXABI_H)
#define CATCHALL \
    catch( const abi::__forced_unwind& e ) { throw; } \
    catch( ... )
#else
#define CATCHALL \
	catch( ... )
#endif

#endif //_THREAD_UTILS_H_

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
