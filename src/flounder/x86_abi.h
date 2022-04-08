/**
 * @file
 * Define x86 (and respective Flounder) characteristics that are used by the compiler. 
 * @author Henning Funke <henning.funke@cs.tu-dortmund.de>
 */
#pragma once

/* parameter order 
 * syscall: rax, rdi, rsi, rdx, r10, r8, r9 
 * call:    rdi, rsi, rdx, rcx, r8,  r9
 */


/*                 |    0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
 * names           |   rax  rcx  rdx  rbx  rsp  rbp  rsi  rdi  r8   r9  r10  r11  t12  r13  r14  r15
 *---------------------------------------------------------------------------------------------------
 * div             |   rax       rdx 
 * return value    |   rax
 * stack pointer   |                       rsp 
 * spill loads     |   rax, rcx, rdx
 * vreg alloc (12) |                  rbx,      rbp, rsi, rdi, r8,  r9, r10, r11, r12, r13, r14, r15
 * syscall param:  |   rax,      rdx,                rsi, rdi, r8,  r9, r10 
 * call param:     |        rcx, rdx,                rsi, rdi, r8,  r9
 * callee-save:    |                  rbx, rsp, rbp,                              r12, r13, r14, r15
 * caller-save:    |   rax, rcx, rdx,                rsi, rdi, r8,  r9, r10, r11
 */


/* x86 ABI */
const uint8_t numMregs                          = 16;
                                                 /* 0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 */
const uint8_t callerSaveMask[numMregs]          = { 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0 };
const uint8_t calleeSaveMask[numMregs]          = { 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1 };
const uint8_t isParamRegSyscall [ numMregs ]    = { 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0 };
const uint8_t isParamRegCall [ numMregs ]       = { 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0 };

const uint8_t paramOrderSyscall[7]              = { 0, 7, 6, 2, 10, 8,   9 };
const uint8_t paramOrderCall[7]                 = { 7, 6, 2, 1,  8, 9, 255 };

/* Flounder IR translation */
const uint8_t numAllocationMregs                = 12;
const uint8_t spillLoadRegs[3]                  = { 0, 1, 2 };
const uint8_t allocationMregs [ numMregs ]      = { 0, 0, 0, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };
                                                 /* 0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 */

