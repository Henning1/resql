/**
 * @file
 * Methods to emit instructions for certain constructs to simplify code generation.
 * @author Henning Funke <henning.funke@cs.tu-dortmund.de>
 */
#pragma once

#include "flounder_lang.h"



ir_node* scaleMovSx ( ir_node* op1, ir_node* op2 ) {

    if ( isReg ( op1 ) && isReg ( op2 ) ) {
        if ( regByteSize ( op1 ) > regByteSize ( op2 ) ) {
            return movsx ( op1, op2 );
        } 
    }
    return mov ( op1, op2 );
}


typedef struct BinaryComparator {
    void (*gen_func)( ir_node* root, struct BinaryComparator cmp, ir_node* jumpDest );  
    void (*gen_func_inv)( ir_node* root, struct BinaryComparator cmp, ir_node* jumpDest );  
    ir_node* a;
    ir_node* b;
} BinaryComparator;


static void genFuncIsEqual ( ir_node* root, BinaryComparator binaryCompare, ir_node* jumpDest ) {
    addChild ( root, cmp ( binaryCompare.a, binaryCompare.b ) );
    addChild ( root, je ( jumpDest ) );   
}


static void genFuncIsEqualInv ( ir_node* root, BinaryComparator binaryCompare, ir_node* jumpDest ) {
    addChild ( root, cmp ( binaryCompare.a, binaryCompare.b ) );
    addChild ( root, jne ( jumpDest ) );   
}


static void genFuncIsSmaller ( ir_node* root, BinaryComparator binaryCompare, ir_node* jumpDest ) {
    addChild ( root, cmp ( binaryCompare.a, binaryCompare.b ) );
    addChild ( root, jl ( jumpDest ) );   
}


static void genFuncIsLarger ( ir_node* root, BinaryComparator binaryCompare, ir_node* jumpDest ) {
    addChild ( root, cmp ( binaryCompare.a, binaryCompare.b ) );
    addChild ( root, jg ( jumpDest ) );   
}


static void genFuncIsLargerInv ( ir_node* root, BinaryComparator binaryCompare, ir_node* jumpDest ) {
    addChild ( root, cmp ( binaryCompare.a, binaryCompare.b ) );
    addChild ( root, jle ( jumpDest ) );   
}


static void genFuncIsSmallerInv ( ir_node* root, BinaryComparator binaryCompare, ir_node* jumpDest ) {
    addChild ( root, cmp ( binaryCompare.a, binaryCompare.b ) );
    addChild ( root, jge ( jumpDest ) );   
}


static BinaryComparator isSmaller ( ir_node* a, ir_node* b ) {
    return (BinaryComparator) { &genFuncIsSmaller, &genFuncIsSmallerInv, a, b };
}

static BinaryComparator isLarger ( ir_node* a, ir_node* b ) {
    return (BinaryComparator) { &genFuncIsLarger, &genFuncIsLargerInv, a, b };
}


static BinaryComparator isLargerEqual ( ir_node* a, ir_node* b ) {
    return (BinaryComparator) { &genFuncIsSmallerInv, &genFuncIsSmaller, a, b };
}


static BinaryComparator isEqual ( ir_node* a, ir_node* b ) {
    return (BinaryComparator) { &genFuncIsEqual, &genFuncIsEqualInv, a, b };
}


static BinaryComparator isNotEqual ( ir_node* a, ir_node* b ) {
    return (BinaryComparator) { &genFuncIsEqualInv, &genFuncIsEqual, a, b };
}


static int loopId = 0;


typedef struct WhileLoop {
    int id; 
    ir_node* root;
    ir_node* headLabel;
    ir_node* footLabel;
} WhileLoop;


static WhileLoop While ( BinaryComparator condition, ir_node* root ) {
    char* strHead;
    char* strFoot;
    asprintf ( &strHead, "loop_head%i", loopId );
    asprintf ( &strFoot, "loop_foot%i", loopId );

    WhileLoop loop = (WhileLoop) { 
        loopId++, 
        root, 
        label ( strHead ), 
        label ( strFoot ) 
    };

    addChild ( root, openLoop ( loop.id ) );
    addChild ( root, placeLabel ( loop.headLabel ) );
    condition.gen_func_inv ( root, condition, loop.footLabel );

    free ( strHead ); free ( strFoot );
    return loop;
}


static void closeWhile ( WhileLoop loop ) {
    addChild ( loop.root, jmp ( loop.headLabel ) );
    addChild ( loop.root, placeLabel ( loop.footLabel ) );
    addChild ( loop.root, closeLoop ( loop.id ) );
}


static WhileLoop WhileTrue ( ir_node* root ) {
    char* strHead;
    char* strFoot;
    asprintf ( &strHead, "loop_head%i", loopId );
    asprintf ( &strFoot, "loop_foot%i", loopId );

    WhileLoop loop = (WhileLoop) { 
        loopId++, 
        root, 
        label ( strHead ), 
        label ( strFoot ) 
    };

    addChild ( root, openLoop ( loop.id ) );
    addChild ( root, placeLabel ( loop.headLabel ) );

    free ( strHead ); free ( strFoot );
    return loop;
}


static void breakWhile ( WhileLoop loop, BinaryComparator condition ) {
    condition.gen_func ( loop.root, condition, loop.footLabel );
}

static void continueWhile ( WhileLoop loop, BinaryComparator condition ) {
    condition.gen_func ( loop.root, condition, loop.headLabel );
}


static int ifId = 0;


typedef struct IfClause {
    int id; 
    ir_node* root;
    ir_node* footLabel;
} IfClause;


static IfClause If ( BinaryComparator condition, ir_node* root ) {
    char* strFoot;
    asprintf ( &strFoot, "if_foot%i", ifId );

    IfClause ifclause = (IfClause) { 
        ifId++, 
        root, 
        label ( strFoot ) 
    };

    condition.gen_func_inv ( root, condition, ifclause.footLabel );

    free ( strFoot );
    return ifclause;
}


static void closeIf ( IfClause loop ) {
    addChild ( loop.root, placeLabel ( loop.footLabel ) );
}
