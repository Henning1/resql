/**
 * @file
 * Build call convention code for mcall pseudo-instructions. 
 * @author Henning Funke <henning.funke@cs.tu-dortmund.de>
 */
#pragma once


#include "flounder.h"
#include "x86_abi.h"
#include "RegisterAllocationState.h"


static int callParamRegisterOverwrites = 0;


typedef struct StackSavedRegisters {
    uint8_t   saved[numMregs];
    uint32_t  stackOffset[numMregs];
    int       stackEnd;
} StackSavedRegisters;


static ir_node* saveCallerSaveRegisters ( ir_node* base, 
                                          ir_node* insertPos, 
                                          StackSavedRegisters& callerSave, 
                                          RegisterAllocationState& regState, 
                                          ir_node* retValReg ) 
{
    int stackPos = 0; /* using 0 for positive spill offsets */

    //int stackPos = regState.spillSize;
    for ( int i=0; i<numMregs; i++ ) {                 /* this caused bug for entry = ht_get ( .., entry, .. ) */
        if ( callerSaveMask[i] && regState.mregInUse[i] ) {     //&& i != retValReg->id ) {
            stackPos += 8;
            callerSave.saved[i] = 1;
            callerSave.stackOffset[i] = stackPos;
            insertPos = insertAfterChild ( base, insertPos, mov ( memAtSub ( reg64 ( RSP ), constInt32 ( stackPos ) ), reg64 (i) ) );
        } 
        else {
            callerSave.saved[i] = 0;
        }
    }
    callerSave.stackEnd = stackPos;
    return insertPos;
}
    

static ir_node* restoreCallerSaveRegisters ( ir_node* base, ir_node* insertPos, StackSavedRegisters& callerSave ) {
    for ( int i=0; i<numMregs; i++ ) {
        if ( callerSave.saved[i] ) {
            int stackPos = callerSave.stackOffset[i];
            insertPos = insertAfterChild ( base, insertPos, mov ( reg64 ( i ), memAtSub ( reg64 ( RSP ), constInt32 ( stackPos ) ) ) );
        }
    }
    return insertPos;
}



/* assign values from registers to parameter registers 
 *  - keep track of overwrites in regWriteSet
 */
static ir_node* assignParamReg ( ir_node*             base, 
                                 ir_node*             insertPos, 
                                 ir_node*             param, 
                                 int                  paramIdx,
                                 const uint8_t*       paramOrder,
                                 StackSavedRegisters& callerSave,
                                 int*                 regWriteSet ) {
     
    ir_node* paramReg = reg64 ( paramOrder [ paramIdx ] );
    regWriteSet [ paramReg->id ] = 1;
    if ( param->id == paramReg->id) return insertPos;
    ir_node* source = param;

    // if value was overwritten by call convention
    //  -> substitute source with stack saved value from caller save
    if ( regWriteSet [ param->id ] ) 
    {
        int stackPos = callerSave.stackOffset [ param->id ];
        source = memAtSub ( reg64 ( RSP ), constInt32 ( stackPos ) );
        callParamRegisterOverwrites++;
    }
    insertPos = insertAfterChild ( base, insertPos, scaleMovSx ( paramReg, source ) );
    return insertPos;
}




/* parameter registers will be overwritten -> move them first */
static ir_node* setParameterRegisters ( ir_node*                base, 
                                        ir_node*                insertPos, 
                                        ir_node*                firstParam, 
                                        const uint8_t*          paramOrder,
                                        StackSavedRegisters&    callerSave ) {
    
    int regWriteSet[numMregs] = {0};

    // assign parameter values from registers that are used for parameter passing 
    ir_node* param = firstParam;
    for ( int p=0; param!=NULL; param=param->next, p++ ) {
        if ( isReg ( param ) && isParamRegCall[param->id] ) { 
            insertPos = assignParamReg ( base, insertPos, param, p, paramOrder, callerSave, regWriteSet );
        }
    }

    // assign registers that are not used for parameter passing
    param = firstParam;
    for ( int p=0; param!=NULL; param=param->next, p++ ) {
        if ( isReg ( param ) && !isParamRegCall[param->id] ) { 
            insertPos = assignParamReg ( base, insertPos, param, p, paramOrder, callerSave, regWriteSet );
        } 
    }
    
    // assign other parameters values, e.g.: constants, spilled vregs
    param = firstParam;
    for ( int p=0; param!=NULL; param=param->next, p++ ) {
        if ( ! isReg ( param ) ) { 
            insertPos = insertAfterChild ( base, insertPos, mov ( reg64 ( paramOrder [ p ] ), param ) );
        } 
    }

    return insertPos;
}


