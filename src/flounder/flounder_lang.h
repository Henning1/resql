/**
 * @file
 * Main include for Flounder IR.
 * Lightweight intermediate representation for query compilation. 
 * @author Henning Funke <henning.funke@cs.tu-dortmund.de>
 */
#pragma once


#include "asm_lang.h"
#include "asm_lang_simd.h"


enum ExtendedNodeTypes {
    VREG8             = 80,
    VREG32            = 81,
    VREG64            = 82,
    REQ_VREG          = 84,
    CLEAR_VREG        = 85,
    MANAGED_SYSCALL   = 90,
    MANAGED_CALL      = 91,
    CONST_LOAD        = 92,
    OPEN_LOOP         = 93,
    CLOSE_LOOP        = 94
};


/* indicates whether instr reads its p-th parameter */
static bool checkInstrRead ( ir_node* instr, int p ) {
    switch ( instr->nodeType ) {
        case MANAGED_CALL: 
            if ( p < 2 )  return false;
            if ( p >= 2 ) return true;
            break;
        case CONST_LOAD: 
            if ( p == 0 ) return true;
            break;
    }
    return checkInstrReadAsm ( instr, p );
}


/* indicates whether instr writes its p-th parameter */
static bool checkInstrWrite ( ir_node* instr, int p ) {
    switch ( instr->nodeType ) {
        case MANAGED_CALL: 
            if ( p == 0 ) return true;
            break;
    }
    return checkInstrWriteAsm ( instr, p );
}


/* ------------------ VIRTUAL REGISTERS ----------------------  */
static int vRegNum = 0;


static bool isVregNodeType ( ExtendedNodeTypes type ) {
    return ( type == VREG8  ||
             type == VREG32 ||
             type == VREG64 );
}


static bool isVreg ( ir_node* node ) {
    if ( node == nullptr ) return false;
    return ( isVregNodeType ( (ExtendedNodeTypes)node->nodeType ) );
}


static uint8_t getVregByteSize ( ir_node* vregNd ) {

    ExtendedNodeTypes type = (ExtendedNodeTypes) vregNd->nodeType;

    M_Assert ( isVregNodeType ( type ), "Node type no vreg node type." );

    switch ( type ) {
        case VREG8:
            return 1;
        case VREG32:
            return 4;
        case VREG64:
            return 8;
        default:
            // should not happen
            return 0;
    } 
}


static ir_node* vreg ( const char* name, ExtendedNodeTypes type ) {
    char* s;
    int id = vRegNum++;
    //asprintf ( &s, "%s%i(%i)", name, getVregNodeTypeBits ( type ), id );
    asprintf ( &s, "{%s(%i)}", name, id );
    ir_node* res = literal ( s, type );
    res->id = id;
    return res;
}


static ir_node* vreg8 ( const char* name ) {
    return vreg ( name, VREG8 );
}


static ir_node* vreg32 ( const char* name ) {
    return vreg ( name, VREG32 );
}


static ir_node* vreg64 ( const char* name ) {
    return vreg ( name, VREG64 );
}


static ir_node* vreg64cast ( ir_node* otherVreg ) {
    ir_node* res = vreg64 ( otherVreg->ident );
    res->id = otherVreg->id;
    return res;
}


static ir_node* request ( ir_node* vregNode ) {
    ir_node* res = unaryInstr ( "vreg", vregNode, REQ_VREG );
    return res;
}


static ir_node* clear ( ir_node* vregNode ) {
    ir_node* res = unaryInstr ( "clear", vregNode, CLEAR_VREG );
    return res;
}


static ir_node* constLoad ( ir_node* c ) {
    ir_node* res = unarySub ( "constLoad", c, CONST_LOAD );
    return res;
}


/* -------------------- LOOP MARKERS ----------------------- */
static ir_node* openLoop ( int loopId ) {
    char* strVal;
    asprintf ( &strVal, "openLoop%i\n", loopId );
    ir_node* res = literal ( strVal, OPEN_LOOP );
    res->id = loopId;
    return res;
}


static ir_node* closeLoop ( int loopId ) {
    char* strVal;
    asprintf ( &strVal, "closeLoop%i\n", loopId );
    ir_node* res = literal ( strVal, CLOSE_LOOP );
    res->id = loopId;
    return res;
}
 

/* -------------------- FUNCTION CALLS --------------------  */
static bool isManagedCall ( int nodeType ) {
    return ( nodeType == MANAGED_SYSCALL ) 
        || ( nodeType == MANAGED_CALL );
}


