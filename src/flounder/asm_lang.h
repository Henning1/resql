/**
 * @file
 * Definition of x86_64 assembly innstructions.
 * @author Henning Funke <henning.funke@cs.tu-dortmund.de>
 */
#pragma once

#include <stdint.h>
#include <inttypes.h>
#include "ir_base.h"


enum NodeTypes {
    REG8              =  1,
    REG32             =  2,
    REG64             =  3,
    ID_LABEL          =  6,
    LABEL             =  7,
    CONSTANT          =  8,
    BYTE_CONSTANT     =  9,
    SYSCALL           = 10,
    COMMENT_LINE      = 12,
    FUNC              = 13,
    PUSH              = 14,
    POP               = 15,
    DB                = 16,
    INTERRUPT         = 17,
    CALL              = 18,
    DECL_EXTERN       = 19,
    GLOBAL            = 20,
    RESB              = 21,
    RESD              = 22,
    INC               = 23,
    DEC               = 24,
    JE                = 25,
    JMP               = 26,
    MOV               = 27,
    LEA               = 28,
    CMP               = 29,
    ADD               = 30,
    XOR               = 31,
    MEM_AT            = 32,
    BYTE_AT           = 33,
    MEM_ADD           = 34,
    MEM_SUB           = 35,
    PLACE_LABEL       = 36,
    SECTION           = 37,
    BITS64            = 38,
    RET               = 39,
    IMUL              = 40,
    JL                = 41,
    JGE               = 42,
    DIV               = 43,
    SUB               = 44,
    JG                = 45,
    JLE               = 46,
    JNE               = 47,
    AND               = 48,
    OR                = 49,
    IDIV              = 50,
    CDQE              = 51,
    CQO               = 52,
    MOVSX             = 53,
    MOVZX             = 54,
    CONSTANT_ADDRESS  = 55,
    CONSTANT_INT64    = 56,
    CONSTANT_INT32    = 57,
    CONSTANT_INT8     = 58,
    CONSTANT_DOUBLE   = 59,
    MOVSXD            = 60,
    CRC32             = 61
};


/* indicates whether instr reads its p-th parameter */
static bool checkInstrReadAsm ( ir_node* instr, int p ) {
    switch ( instr->nodeType ) {
        case MOV:
        case MOVZX:
        case MOVSX:
        case MOVSXD:
            if ( p == 0 ) return false;         
            if ( p == 1 ) return true;         
            break;
            if ( p == 0 ) return false;         
            if ( p == 1 ) return true;         
            break;
        case CMP:
            if ( p == 0 ) return true;
            if ( p == 1 ) return true;         
            break;
        case ADD:
            if ( p == 0 ) return true;
            if ( p == 1 ) return true;         
            break;
        case SUB:
            if ( p == 0 ) return true;
            if ( p == 1 ) return true;         
            break;
        case IMUL:
            if ( p == 0 ) return true;
            if ( p == 1 ) return true;         
            break;
        case DIV:
            if ( p == 0 ) return true;
            break;
        case IDIV:
            if ( p == 0 ) return true;
            break;
        case INC:
            if ( p == 0 ) return true;
            break;
        case AND:
            if ( p == 0 ) return true;
            if ( p == 1 ) return true;
        case OR:
            if ( p == 0 ) return true;
            if ( p == 1 ) return true;
            break;
        case CRC32:
            if ( p == 0 ) return true;
            if ( p == 1 ) return true;
            break;
        case MEM_AT:
            if ( p == 0)  return true;
            break;
        case MEM_ADD:
            if ( p == 0 ) return true;
            if ( p == 1 ) return true;
            break;
        case MEM_SUB:
            if ( p == 0 ) return true;
            if ( p == 1 ) return true;
            break;
    }
    return false;
}


/* indicates whether instr writes its p-th parameter */
static bool checkInstrWriteAsm ( ir_node* instr, int p ) {
    switch ( instr->nodeType ) {
        case MOV:
        case MOVZX:
        case MOVSX:
        case MOVSXD:
            if ( p == 0 ) return true;         
            break;
        case ADD:
            if ( p == 0 ) return true;
            break;
        case SUB:
            if ( p == 0 ) return true;
            break;
        case IMUL:
            if ( p == 0 ) return true;
            break;
        case INC:
            if ( p == 0 ) return true;
            break;
        case AND:
            if ( p == 0 ) return true;
            break;
        case OR:
            if ( p == 0 ) return true;
            break;
        case CRC32:
            if ( p == 0 ) return true;
            break;
    }
    return false;
}


