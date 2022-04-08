/**
 * @file
 * Error handling.
 * @author Henning Funke <henning.funke@cs.tu-dortmund.de>
 */
#pragma once

#include <execinfo.h>
#include <unistd.h>
#include <stdexcept>
#include <exception>



enum QueryErrorType {
    DIVISION_BY_ZERO,
    NOT_IMPLEMENTED,
    ARITHMETIC_OVERFLOW,
    OUT_OF_MEMORY,
    PARSE_ERROR,
    ELEMENT_NOT_FOUND,
    HASH_TABLE_FULL,
    WRONG_TAG,
    INCOMPATIBLE_TYPES,
    CODEGEN_ERROR
};


void query_error ( QueryErrorType err ) {

    std::cerr << "Error: ";

    switch ( err ) {
        case DIVISION_BY_ZERO:
            std::cerr << "Division by zero";
            break;
        case NOT_IMPLEMENTED:
            std::cerr << "Not implemented";
            break;
        case ARITHMETIC_OVERFLOW:
            std::cerr << "Arithmetic overflow";
            break;
        case OUT_OF_MEMORY:
            std::cerr << "Out of memory";
            break;
        case HASH_TABLE_FULL:
            std::cerr << "Hash table full";
            break;
        case ELEMENT_NOT_FOUND:
            std::cerr << "Element not found";
            break;
        case CODEGEN_ERROR:
            std::cerr << "Code generation error";
            break;
        case WRONG_TAG:
            std::cerr << "Wrong tag";
            break;
        case INCOMPATIBLE_TYPES:
            std::cerr << "Incompatible types";
            break;
        default: 
            std::cerr << "Undefined";
            break;
    }

    std::cerr << std::endl;
    exit(0);
}

#define error_msg(err, msg) \
    _error_msg_(err, msg, __FILE__, __LINE__)

void _error_msg_ ( QueryErrorType err, std::string msg, const char* file, int line ) {
    std::cerr << "Error: " << msg << std::endl
              << "Source:\t\t" << file << ", line " << line << "\n\n";

    void *array[10];
    size_t size;
    // get void*'s for all entries on the stack
    size = backtrace(array, 10);
    // print out all the frames to stderr
    backtrace_symbols_fd(array, size, STDERR_FILENO);
    query_error ( err );
}

