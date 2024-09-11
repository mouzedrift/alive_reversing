#pragma once

#ifdef WIN32

    #include <windows.h>
    #pragma warning(push)
    #pragma warning(disable : 4091) // typedef ': ignored on left of '' when no variable is declared in SDK header
    #include <Dbghelp.h>
    #pragma warning(pop)
    #include <tchar.h>
    #include <stdio.h>
    #include "SDL.h"
    #include "relive_config.h"
    #include "Sys_common.hpp"

    #ifndef __MINGW32__

        #pragma comment(lib, "dbghelp.lib")

inline void create_minidump(PEXCEPTION_POINTERS /* apExceptionInfo*/)
{
    char_type errMsg[1024] = {};
    _snprintf(errMsg, _countof(errMsg), "R.E.L.I.V.E. has crashed");
    Alive_Show_ErrorMsg(errMsg);
}

    #else

inline void create_minidump(PEXCEPTION_POINTERS /* apExceptionInfo */)
{
    char_type errMsg[1024] = {};
    _snprintf(errMsg, _countof(errMsg), "R.E.L.I.V.E. has crashed, could not write dump file");
    Alive_Show_ErrorMsg(errMsg);
}

    #endif

inline LONG WINAPI crash_handler(PEXCEPTION_POINTERS pExceptionInfo)
{
    create_minidump(pExceptionInfo);
    return EXCEPTION_CONTINUE_SEARCH;
}

inline void Install_Crash_Handler()
{
    ::SetUnhandledExceptionFilter(crash_handler);
}

#else // Assume Linux/mac

    #include <stdio.h>
    #include <execinfo.h>
    #include <signal.h>
    #include <stdlib.h>
    #include <unistd.h>


inline void crash_handler(int sig)
{
    void* callStack[30];
    size_t size;

    // get void*'s for all entries on the stack
    size = backtrace(callStack, 30);

    // print out all the frames to stderr
    fprintf(stderr, "Error: signal %d:\n", sig);
    backtrace_symbols_fd(callStack, size, STDERR_FILENO);
    exit(1);
}

inline void Install_Crash_Handler()
{
    signal(SIGSEGV, crash_handler);
}

#endif