enum MachineRegName64 {
    RAX = 0,
    RCX = 1,
    RDX = 2,
    RBX = 3,
    RSP = 4,
    RBP = 5,
    RSI = 6,
    RDI = 7,
    R8  = 8,
    R9  = 9,
    R10 = 10, 
    R11 = 11, 
    R12 = 12, 
    R13 = 13, 
    R14 = 14, 
    R15 = 15 
};
    
static char* regNames64[] = { 
    "rax", 
    "rcx", 
    "rdx", 
    "rbx", 
    "rsp", 
    "rbp", 
    "rsi", 
    "rdi",
    "r8" , 
    "r9" , 
    "r10", 
    "r11",
    "r12", 
    "r13", 
    "r14", 
    "r15" 
}; 

static ir_node* reg64 ( enum MachineRegName64 id ) {
    ir_node* res = literal ( regNames64[id], REG64 );
    res->id = id;
    res->nodeType = REG64;
    return res;
}

static ir_node* reg64 ( int id ) {
    return reg64 ( (MachineRegName64) id );
}


enum MachineRegName32 {
    EAX  = 0,
    ECX  = 1,
    EDX  = 2,
    EBX  = 3,
    ESP  = 4,
    EBP  = 5,
    ESI  = 6,
    EDI  = 7,
    R8D  = 8,
    R9D  = 9,
    R10D = 10, 
    R11D = 11, 
    R12D = 12, 
    R13D = 13, 
    R14D = 14, 
    R15D = 15 
};
    
static char* regNames32[] = { 
    "eax", 
    "ecx", 
    "edx", 
    "ebx", 
    "esp", 
    "ebp", 
    "esi", 
    "edi",
    "r8d" , 
    "r9d" , 
    "r10d", 
    "r11d",
    "r12d", 
    "r13d", 
    "r14d", 
    "r15d" 
}; 

static ir_node* reg32 ( enum MachineRegName32 id ) {
    ir_node* res = literal ( regNames32[id], REG32 );
    res->id = id;
    res->nodeType = REG32;
    return res;
}

static ir_node* reg32 ( int id ) {
    return reg32 ( (MachineRegName32) id );
}


enum MachineRegName8 {
    AL   = 0,
    CL   = 1,
    BL   = 2,
    DL   = 3,
    SPL  = 4,
    BPL  = 5,
    SIL  = 6,
    DIL  = 7,
    R8B  = 8,
    R9B  = 9,
    R10B = 10, 
    R11B = 11, 
    R12B = 12, 
    R13B = 13, 
    R14B = 14, 
    R15B = 15 
};
    
static char* regNames8[] = { 
    "al", 
    "cl", 
    "bl", 
    "dl", 
    "spl", 
    "bpl", 
    "sil", 
    "dil",
    "r8b" , 
    "r9b" , 
    "r10b", 
    "r11b",
    "r12b", 
    "r13b", 
    "r14b", 
    "r15b" 
}; 

static ir_node* reg8 ( enum MachineRegName8 id ) {
    ir_node* res = literal ( regNames8[id], REG8 );
    res->id = id;
    res->nodeType = REG8;
    return res;
}

static ir_node* reg8 ( int id ) {
    return reg8 ( (MachineRegName8) id );
}


static bool isRegNodeType ( NodeTypes type ) {
    return ( type == REG8  ||
             type == REG32 ||
             type == REG64 );
}


static size_t regByteSize ( ir_node* const_ ) {

    switch ( const_->nodeType ) {
        case REG8:
            return 1;
        case REG32:
            return 4;
        case REG64:
            return 8;
        default:
            std::cout << "Error in regByteSize(..). Unknown register type." << std::endl; 
            return 0;
    }
}


static size_t constByteSize ( ir_node* const_ ) {

    switch ( const_->nodeType ) {
        case CONSTANT_ADDRESS:
            return 8;
        case CONSTANT_DOUBLE:
            return 8;
        case CONSTANT_INT8:
            return 1;
        case CONSTANT_INT32:
            return 4;
        case CONSTANT_INT64:
            return 8;
        default:
            std::cout << "Error in constByteSize(..). Unknown constant type." << std::endl; 
            return 0;
    }
}


static bool isConstNodeType ( NodeTypes type ) {
    return ( type == CONSTANT_ADDRESS  ||
             type == CONSTANT_INT8     ||
             type == CONSTANT_INT32    ||
             type == CONSTANT_INT64    ||
             type == CONSTANT_DOUBLE );
}


static bool isConst ( ir_node* node ) {
    return isConstNodeType ( (NodeTypes)node->nodeType );
}

static bool isReg ( ir_node* node ) {
    if ( node == nullptr ) return false;
    return ( isRegNodeType ( (NodeTypes)node->nodeType ) );
}


unsigned int labelId = 0;


