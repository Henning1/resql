/**
 * @file
 * Represent the allocation state of virtual registers to machine registers.
 * @author Henning Funke <henning.funke@cs.tu-dortmund.de>
 */
#pragma once

#include <cassert>
#include <queue>

#include "x86_abi.h"


const bool printAllocation = false;


struct RegisterAllocationState {


    RegisterAllocationState ( int numVregs ) : numVregs(numVregs) {
        allocation = (int*) malloc ( sizeof ( int ) * numVregs );
        currentlyAllocated = (bool*) malloc ( sizeof ( bool ) * numVregs );
        explicitAlloc = (bool*) malloc ( sizeof ( bool ) * numVregs );
        for ( int i=0; i<numMregs; i++) mregInUse[i] = 0;
        for ( int i=0; i<vRegNum; i++) allocation[i] = 0;
        for ( int i=0; i<vRegNum; i++) currentlyAllocated[i] = false;
    }

    
    ~RegisterAllocationState() {
        free ( explicitAlloc );
        free ( allocation );
        free ( currentlyAllocated );
    }

    // total number of vregs
    int numVregs;

    // Indicates the current allocation state of virtual registers.
    //
    //    allocation[i] == 0: 
    //     - vreg i unallocated
    //
    //    allocation[i] > 0: 
    //     - vreg i allocated to machine register 'allocation[i]-1' 
    //
    //    allocation[i] < 0: 
    //     - vreg i spilled to slot '-allocation[i]'
    //
    int*     allocation;
    

    bool*    currentlyAllocated;


    // Keep track of allocation mode for each virtual register.
    //
    //    'explicitAlloc[i]' = false  (default)
    //     -  indicates vreg i is allocated implicitly via first and last use
    //
    //    'explicitAlloc[i]' = true
    //     -  indicates vreg i is allocated explicitly via 'vreg x' and 'clear x',
    //
    bool*    explicitAlloc = 0;
    

    // Number of machine registers in use for virtual register allocation
    uint8_t  numMregsUsed = 0;


    // Boolean flag that indicates machine register use
    uint8_t  mregInUse[numMregs];


    // Number of spilled virtual registers 
    //
    //   - Currently spill slots are not reassigned.
    //     Doing so could improve stack access locality.
    //
    int numSpillSlots = 0;
    

    size_t spillSize = 0;


    // To be deallocated after instruction
    std::vector < ir_node* > vregsToDealloc;


    std::map < int, ir_node* > allocatedVregs;
    

    std::queue < int > freeSpillSlots;

};


static ir_node* getAllocatedMachineRegister ( ir_node* vregNode, 
                                              int*     allocation ) {
    uint8_t id = allocation [ vregNode->id ] - 1;
    assert ( isVreg ( vregNode ) );
    if ( vregNode->nodeType == VREG8 ) {
        return reg8 ( id );
    }
    else if ( vregNode->nodeType == VREG32 ) {
        return reg32 ( id );
    }
    else if ( vregNode->nodeType == VREG64 ) {
        return reg64 ( id );
    }

    return nullptr;
}


static int getFreeMregId ( RegisterAllocationState& state ) {
   
    // prioritize callee-save registers 
    for ( int i=0; i<numMregs; i++ ) {
        if ( !state.mregInUse[i] && allocationMregs[i] && !callerSaveMask[i] ) {
            return i;
        }
    }
    // then use caller-save
    for ( int i=0; i<numMregs; i++ ) {
        if ( !state.mregInUse[i] && allocationMregs[i] ) {
            return i;
        }
    }
    assert (0);
    return -1;
}




void allocateReg ( ir_node*                 vregNd, 
                   RegisterAllocationState& state ) {

    // use machine register if available
    if ( state.numMregsUsed < numAllocationMregs ) {
        state.numMregsUsed++;
        int mregId = getFreeMregId ( state );
        state.allocation [ vregNd->id ] = mregId+1;
        state.currentlyAllocated [ vregNd->id ] = true;
        state.mregInUse [ mregId ] = 1;
        if ( printAllocation ) {
            printf ( " - ! - allocated %s to %i\n", vregNd->ident, mregId );
        }
    }

    // spill register
    else {
        // reuse spill slot
        if ( state.freeSpillSlots.size() > 0 ) {
            state.allocation [ vregNd->id ] = state.freeSpillSlots.front();
            state.freeSpillSlots.pop();
        }
        // new spill slot
        else {
            state.numSpillSlots++;
            state.allocation [ vregNd->id ] = -state.numSpillSlots;
        }
        state.currentlyAllocated [ vregNd->id ] = true;
        if ( printAllocation ) {
            printf ( "- ! - spill %s to [rsp-%i]\n", vregNd->ident, state.allocation [ vregNd->id ]*(-8) ); 
        }
        state.spillSize += getVregByteSize ( vregNd );
    }
}


void freeReg ( ir_node*                 vregNd, 
               RegisterAllocationState& state ) {

    int allocVal = state.allocation [ vregNd->id ];
    if ( allocVal > 0 ) {
        state.mregInUse [ allocVal-1 ] = 0; 
        state.numMregsUsed--;
        if ( printAllocation ) {
            printf ( " - ! - cleared %s from %i\n", vregNd->ident, allocVal-1 );
        }
    }
    if ( allocVal < 0 ) {
        state.freeSpillSlots.push ( allocVal );
    }
    state.allocation [ vregNd->id ] = 0;
    state.currentlyAllocated[ vregNd->id ] = false;
}


