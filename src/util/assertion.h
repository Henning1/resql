#pragma once

#include <iostream>
#include <execinfo.h>
#include <unistd.h>

#ifndef NDEBUG
#   define M_Assert(Expr, Msg) \
    __M_Assert(#Expr, Expr, __FILE__, __LINE__, Msg)
#else
#   define M_Assert(Expr, Msg) ;
#endif

static void __M_Assert(const char* expr_str, bool expr, const char* file, int line, std::string msg)
{
    if (!expr)
    {
        std::cerr << "Assert failed:\t" << msg << "\n"
            << "Expected:\t" << expr_str << "\n"
            << "Source:\t\t" << file << ", line " << line << "\n\n";

        void *array[10];
        size_t size;

        // get void*'s for all entries on the stack
        size = backtrace(array, 10);

        // print out all the frames to stderr
        backtrace_symbols_fd(array, size, STDERR_FILENO);
        exit(1);
    }
}