static ir_node* idLabel ( char* ident ) {
    char* label;
    asprintf ( &label, "%s%i", ident, labelId );
    labelId++;
    return literal ( label, ID_LABEL ); 
}

static ir_node* label ( char* ident ) {
    return literal ( ident, LABEL );
}

static ir_node* bits64 () {
    return literal ( "bits 64\n", 0 );
}



/* address constants */

static char* emitConstAddress ( ir_node* node ) {
    char* strVal;
    asprintf ( &strVal, "0x%" PRIx64, (unsigned long)node->addressData );
    return strVal; 
}


static ir_node* constAddress ( void* val ) {
    ir_node* res = getNode();
    res->emit_fun = &(emitConstAddress);
    res->nodeType = CONSTANT_ADDRESS;
    res->addressData = val;
    return res;
}



/* 8 bit integer constants */

static char* emitConstInt8 ( ir_node* node ) {
    char* strVal;
    asprintf ( &strVal, "%hhi", node->int8Data );
    return strVal; 
}


static ir_node* constInt8 ( int8_t val ) {
    ir_node* res = getNode();
    res->emit_fun = &(emitConstInt8);
    res->nodeType = CONSTANT_INT8;
    res->int8Data = val;
    return res;
}



/* 32 bit integer constants */

static char* emitConstInt32 ( ir_node* node ) {
    char* strVal;
    asprintf ( &strVal, "%i", node->int32Data );
    return strVal; 
}


static ir_node* constInt32 ( int32_t val ) {
    ir_node* res = getNode();
    res->emit_fun = &(emitConstInt32);
    res->nodeType = CONSTANT_INT32;
    res->int32Data = val;
    return res;
}



/* 64 bit integer constants */

static char* emitConstInt64 ( ir_node* node ) {
    char* strVal;
    asprintf ( &strVal, "%" PRId64, node->int64Data );
    return strVal; 
}


static ir_node* constInt64 ( int64_t val ) {
    ir_node* res = getNode();
    res->nodeType = CONSTANT_INT64;
    res->int64Data = val;
    res->emit_fun = &(emitConstInt64);
    return res;
}




/* double constants */

static char* emitConstDouble ( ir_node* node ) {
    char* strVal;
    asprintf ( &strVal, "%lf", node->doubleData );
    return strVal;
}


static ir_node* constDouble ( double val ) {
    ir_node* res = getNode();
    res->emit_fun = &(emitConstDouble);
    res->nodeType = CONSTANT_DOUBLE;
    res->doubleData = val;
    return res;
}


static ir_node* syscall () {
    return literal ( "syscall\n", SYSCALL );
}

static ir_node* ret () {
    return literal ( "ret\n", RET );
}

static ir_node* cdqe () {
    return literal ( "cdqe\n", CDQE );
}

static ir_node* cqo () {
    return literal ( "cqo\n", CQO );
}

static ir_node* commentLine ( char* msg ) {
    char* ln;
    asprintf ( &ln, ";%s\n", msg );
    return literal ( ln, COMMENT_LINE );
}

static ir_node* commentLine ( const char* msg ) {
    char* ln;
    asprintf ( &ln, ";%s\n", msg );
    return literal ( ln, COMMENT_LINE );
}

static ir_node* func ( char* name ) {
    return literal ( name, FUNC );
}
static ir_node* push ( ir_node* preg ) {
    return unaryInstr ( "push", preg, PUSH );
}

static ir_node* pop ( ir_node* preg ) {
    return unaryInstr ( "pop", preg, POP );
}

static ir_node* db ( ir_node* func ) {
    return unaryInstr ( "db", func, DB );
}

static ir_node* interrupt ( ir_node* func ) {
    return unaryInstr ( "int", func, INTERRUPT );
}

static ir_node* call ( ir_node* func ) {
    return unaryInstr ( "call", func, CALL );
}

static ir_node* declExtern ( ir_node* func ) {
    return unaryInstr ( "extern", func, DECL_EXTERN );
}

static ir_node* global ( ir_node* label ) {
    return unaryInstr ( "global", label, GLOBAL );
}

static ir_node* inc ( ir_node* op1 ) {
    return unaryInstr ( "inc", op1, INC );
}

static ir_node* dec ( ir_node* op1 ) {
    return unaryInstr ( "dec", op1, DEC );
}

static ir_node* je ( ir_node* op1 ) {
    return unaryInstr ( "je", op1, JE );
}

static ir_node* jl ( ir_node* op1 ) {
    return unaryInstr ( "jl", op1, JL );
}

static ir_node* jg ( ir_node* op1 ) {
    return unaryInstr ( "jg", op1, JG );
}

static ir_node* jge ( ir_node* op1 ) {
    return unaryInstr ( "jge", op1, JGE );
}

