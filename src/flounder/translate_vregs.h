/**
 * @file
 * Handle register allocation and spill code generation. 
 * @author Henning Funke <henning.funke@cs.tu-dortmund.de>
 */
#pragma once


#include "flounder.h"
#include "RegisterAllocationState.h"
#include "translate_analyze.h"
#include "limits.h"
#include "stdlib.h"


typedef struct SpillAccessStackEntry {
    /* expression that is pushed for spill code generation, typically vreg */
    ir_node* expr;
    
    /* parent node of expression */
    ir_node* parent;
    
    /* spillId-th spill access in current instruction */
    int      spillId;
    
    /* loc */
    int      spillLocation;
    
    /* paramIdx-th parameter of parent node */
    int      paramIdx;
} SpillAccessStackEntry;


/* keeps track of spill loads and stores per instruction */
typedef struct SpillAccessStack {

    /* number of elements on stack */
    int n;

    /* stack content 
     * max 7 operands for mcall
     * max for other instructions is 3 
     */
    SpillAccessStackEntry content[7];

} SpillAccessStack;


void clearSpillAccessStack ( SpillAccessStack& stack ) {
    stack.n = 0;
}


void markSpill ( SpillAccessStack& stack, 
                 ir_node*          parent, 
                 ir_node*          vreg, 
                 int               alloca, 
                 int               paramIdx ) {

    // each instruction uses at most 3 (7 for calls) spill registers
    assert ( stack.n < 7 );
    SpillAccessStackEntry& e = stack.content [ stack.n ];
    e.parent   =  parent;
    e.expr     =  vreg;
    e.spillId  = -alloca;
    e.paramIdx = paramIdx;
    stack.n++;
}


/* -------------------- VREGS (and constant loads) ---------------------- */
static void replaceOperandsDescend ( ir_node*                 node, 
                                     RegisterAllocationState& state, 
                                     SpillAccessStack&        stack ) {
    
    // traverse children
    ir_node* child = node->firstChild;
    int paramIdx = 0;
    while ( child != NULL ) {

        // child is virtual register
        if ( isVreg ( child ) ) {
            if ( state.allocation [ child->id ] == 0 ) {
                char* errChr = call_emit ( node );
                std::string errorLine ( errChr );
                free ( errChr );
                throw ResqlError ( "access to unallocated vreg in " + errorLine );
            }

            // replace node with machine register
            if ( state.allocation [ child->id ] > 0 ) {
                ir_node* mreg = getAllocatedMachineRegister ( child, state.allocation );
                replaceChild ( node, child, mreg );
            }
            
            // issue spill code
            if ( state.allocation [ child->id ] < 0 ) {
                markSpill ( stack, node, child, state.allocation [ child->id ], paramIdx );
            }

        }
        else if ( child->nodeType == CONST_LOAD ) {
                
            // spill load 64 bit immediates
            if ( constByteSize ( child->firstChild ) > 4 ) {
                markSpill ( stack, node, child, 0, paramIdx );
            }

            // keep 32 bit immediates
            else {
                replaceChild ( node, child, child->firstChild );
            }
        }

        replaceOperandsDescend ( child, state, stack );
        child = child->next; 
        paramIdx++;
    }
}


ir_node* accessSpillSlot ( int spillId ) {
    //ir_node* stackAccess = memAtSub ( reg64 ( RSP ), constInt32 ( spillId*8 ) );
    ir_node* stackAccess = memAtAdd ( reg64 ( RSP ), constInt32 ( spillId*8-8 ) );
    return stackAccess;
}

int numSpillAccess = 0;


ir_node* getSpillLoadReg ( ir_node* expr, int i ) {

    if ( expr->nodeType == VREG8 ) {
        return reg8 ( spillLoadRegs[i] );
    }
    else if ( expr->nodeType == VREG32 ) {
        return reg32 ( spillLoadRegs[i] );
    }
    else if ( expr->nodeType == VREG64 ) {
        return reg64 ( spillLoadRegs[i] );
    }
    else {
        /* loads constants always to 64 bit registers */
        return reg64 ( spillLoadRegs[i] );
    }
}


void emitSpillCode ( ir_node*          base, 
                     ir_node*          instr, 
                     SpillAccessStack& stack ) {

    // iterate spill registers in current instruction
    for ( int i=0; i < stack.n; i++ ) {
       
        SpillAccessStackEntry& e = stack.content[i];
 
        // Prepare code to access spill slot
        //  - stack access for regular spills
        //  - constant literal for CONST_LOAD
        ir_node* spillAccess = accessSpillSlot ( e.spillId );
        if ( e.expr->nodeType == CONST_LOAD ) {
            spillAccess = e.expr->firstChild;
        } 
        
        if ( spillAccess->nodeType == MEM_AT ) {
            if ( canUseMemoryOperand ( e.parent, e.expr ) ) { 
                replaceChild ( e.parent, e.expr, spillAccess );
                continue;
            } 
        }

        // spill loads
        if ( checkInstrRead ( e.parent, e.paramIdx ) ) {
            ir_node* load = mov ( getSpillLoadReg ( e.expr, i ), spillAccess ); 
            insertBeforeChild ( base, instr, load ); 
            if ( e.expr->nodeType != CONST_LOAD ) numSpillAccess++;
        }

        // spill stores
        if ( checkInstrWrite ( e.parent, e.paramIdx ) ) { 
            ir_node* store = mov ( spillAccess, getSpillLoadReg ( e.expr, i ) ); 
            insertAfterChild ( base, instr, store ); 
            numSpillAccess++;
        }

        // replace use of spill register in instructions with spill-dedicated machine register
        replaceChild ( e.parent, e.expr, getSpillLoadReg ( e.expr, i ) );
    }
}


void getAllVregsDescend ( ir_node* node, std::vector < ir_node* >& set ) {
    ir_node* child = node->firstChild;
    while ( child != NULL ) {
        if ( isVreg ( child ) ) {
            set.push_back ( child );
        }
        getAllVregsDescend ( child, set );
        child = child->next;
    }
} 


std::vector < ir_node* > getAllVregs ( ir_node* node ) {
    std::vector < ir_node* > set;
    getAllVregsDescend ( node, set );
    return set;
} 


void allocExplicit ( ir_node*                 baseNode,
                     ir_node*                 line,
                     RegisterAllocationState& state ) {

    ir_node* vreg = line->firstChild;

    if ( line->nodeType == REQ_VREG  ) {
        allocateReg ( vreg, state );
        removeChild ( baseNode, line );
        state.explicitAlloc [ vreg->id ] = true;
        state.allocatedVregs [ vreg->id ] = vreg;
    }
    else if ( line->nodeType == CLEAR_VREG ) {
        M_Assert ( state.explicitAlloc [ vreg->id ] == true, "Cleared vreg was not allocated." );
        freeReg ( vreg, state );
        removeChild ( baseNode, line );
        state.allocatedVregs.erase ( vreg->id );
    }
}


void handleRegisterAllocation ( ir_node*                 baseNode, 
                                ir_node*                 line, 
                                int                      lineNum,
                                RegisterAllocationState& state ) {
 
    if ( printAllocation ) {
        for(int i=0; i<numMregs; i++) {
            printf("%i,", state.mregInUse[i] );
        }
        printf("\n");
    }

    // handle explicit allocation 
    if ( line->nodeType == REQ_VREG || line->nodeType == CLEAR_VREG ) {
        allocExplicit ( baseNode, line, state );
    }
    else {
        // replace vreg operands
        SpillAccessStack stack = { 0, {} };
        replaceOperandsDescend ( line, state, stack );
        emitSpillCode ( baseNode, line, stack );
    }
}
