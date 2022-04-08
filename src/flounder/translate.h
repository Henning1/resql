/**
 * @file
 * Main include file for translation of Flounder IR to machine assembly. 
 * @author Henning Funke <henning.funke@cs.tu-dortmund.de>
 */
#pragma once


#include "flounder.h"
#include "x86_abi.h"
#include "translate_vregs.h"    
#include "translate_call.h"    
#include "translate_optimize.h"    
#include "translate_analyze.h"    


int ceilToMultipleOf ( int val, int multipleOf ) {
    return ( ( ( val + (multipleOf - 1) ) / multipleOf ) * multipleOf ); 
}


void addCalleeSave ( ir_node* baseNode, RegisterAllocationState& state ) {

    int stackSize = state.numSpillSlots * 8;
    stackSize = ceilToMultipleOf ( stackSize, 16 );

    ir_node* save = irRoot();
    addChild ( save, bits64() );
    addChild ( save, push ( reg64 ( RBP ) ) );
    addChild ( save, push ( reg64 ( RBX ) ) );
    addChild ( save, push ( reg64 ( R12 ) ) );
    addChild ( save, push ( reg64 ( R13 ) ) );
    addChild ( save, push ( reg64 ( R14 ) ) );
    addChild ( save, push ( reg64 ( R15 ) ) );
    addChild ( save, sub  ( reg64 ( RSP ), constInt32 ( stackSize ) ) );
    transferNodes ( baseNode, nullptr, save );

    ir_node* restore = irRoot();
    addChild ( restore, add  ( reg64 ( RSP ), constInt32 ( stackSize ) ) );
    addChild ( restore, pop ( reg64 ( R15 ) ) );
    addChild ( restore, pop ( reg64 ( R14 ) ) );
    addChild ( restore, pop ( reg64 ( R13 ) ) );
    addChild ( restore, pop ( reg64 ( R12 ) ) );
    addChild ( restore, pop ( reg64 ( RBX ) ) );
    addChild ( restore, pop ( reg64 ( RBP ) ) );
    transferNodes ( baseNode, baseNode->lastChild->prev, restore );
}
        
        

static void translationPass ( ir_node* baseNode ) {

    RegisterAllocationState state ( vRegNum );
    int lineNum=0;
    ir_node* line = baseNode->firstChild;
    while ( line != NULL ) {
    
        // save next instruction to allow deletion and to skip inserts
        ir_node* next = line->next;

        // register allocation
        handleRegisterAllocation ( baseNode, line, lineNum, state );

        // function calls 
        if ( isManagedCall ( line->nodeType ) ) {
            placeManagedCall ( baseNode, line, state );
        }

        // remove loop marker-instructions
        if ( line->nodeType == OPEN_LOOP || line->nodeType == CLOSE_LOOP ) {
            removeChild ( baseNode, line );
        }

        line = next;
        lineNum++;
    }

    if ( state.allocatedVregs.size() > 0 ) {
        std::cout << std::endl << "WARNING!! The following virtual register are never deallocated." << std::endl;
        for ( auto e : state.allocatedVregs ) {
            std::cout << call_emit ( e.second ) << std::endl;
        }
        std::cout << std::endl;
    }
    
    addCalleeSave ( baseNode, state );
} 



void printFormattedFlounder ( char*          code,
                              bool           indent = true,
                              std::ostream&  stream = std::cout ) {

    std::istringstream iss(code);
    std::string prefix="";
    int lineNumber = 1;
    for ( std::string line; std::getline(iss, line); lineNumber++ ) {
        if ( line.rfind ( "closeLoop", 0 ) == 0 && indent ) {
            prefix = prefix.substr ( 4 );
        }
        stream << prefix << line << std::endl;
        if ( line.rfind ( "openLoop", 0 ) == 0 && indent ) {
            prefix = prefix + "    ";
        }
    }

}



static void translateFlounderToMachineIR ( ir_node*       codeTree,
                                           std::ostream&  stream = std::cout,
                                           bool           optimizeFlounder=false,
                                           bool           printFlounderIr=false,
                                           bool           printAssembly=false ) {
   
    if ( printFlounderIr ) {
        char* code;
        code = call_emit ( codeTree ); 
        stream <<  "--------------------- FLOUNDER IR ---------------------" << std::endl;
        printFormattedFlounder ( code, true, stream );
        free ( code );
    }
    if ( optimizeFlounder ) {
        stream << "optimizing..." << std::endl;
        optimize ( codeTree );
        if ( printFlounderIr ) {
            char* code;
            code = call_emit ( codeTree ); 
            stream << "----------------- OPTIMIZED FLOUNDER IR -----------------" << std::endl;
            printFormattedFlounder ( code, true, stream );
            free ( code );
        }
    }

    numSpillAccess = 0;
    translationPass ( codeTree );

    if ( printAssembly ) {
        char* code;
        code = call_emit ( codeTree ); 
        stream << "----------------- MACHINE ASSEMBLY X64 --------------------" << std::endl;
        int lineNumber = 1;
        std::istringstream iss(code);
        for ( std::string line; std::getline(iss, line); lineNumber++ ) {
            std::cout << line << std::endl;
        }
    }
}