static ir_node* jle ( ir_node* op1 ) {
    return unaryInstr ( "jle", op1, JLE );
}

static ir_node* jne ( ir_node* op1 ) {
    return unaryInstr ( "jne", op1, JNE );
}

static ir_node* jmp ( ir_node* op1 ) {
    return unaryInstr ( "jmp", op1, JMP );
}

static ir_node* mov ( ir_node* op1, ir_node* op2 ) {
    return binaryInstr ( "mov", op1, op2, MOV );
}

static ir_node* movzx ( ir_node* op1, ir_node* op2 ) {
    return binaryInstr ( "movzx", op1, op2, MOVZX );
}

static ir_node* movsx ( ir_node* op1, ir_node* op2 ) {
    return binaryInstr ( "movsx", op1, op2, MOVSX );
}

static ir_node* movsxd ( ir_node* op1, ir_node* op2 ) {
    return binaryInstr ( "movsxd", op1, op2, MOVSXD );
}

static ir_node* lea ( ir_node* op1, ir_node* op2 ) {
    return binaryInstr ( "lea", op1, op2, LEA );
}

static ir_node* cmp ( ir_node* op1, ir_node* op2 ) {
    return binaryInstr ( "cmp", op1, op2, CMP );
}

static ir_node* add ( ir_node* op1, ir_node* op2 ) {
    return binaryInstr ( "add", op1, op2, ADD );
}

static ir_node* sub ( ir_node* op1, ir_node* op2 ) {
    return binaryInstr ( "sub", op1, op2, SUB );
}

static ir_node* imul ( ir_node* op1, ir_node* op2 ) {
    return binaryInstr ( "imul", op1, op2, IMUL );
}

static ir_node* div ( ir_node* op1 ) {
    return unaryInstr ( "div", op1, DIV );
}

static ir_node* idiv ( ir_node* op1 ) {
    return unaryInstr ( "idiv", op1, IDIV );
}

static ir_node* xor_ ( ir_node* op1, ir_node* op2 ) {
    return binaryInstr ( "xor", op1, op2, XOR );
}

static ir_node* and_ ( ir_node* op1, ir_node* op2 ) {
    return binaryInstr ( "and", op1, op2, AND );
}

static ir_node* or_ ( ir_node* op1, ir_node* op2 ) {
    return binaryInstr ( "or", op1, op2, OR );
}

static ir_node* crc32 ( ir_node* op1, ir_node* op2 ) {
    return binaryInstr ( "crc32", op1, op2, CRC32 );
}

static ir_node* memAt ( ir_node* child ) {
    return bracketingNode ( "[", "]", child, MEM_AT ); 
}

static ir_node* byteAt ( ir_node* child ) {
    return bracketingNode ( "byte[", "]", child, MEM_AT ); 
}

static char* emit_memAdd ( ir_node* node ) {
    char* op1 = call_emit ( node->firstChild ); 
    char* op2 = call_emit ( node->firstChild->next ); 
    char* res;
    asprintf ( &res, "%s+%s", op1, op2 );
    free ( op1 ); free ( op2 );
    return res;
}

static ir_node* memAdd ( ir_node* op1, ir_node* op2 ) {
    ir_node* res = getNode();
    res->nodeType = MEM_ADD;
    addChild ( res, op1 );
    addChild ( res, op2 );
    res->emit_fun = &(emit_memAdd);
    return res;
}

static char* emit_memSub ( ir_node* node ) {
    char* op1 = call_emit ( node->firstChild ); 
    char* op2 = call_emit ( node->firstChild->next ); 
    char* res;
    asprintf ( &res, "%s-%s", op1, op2 );
    free ( op1 ); free ( op2 );
    return res;
}

static ir_node* memSub ( ir_node* op1, ir_node* op2 ) {
    ir_node* res = getNode();
    res->nodeType = MEM_SUB;
    addChild ( res, op1 );
    addChild ( res, op2 );
    res->emit_fun = &(emit_memSub);
    return res;
}


static ir_node* placeLabel ( ir_node* label ) {
    char* strVal;
    asprintf ( &strVal, "%s:\n", label->ident );
    return literal ( strVal, SECTION ); 
}


static ir_node* section ( char* secName ) {
    char* strVal;
    asprintf ( &strVal, "section %s\n", secName );
    return literal ( strVal, SECTION ); 
}


static ir_node* memAtSub ( ir_node* op1, ir_node* op2 ) {
    return memAt ( memSub ( op1, op2 ) );
}


static ir_node* memAtAdd ( ir_node* op1, ir_node* op2 ) {
    return memAt ( memAdd ( op1, op2 ) );
}