static char* emit_mcall ( ir_node* n ) {
    char* res;
    asprintf ( &res, "mcall (" );
    ir_node* chd = n->firstChild;
    while ( chd != NULL ) {
	const char* sep = ( chd->next != NULL ) ? ", " : ""; 
        asprintf ( &res, "%s%s%s", res, call_emit ( chd ), sep );
        chd = chd->next;
    }
    asprintf ( &res, "%s )\n", res );
    return res;
}


static ir_node* mcall ( ir_node* retVal, void* f, ir_node* rax, ir_node* rdi, ir_node* rsi, ir_node* rdx, ir_node* r10, ir_node* r8, ir_node* r9 ) {
    ir_node* res = getNode();
    res->nodeType = MANAGED_CALL;
    res->emit_fun = &(emit_mcall);
    addChild ( res, retVal );
    addChild ( res, constAddress (f) );
    addChild ( res, rax );
    addChild ( res, rdi );
    addChild ( res, rsi );
    addChild ( res, rdx );
    addChild ( res, r10 );
    addChild ( res, r8 );
    addChild ( res, r9 );
    return res;
}


static ir_node* mcall1 ( ir_node* retVal, void* f, ir_node* rax ) {
    return mcall ( retVal, f, rax, NULL, NULL, NULL, NULL, NULL, NULL );
}


static ir_node* mcall2 ( ir_node* retVal, void* f, ir_node* rax, ir_node* rdi ) {
    return mcall ( retVal, f, rax, rdi, NULL, NULL, NULL, NULL, NULL );
}


static ir_node* mcall3 ( ir_node* retVal, void* f, ir_node* rax, ir_node* rdi, ir_node* rsi ) {
    return mcall ( retVal, f, rax, rdi, rsi, NULL, NULL, NULL, NULL );
}


static char* emit_msyscall ( ir_node* n ) {
    char* res;
    asprintf ( &res, "msyscall (" );
    ir_node* chd = n->firstChild;
    while ( chd != NULL ) {
	const char* sep = ( chd->next != NULL ) ? ", " : ""; 
        asprintf ( &res, "%s%s%s", res, call_emit ( chd ), sep );
        chd = chd->next;
    }
    asprintf ( &res, "%s )\n", res );
    return res;
}


static ir_node* msyscall ( ir_node* rax, ir_node* rdi, ir_node* rsi, ir_node* rdx, ir_node* r10, ir_node* r8, ir_node* r9 ) {
    ir_node* res = getNode();
    res->nodeType = MANAGED_SYSCALL;
    res->emit_fun = &(emit_msyscall);
    addChild ( res, rax );
    addChild ( res, rdi );
    addChild ( res, rsi );
    addChild ( res, rdx );
    addChild ( res, r10 );
    addChild ( res, r8 );
    addChild ( res, r9 );
    return res;
}


static ir_node* msyscall2 ( ir_node* rax, ir_node* rdi ) {
    return msyscall ( rax, rdi, NULL, NULL, NULL, NULL, NULL );
}


static ir_node* msyscall4 ( ir_node* rax, ir_node* rdi, ir_node* rsi, ir_node* rdx ) {
    return msyscall ( rax, rdi, rsi, rdx, NULL, NULL, NULL );
}


int numMemoryOperands ( ir_node* instr ) {

    int res = 0;
    ir_node* current = instr->firstChild;
    while ( current != nullptr ) {
        if ( current->nodeType == MEM_AT ) {
            res++;
        }
        current = current->next;
    }
    return res; 
}


/* Indicates whether the operand 'child' of the instruction 'instr' *
 * may be a memory operand. If not 'child' needs to be accessed     *
 * through a temporary register.                                    */
static bool canUseMemoryOperand ( ir_node* instr, 
                                  ir_node* child ) {
    
    /* We can always use memory access as mcall operands. The call *
     * convention translation handles the necessary instructions   */
    if ( isManagedCall ( instr->nodeType ) ) {
        return true;
    }   
            
    int nMemOps = numMemoryOperands ( instr );

    switch ( instr->nodeType ) {

        case MOV: {

            /* When moving a constant to memory, we have to load the *
             * constant into a temporary register first. This is     *
             * because the size of the temporary register defines    *
             * the move size.                                        */

            /* todo: Specify size in instruction and use direct move *
             *       e.g. QWORD PTR [rax*4+0x419260].                */ 
            if ( isConst ( instr->lastChild ) ) {
                return false;
            }

            /* Only one of the mov operands can be a memory location */
            return nMemOps == 0;

        }
    }
    return false;
}
