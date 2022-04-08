#pragma once


#include <iostream>
#include <string>


class ResqlError {
public:

    /* maybe reactivate in the future */
    /*
    enum Type {
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
    */


    std::string msg;
    std::string file;
    int line;
    std::string trace;


    ResqlError ( const std::string& msg ) 
        : msg(msg) {

        /* get backtrace */    
        void *array[10];
        size_t size;
        char **strings;
        size = backtrace(array, 10);
        strings = backtrace_symbols(array, size);
        std::stringstream ss;
        for ( size_t i=0; i<size; i++ ) {
            ss << strings[i] << std::endl;
        }
        free(strings);
        trace = ss.str();
    };


    ~ResqlError() {}

    std::string message() const {
        std::stringstream ss;
        ss << msg << std::endl;

        /* add backtrace if in debug build */
        #ifndef NDEBUG
        ss << "Backtrace: " << std::endl;
        ss << trace;
        #endif

        return ss.str();
    }
};