ir_node* insertStepIntoFunction ( ir_node*     base,
                                  ir_node*     insertPos,
                                  int          callType,
                                  ir_node*     funcAddress,
                                  int          stackAdjust) {
    // adjust stack frame
    int align = (( stackAdjust+15) / 16) * 16 + 8;
    insertPos = insertAfterChild ( base, insertPos, sub ( reg64 ( RSP ), constInt32 ( align ) ) );

    // do call
    if ( callType == MANAGED_SYSCALL ) {
        insertPos = insertAfterChild ( base, insertPos, syscall() ); 
    }
    else if ( callType == MANAGED_CALL ) {
        insertPos = insertAfterChild ( base, insertPos, mov ( reg64 ( RAX ), funcAddress ) );
        insertPos = insertAfterChild ( base, insertPos, call ( reg64 ( RAX ) ) );
    }

    // restore stack frame
    insertPos = insertAfterChild ( base, insertPos, add ( reg64 ( RSP ), constInt32 ( align ) ) );
    return insertPos;
}


ir_node* insertCallConventionCode ( ir_node*                 base, 
                                    int                      callType, 
                                    ir_node*                 insertPos, 
                                    const uint8_t*           paramOrder, 
                                    RegisterAllocationState& state, 
                                    ir_node*                 funcAddress, 
                                    ir_node*                 retVal, 
                                    ir_node*                 param ) {
    
    /* save caller-save registers, that are in use, on the stack */
    StackSavedRegisters callerSave;
    insertPos = saveCallerSaveRegisters ( base, insertPos, callerSave, state, retVal );
   
    /* assign call parameters via registers (call convention) */ 
    insertPos = setParameterRegisters ( base, insertPos, param, paramOrder, callerSave );

    /* do call (and adjust stackframe) */
    insertPos = insertStepIntoFunction ( base, insertPos, callType, funcAddress, callerSave.stackEnd ); 

    /* restore caller-save registers */ 
    /* todo: remove return value register when not used as parameter */
    insertPos = restoreCallerSaveRegisters ( base, insertPos, callerSave );

    // retrieve return value
    if ( retVal != NULL) {

        ir_node* retReg = nullptr;
        if      ( retVal->nodeType == REG8  ) retReg = reg8  ( AL ); 
        else if ( retVal->nodeType == REG32 ) retReg = reg32 ( EAX ); 
        /* use reg64 as default. This also works when result is stack location *
         * todo: fix result size to support size-adapted stack slots.          */
        else retReg = reg64 ( RAX ); 

        insertPos = insertAfterChild ( base, insertPos, mov ( retVal, retReg ) );
    }
    
    return insertPos;
}


static void placeManagedCall ( ir_node* base, 
                               ir_node* callLine, 
                               RegisterAllocationState& state ) {
    
    assert ( callLine->nChildren > 0 );

    // remove m(sys)call pseudo instruction
    ir_node* insertPos = removeChild ( base, callLine );

    // decode
    const uint8_t* paramOrder;
    ir_node* funcAddress = NULL;
    ir_node* firstParam = NULL;
    ir_node* retVal = NULL;
    if ( callLine->nodeType == MANAGED_SYSCALL ) {
        paramOrder = paramOrderSyscall;
        firstParam = callLine->firstChild;
    }
    else if ( callLine->nodeType == MANAGED_CALL ) {
        paramOrder = paramOrderCall;
        retVal = callLine->firstChild;
        funcAddress = retVal->next;
        firstParam = funcAddress->next;
    }
 
    // insert actual call code
    insertPos = insertAfterChild ( base, insertPos, commentLine ( " func call {" ) );
    insertPos = insertCallConventionCode ( base, callLine->nodeType, insertPos, paramOrder, state, funcAddress, retVal, firstParam );
    insertPos = insertAfterChild ( base, insertPos, commentLine ( " } end call" ) );

}

