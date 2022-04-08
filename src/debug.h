/**
 * @file
 * Used for debugging the query compiler.
 * @author Henning Funke <henning.funke@cs.tu-dortmund.de>
 */
#pragma once
#include "JitContextFlounder.h"


void debugPrintSigned32 ( int32_t v ) {
    std::cout << "si32: " << v << std::endl << std::flush;
}

void debugPrintUnsigned32 ( uint32_t v ) {
    std::cout << "ui32: " << v << std::endl << std::flush;
}

void debugPrintSigned64 ( int64_t v ) {
    std::cout << "si64: " << v << std::endl << std::flush;
}

void debugPrintUnsigned64 ( uint64_t v ) {
    std::cout << "ui64: " << v << std::endl << std::flush;
}

void debugPrintAddress ( void* addr ) {
    std::cout << "addr: " << addr << std::endl << std::flush;
}


void debugVreg ( ir_node* vreg, void* printFunc, JitContextFlounder& ctx ) {
    ir_node* trash = vreg64("trash"); 
    ctx.yield ( request ( trash ) );
    ctx.yield ( mcall1 ( trash, printFunc, vreg ) );
    ctx.yield ( clear ( trash ) );
}

void debugSigned32 ( ir_node* vreg, JitContextFlounder& ctx ) {
    debugVreg ( vreg, (void*)debugPrintSigned32, ctx );
}

void debugUnsigned32 ( ir_node* vreg, JitContextFlounder& ctx ) {
    debugVreg ( vreg, (void*)debugPrintUnsigned32, ctx );
}

void debugSigned64 ( ir_node* vreg, JitContextFlounder& ctx ) {
    debugVreg ( vreg, (void*)debugPrintSigned64, ctx );
}

void debugUnsigned64 ( ir_node* vreg, JitContextFlounder& ctx ) {
    debugVreg ( vreg, (void*)debugPrintUnsigned64, ctx );
}
