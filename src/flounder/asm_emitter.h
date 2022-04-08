/**
 * @file
 * Emit assembly representation as binary. 
 * @author Jan Mühlig <jan.muehlig@cs.tu-dortmund.de>
 * @author Henning Funke <henning.funke@cs.tu-dortmund.de>
 */
#include <unordered_map>
#include <algorithm>
#include <cassert>
#include "asmjit/asmjit.h"
#include "asm_lang.h"


typedef void (*Func)(void);


class Emitter
{
private:
    asmjit::JitRuntime _runtime;
    asmjit::CodeHolder _code;
    std::unordered_map<std::string, asmjit::Label> _labels;

public:
    Emitter() {
        this->_code.init(this->_runtime.codeInfo());
    }


    ~Emitter() = default;


    int emit ( ir_node *root ) {
        asmjit::x86::Assembler asm_container(&this->_code);
        ir_node *current_node = root;
        int c=0;
        while(current_node != nullptr) {

            //printf ( "current_node: %s", call_emit(current_node) );

            c++;
            if(current_node->nodeType == BaseNodeTypes::ROOT) {
                current_node = current_node->firstChild;
                continue;
            } else if(current_node->nodeType == BaseNodeTypes::UNDEFINED) {
                current_node = current_node->next; // TODO: Should this happen?
                continue;
            } else if(current_node->nodeType == NodeTypes::PUSH) {
                assert(current_node->nChildren == 1 && "PUSH has != 1 children");
                assert( isReg ( current_node->firstChild ) && "PUSH has other type than REG");
                asm_container.push(this->interpret_register(current_node->firstChild));
            } else if(current_node->nodeType == NodeTypes::POP) {
                assert(current_node->nChildren == 1 && "POP has != 1 children");
                assert( isReg ( current_node->firstChild ) && "POP has other type than REG");
                asm_container.pop(this->interpret_register(current_node->firstChild));
            } else if(current_node->nodeType == NodeTypes::RET) {
                asm_container.ret();
            } else if(current_node->nodeType == NodeTypes::MOV) {
                assert(current_node->nChildren == 2 && "MOV has != 2 children");
                if( isReg ( current_node->firstChild ) ) {
                    auto second_child = current_node->lastChild;
                    if ( isConst ( second_child ) ) {
                        asm_container.mov(
                                this->interpret_register(current_node->firstChild),
                                this->interpret_constant(current_node->lastChild)
                        );
                    } else if (second_child->nodeType == NodeTypes::MEM_AT) {
                        asm_container.mov(
                                this->interpret_register(current_node->firstChild),
                                this->interpret_mem(second_child)
                        );
                    } else if ( isReg ( second_child ) ) {
                        asm_container.mov(
                            this->interpret_register(current_node->firstChild),
                            this->interpret_register(second_child)
                        );
                    } else {
                        std::cout << "TODO: MOV REG, " << second_child->nodeType << std::endl;
                        std::cout << "MOV " << current_node->firstChild->ident << " " << second_child->ident
                                  << std::endl;
                    }
                } else if(current_node->firstChild->nodeType == NodeTypes::MEM_AT) {
                    if( isReg ( current_node->lastChild ) ) {
                        asm_container.mov(
                                this->interpret_mem(current_node->firstChild),
                                this->interpret_register(current_node->lastChild)
                        );
                    } else {
                        std::cout << "TODO: MOV MEM_AT " << current_node->lastChild->nodeType << std::endl;
                    }
                } else {
                    std::cout << "TODO: MOV to " << current_node->firstChild->nodeType << std::endl;
                }
            } else if(current_node->nodeType == NodeTypes::MOVZX) {
                assert(current_node->nChildren == 2 && "MOVZX has != 2 children");
                if( isReg ( current_node->firstChild ) ) {
                    auto second_child = current_node->lastChild;
                    if (second_child->nodeType == NodeTypes::MEM_AT) {
                        asm_container.movzx(
                                this->interpret_register(current_node->firstChild),
                                this->interpret_mem(second_child)
                        );
                    } else if ( isReg ( second_child ) ) {
                        asm_container.movzx(
                            this->interpret_register(current_node->firstChild),
                            this->interpret_register(second_child)
                        );
                    } else {
                        std::cout << "TODO: MOVZX REG, " << second_child->nodeType << std::endl;
                        std::cout << "MOVZX " << current_node->firstChild->ident << " " << second_child->ident
                                  << std::endl;
                    }
                }
                else {
                    std::cout << "MOVZX: first operand must be reg." << std::endl;
                }
            } else if(current_node->nodeType == NodeTypes::MOVSX) {
                assert(current_node->nChildren == 2 && "MOVSX has != 2 children");
                if( isReg ( current_node->firstChild ) ) {
                    auto second_child = current_node->lastChild;
                    if (second_child->nodeType == NodeTypes::MEM_AT) {
                        asm_container.movsx(
                                this->interpret_register(current_node->firstChild),
                                this->interpret_mem(second_child)
                        );
                    } else if ( isReg ( second_child ) ) {
                        asm_container.movsx(
                            this->interpret_register(current_node->firstChild),
                            this->interpret_register(second_child)
                        );
                    } else {
                        std::cout << "TODO: MOVSX REG, " << second_child->nodeType << std::endl;
                        std::cout << "MOVSX " << current_node->firstChild->ident << " " << second_child->ident
                                  << std::endl;
                    }
                }
                else {
                    std::cout << "MOVSX: first operand must be reg." << std::endl;
                }
            } else if(current_node->nodeType == NodeTypes::MOVSXD) {
                assert(current_node->nChildren == 2 && "MOVSXD has != 2 children");
                if( isReg ( current_node->firstChild ) ) {
                    auto second_child = current_node->lastChild;
                    if (second_child->nodeType == NodeTypes::MEM_AT) {
                        asm_container.movsxd(
                                this->interpret_register(current_node->firstChild),
                                this->interpret_mem(second_child)
                        );
                    } else if ( isReg ( second_child ) ) {
                        asm_container.movsxd(
                            this->interpret_register(current_node->firstChild),
                            this->interpret_register(second_child)
                        );
                    } else {
                        std::cout << "TODO: MOVSXD REG, " << second_child->nodeType << std::endl;
                        std::cout << "MOVSXD " << current_node->firstChild->ident << " " << second_child->ident
                                  << std::endl;
                    }
                }
                else {
                    std::cout << "MOVSXD: first operand must be reg." << std::endl;
                }
            } else if(current_node->nodeType == NodeTypes::CMP) {
                assert(current_node->nChildren == 2 && "CMP has != 2 children");
 
                ir_node* op1 = current_node->firstChild;
                ir_node* op2 = current_node->lastChild;
 
                if ( isReg ( op1 ) && isConst ( op2 ) ) {
                    asm_container.cmp(this->interpret_register(op1), this->interpret_constant(op2));
                }
                else if ( isReg ( op1 ) && op2->nodeType == MEM_AT ) {
                    asm_container.cmp(this->interpret_register(op1), this->interpret_mem(op2));
                }
                else if ( op1->nodeType == MEM_AT && isReg ( op2 ) ) {
                    asm_container.cmp(this->interpret_mem(op1), this->interpret_register(op2));
                }
                else if ( op1->nodeType == MEM_AT && isConst ( op2 ) ) {
                    asm_container.cmp(this->interpret_mem(op1), this->interpret_constant(op2));
                } 
                else if ( isReg ( op1 ) && isReg ( op2 ) ) {
                    asm_container.cmp(this->interpret_register(op1), this->interpret_register(op2));
                } 
                else {
                    assert(false && "CMP [2] is not REG or CONSTANT");
                }
    
            } 
            else if(current_node->nodeType == NodeTypes::COMMENT_LINE) {
                c--;c++; /* do nothing */
            } else if(current_node->nodeType == NodeTypes::JGE) { c--;
                assert(current_node->nChildren == 1 && "JGE has != 1 children");
                asm_container.jge(this->label(asm_container, current_node->firstChild));
            } else if(current_node->nodeType == NodeTypes::JG) { c--;
                assert(current_node->nChildren == 1 && "JG has != 1 children");
                asm_container.jg(this->label(asm_container, current_node->firstChild));
            } else if(current_node->nodeType == NodeTypes::JLE) {
                c--;
                assert(current_node->nChildren == 1 && "JLE has != 1 children");
                asm_container.jle(this->label(asm_container, current_node->firstChild));
            } else if(current_node->nodeType == NodeTypes::JL) { c--;
                assert(current_node->nChildren == 1 && "JL has != 1 children");
                asm_container.jl(this->label(asm_container, current_node->firstChild));
            } else if(current_node->nodeType == NodeTypes::SECTION) {
                assert(current_node->nChildren == 0 && "SECTION has children");
                asm_container.bind(this->label(asm_container, current_node));
            } else if(current_node->nodeType == NodeTypes::ADD) {
                assert(current_node->nChildren == 2 && "ADD has != 2 children");
                assert( isReg ( current_node->firstChild ) && "ADD [1] is not a REG");
                if( isConst ( current_node->lastChild ) ) {
                    asm_container.add(
                        this->interpret_register(current_node->firstChild),
                        this->interpret_constant(current_node->lastChild)
                    );
                } else if( isReg ( current_node->lastChild ) ) {
                    asm_container.add(
                        this->interpret_register(current_node->firstChild),
                        this->interpret_register(current_node->lastChild)
                    );
                } else {
                    assert(false && "ADD [2] is not a REG or CONSTANT");
                }
            } else if(current_node->nodeType == NodeTypes::SUB) {
                assert(current_node->nChildren == 2 && "SUB has != 2 children");
                assert( isReg ( current_node->firstChild ) && "SUB [1] is not a REG");
                if( isConst ( current_node->lastChild ) ) {
                    asm_container.sub(
                        this->interpret_register(current_node->firstChild),
                        this->interpret_constant(current_node->lastChild)
                    );
                }
                else if( isReg ( current_node->lastChild ) ) {
                    asm_container.sub(
                        this->interpret_register(current_node->firstChild),
                        this->interpret_register(current_node->lastChild)
                    );
                }
            } else if(current_node->nodeType == NodeTypes::IMUL) {
                assert(current_node->nChildren == 2 && "IMUL has != 2 children");
                assert( isReg ( current_node->firstChild ) && "IMUL [1] is not a REG");
                if( isConst ( current_node->lastChild ) ) {
                    asm_container.imul(
                        this->interpret_register(current_node->firstChild),
                        this->interpret_constant(current_node->lastChild)
                    );
                }
                else if( isReg ( current_node->lastChild ) ) {
                    asm_container.imul(
                        this->interpret_register(current_node->firstChild),
                        this->interpret_register(current_node->lastChild)
                    );
                }
            } else if(current_node->nodeType == NodeTypes::XOR) {
                assert(current_node->nChildren == 2 && "XOR has != 2 children");
                assert( isReg ( current_node->firstChild ) && "XOR [1] is not a REG");
                assert( isReg ( current_node->lastChild ) && "XOR [2] is not a REG");
                asm_container.xor_(
                    this->interpret_register(current_node->firstChild),
                    this->interpret_register(current_node->lastChild)
                );
            } else if(current_node->nodeType == NodeTypes::AND) {
                assert(current_node->nChildren == 2 && "AND has != 2 children");
                assert( isReg ( current_node->firstChild ) && "AND [1] is not a REG");
                assert( isReg ( current_node->lastChild ) && "AND [2] is not a REG");
                asm_container.and_ (
                    this->interpret_register(current_node->firstChild),
                    this->interpret_register(current_node->lastChild)
                );
            } else if(current_node->nodeType == NodeTypes::OR) {
                assert(current_node->nChildren == 2 && "OR has != 2 children");
                assert( isReg ( current_node->firstChild ) && "OR [1] is not a REG");
                assert( isReg ( current_node->lastChild ) && "OR [2] is not a REG");
                asm_container.or_ (
                    this->interpret_register(current_node->firstChild),
                    this->interpret_register(current_node->lastChild)
                );
            } else if(current_node->nodeType == NodeTypes::CRC32) {
                assert(current_node->nChildren == 2 && "CRC32 has != 2 children");
                assert( isReg ( current_node->firstChild ) && "CRC32 [1] is not a REG");
                assert( isReg ( current_node->lastChild ) && "CRC32 [2] is not a REG");
                asm_container.crc32 (
                    this->interpret_register(current_node->firstChild),
                    this->interpret_register(current_node->lastChild)
                );
            } else if(current_node->nodeType == NodeTypes::DIV) {
                assert(current_node->nChildren == 1 && "DIV has != 1 children");
                assert( isReg ( current_node->firstChild ) && "DIV [1] is not a REG");
                asm_container.div(
                    this->interpret_register(current_node->firstChild)
                );
            } else if(current_node->nodeType == NodeTypes::IDIV) {
                assert(current_node->nChildren == 1 && "IDIV has != 1 children");
                assert( isReg ( current_node->firstChild ) && "IDIV [1] is not a REG");
                asm_container.idiv(
                    this->interpret_register(current_node->firstChild)
                );
            } else if(current_node->nodeType == NodeTypes::INC) {
                assert(current_node->nChildren == 1 && "INC has != 1 children");
                asm_container.inc(this->interpret_register(current_node->firstChild));
            } else if(current_node->nodeType == NodeTypes::JMP) {
                assert(current_node->nChildren == 1 && "JMP has != 1 children");
                asm_container.jmp(this->label(asm_container, current_node->firstChild));
            } else if(current_node->nodeType == NodeTypes::JE) {
                assert(current_node->nChildren == 1 && "JE has != 1 children");
                asm_container.je(this->label(asm_container, current_node->firstChild));
            } else if(current_node->nodeType == NodeTypes::CDQE) {
                assert(current_node->nChildren == 0 && "CDQE has != 0 children");
                asm_container.cdqe();
            } else if(current_node->nodeType == NodeTypes::CQO) {
                assert(current_node->nChildren == 0 && "CQO has != 0 children");
                asm_container.cqo();
            } else if(current_node->nodeType == NodeTypes::JNE) {
                assert(current_node->nChildren == 1 && "JNE has != 1 children");
                asm_container.jne(this->label(asm_container, current_node->firstChild));
            } else if(current_node->nodeType == NodeTypes::CALL) {
                assert(current_node->nChildren == 1 && "CALL has != 1 children");
                assert( isReg ( current_node->firstChild ) && "CALL has not REG");
                asm_container.call(this->interpret_register(current_node->firstChild));
            /* aligned simd move */
            } else if ( current_node->nodeType == SimdNodeTypes::VMOVDQA ) {
                assert(current_node->nChildren == 2 && "VMOVDQA has != 2 children");
                ir_node* fst  = current_node->firstChild;
                ir_node* scd = current_node->lastChild;
                if ( fst->nodeType == SimdNodeTypes::YMM && scd->nodeType == SimdNodeTypes::YMM ) {
                    asm_container.vmovdqa ( this->interpret_register_avx ( fst ), this->interpret_register_avx ( scd ) );
                }
                if ( fst->nodeType == SimdNodeTypes::YMM && scd->nodeType == NodeTypes::MEM_AT ) {
                    asm_container.vmovdqa ( this->interpret_register_avx ( fst ), this->interpret_mem ( scd ) );
                }
                if ( fst->nodeType == NodeTypes::MEM_AT && scd->nodeType == SimdNodeTypes::YMM ) {
                    asm_container.vmovdqa ( this->interpret_mem ( fst ), this->interpret_register_avx ( scd ) );
                }
            } else if ( current_node->nodeType == SimdNodeTypes::VMOVDQA32 ) {
                assert(current_node->nChildren == 2 && "VMOVDQA32 has != 2 children");
                ir_node* fst  = current_node->firstChild;
                ir_node* scd = current_node->lastChild;
                if ( fst->nodeType == SimdNodeTypes::ZMM && scd->nodeType == SimdNodeTypes::ZMM ) {
                    asm_container.vmovdqa32 ( this->interpret_register_avx512 ( fst ), this->interpret_register_avx512 ( scd ) );
                }
                if ( fst->nodeType == SimdNodeTypes::ZMM && scd->nodeType == NodeTypes::MEM_AT ) {
                    asm_container.vmovdqa32 ( this->interpret_register_avx512 ( fst ), this->interpret_mem ( scd ) );
                }
                if ( fst->nodeType == NodeTypes::MEM_AT && scd->nodeType == SimdNodeTypes::ZMM ) {
                    asm_container.vmovdqa32 ( this->interpret_mem ( fst ), this->interpret_register_avx512 ( scd ) );
                }
            } else if ( current_node->nodeType == SimdNodeTypes::MOVDQA ) {
                assert(current_node->nChildren == 2 && "MOVQA has != 2 children");
                ir_node* fst  = current_node->firstChild;
                ir_node* scd = current_node->lastChild;
                if ( fst->nodeType == SimdNodeTypes::XMM && scd->nodeType == SimdNodeTypes::XMM ) {
                    asm_container.movdqa ( this->interpret_register_sse ( fst ), this->interpret_register_sse ( scd ) );
                }
                if ( fst->nodeType == SimdNodeTypes::XMM && scd->nodeType == NodeTypes::MEM_AT ) {
                    asm_container.movdqa ( this->interpret_register_sse ( fst ), this->interpret_mem ( scd ) );
                }
                if ( fst->nodeType == NodeTypes::MEM_AT && scd->nodeType == SimdNodeTypes::XMM ) {
                    asm_container.movdqa ( this->interpret_mem ( fst ), this->interpret_register_sse ( scd ) );
                }
            /* unaligned simd move */
            } else if ( current_node->nodeType == SimdNodeTypes::VMOVDQU ) {
                assert(current_node->nChildren == 2 && "VMOVDQU has != 2 children");
                ir_node* fst  = current_node->firstChild;
                ir_node* scd = current_node->lastChild;
                if ( fst->nodeType == SimdNodeTypes::YMM && scd->nodeType == SimdNodeTypes::YMM ) {
                    asm_container.vmovdqu ( this->interpret_register_avx ( fst ), this->interpret_register_avx ( scd ) );
                }
                if ( fst->nodeType == SimdNodeTypes::YMM && scd->nodeType == NodeTypes::MEM_AT ) {
                    asm_container.vmovdqu ( this->interpret_register_avx ( fst ), this->interpret_mem ( scd ) );
                }
                if ( fst->nodeType == NodeTypes::MEM_AT && scd->nodeType == SimdNodeTypes::YMM ) {
                    asm_container.vmovdqu ( this->interpret_mem ( fst ), this->interpret_register_avx ( scd ) );
                }
            } else if ( current_node->nodeType == SimdNodeTypes::VMOVDQU32 ) {
                assert(current_node->nChildren == 2 && "VMOVDQU32 has != 2 children");
                ir_node* fst  = current_node->firstChild;
                ir_node* scd = current_node->lastChild;
                if ( fst->nodeType == SimdNodeTypes::ZMM && scd->nodeType == SimdNodeTypes::ZMM ) {
                    asm_container.vmovdqu32 ( this->interpret_register_avx512 ( fst ), this->interpret_register_avx512 ( scd ) );
                }
                if ( fst->nodeType == SimdNodeTypes::ZMM && scd->nodeType == NodeTypes::MEM_AT ) {
                    asm_container.vmovdqu32 ( this->interpret_register_avx512 ( fst ), this->interpret_mem ( scd ) );
                }
                if ( fst->nodeType == NodeTypes::MEM_AT && scd->nodeType == SimdNodeTypes::ZMM ) {
                    asm_container.vmovdqu32 ( this->interpret_mem ( fst ), this->interpret_register_avx512 ( scd ) );
                }
            } else if ( current_node->nodeType == SimdNodeTypes::MOVDQU ) {
                assert(current_node->nChildren == 2 && "MOVQU has != 2 children");
                ir_node* fst  = current_node->firstChild;
                ir_node* scd = current_node->lastChild;
                if ( fst->nodeType == SimdNodeTypes::XMM && scd->nodeType == SimdNodeTypes::XMM ) {
                    asm_container.movdqu ( this->interpret_register_sse ( fst ), this->interpret_register_sse ( scd ) );
                }
                if ( fst->nodeType == SimdNodeTypes::XMM && scd->nodeType == NodeTypes::MEM_AT ) {
                    asm_container.movdqu ( this->interpret_register_sse ( fst ), this->interpret_mem ( scd ) );
                }
                if ( fst->nodeType == NodeTypes::MEM_AT && scd->nodeType == SimdNodeTypes::XMM ) {
                    asm_container.movdqu ( this->interpret_mem ( fst ), this->interpret_register_sse ( scd ) );
                }
            /* simd extract */
            } else if ( current_node->nodeType == SimdNodeTypes::VEXTRACTI64X2 ) {
                assert(current_node->nChildren == 3 && "VEXTRACTI64X2 has != 3 children");
                ir_node* fst = current_node->firstChild;
                ir_node* scd = current_node->firstChild->next;
                ir_node* thd = current_node->firstChild->next->next;

                asm_container.vextracti64x2 ( 
                    this->interpret_register_sse ( fst ), 
		    this->interpret_register_avx ( scd ),
                    this->interpret_constant ( thd )
		);
            } else if ( current_node->nodeType == SimdNodeTypes::VEXTRACTF128 ) {
                assert(current_node->nChildren == 3 && "VEXTRACTF128 has != 3 children");
                ir_node* fst = current_node->firstChild;
                ir_node* scd = current_node->firstChild->next;
                ir_node* thd = current_node->firstChild->next->next;

                asm_container.vextractf128 ( 
                    this->interpret_register_sse ( fst ), 
		    this->interpret_register_avx ( scd ),
                    this->interpret_constant ( thd )
		);
            } else if ( current_node->nodeType == SimdNodeTypes::VPEXTRQ ) {
                assert(current_node->nChildren == 3 && "VPEXTRQ has != 3 children");
                ir_node* fst = current_node->firstChild;
                ir_node* scd = current_node->firstChild->next;
                ir_node* thd = current_node->firstChild->next->next;

                asm_container.vpextrq ( 
                    this->interpret_register ( fst ), 
		    this->interpret_register_sse ( scd ),
                    this->interpret_constant ( thd )
		);
            } else if ( current_node->nodeType == SimdNodeTypes::PEXTRQ ) {
                assert(current_node->nChildren == 3 && "PEXTRQ has != 3 children");
                ir_node* fst = current_node->firstChild;
                ir_node* scd = current_node->firstChild->next;
                ir_node* thd = current_node->firstChild->next->next;

                asm_container.pextrq ( 
                    this->interpret_register ( fst ), 
		    this->interpret_register_sse ( scd ),
                    this->interpret_constant ( thd )
		);
            } else {
                std::cout << "NodeType not recognized by MC Emitter: " << current_node->nodeType << std::endl;
            }
    
            current_node = current_node->next;
        }
        return c;
    }


    void execute() {
        Func func;
        auto err = this->_runtime.add(&func, &this->_code);
        if (!err) {
            func();
        }
    }

private:

    asmjit::Label& label(asmjit::x86::Assembler &assembler, ir_node *node) {
        std::string label(node->ident);
        label.erase(std::remove_if(label.begin(), label.end(), [](char c) {
            return c == ':' || c == '\n';
        }), label.end());
    
        if(this->_labels.find(label) == this->_labels.end()) {
            this->_labels[label] = assembler.newLabel();
        }
    
        return this->_labels[label];
    }



    std::int64_t interpret_constant(ir_node *node) {
        switch ( node->nodeType ) {
            case CONSTANT_INT64:
                return node->int64Data;
            case CONSTANT_INT32:
                return (int64_t)node->int32Data;
            case CONSTANT_INT8:
                return (int64_t)node->int8Data;
            case CONSTANT_ADDRESS:
                return (int64_t)node->addressData;
            case CONSTANT_DOUBLE:
                return (int64_t)node->doubleData;
            default:
                /* todo: error */
                std::cerr << "Wrong constant type." << std::endl;
                exit(0); return 0;    
        }
    }


    asmjit::x86::Gp interpret_register(ir_node* node) {
        if ( node->nodeType == REG64 ) {
            if ( node->id == RAX) {
                return asmjit::x86::rax;
            } else if ( node->id == RCX) {
                return asmjit::x86::rcx;
            } else if ( node->id == RDX) {
                return asmjit::x86::rdx;
            } else if ( node->id == RBX) {
                return asmjit::x86::rbx;
            } else if ( node->id == RSP) {
                return asmjit::x86::rsp;
            } else if ( node->id == RBP) {
                return asmjit::x86::rbp;
            } else if ( node->id == RSI) {
                return asmjit::x86::rsi;
            } else if ( node->id == RDI) {
                return asmjit::x86::rdi;
            } else if ( node->id == R8) {
                return asmjit::x86::r8;
            } else if ( node->id == R9) {
                return asmjit::x86::r9;
            } else if ( node->id == R10) {
                return asmjit::x86::r10;
            } else if ( node->id == R11) {
                return asmjit::x86::r11;
            } else if ( node->id == R12) {
                return asmjit::x86::r12;
            } else if ( node->id == R13) {
                return asmjit::x86::r13;
            } else if ( node->id == R14) {
                return asmjit::x86::r14;
            } else if ( node->id == R15) {
                return asmjit::x86::r15;
            }
        }
        else if ( node->nodeType == REG32 ) {
            if ( node->id == EAX) {
                return asmjit::x86::eax;
            } else if ( node->id == ECX) {
                return asmjit::x86::ecx;
            } else if ( node->id == EDX) {
                return asmjit::x86::edx;
            } else if ( node->id == EBX) {
                return asmjit::x86::ebx;
            } else if ( node->id == ESP) {
                return asmjit::x86::esp;
            } else if ( node->id == EBP) {
                return asmjit::x86::ebp;
            } else if ( node->id == ESI) {
                return asmjit::x86::esi;
            } else if ( node->id == EDI) {
                return asmjit::x86::edi;
            } else if ( node->id == R8D) {
                return asmjit::x86::r8d;
            } else if ( node->id == R9D) {
                return asmjit::x86::r9d;
            } else if ( node->id == R10D) {
                return asmjit::x86::r10d;
            } else if ( node->id == R11D) {
                return asmjit::x86::r11d;
            } else if ( node->id == R12D) {
                return asmjit::x86::r12d;
            } else if ( node->id == R13D) {
                return asmjit::x86::r13d;
            } else if ( node->id == R14D) {
                return asmjit::x86::r14d;
            } else if ( node->id == R15D) {
                return asmjit::x86::r15d;
            }
        }
        else if ( node->nodeType == REG8 ) {
            if ( node->id == AL) {
                return asmjit::x86::al;
            } else if ( node->id == CL) {
                return asmjit::x86::cl;
            } else if ( node->id == BL) {
                return asmjit::x86::bl;
            } else if ( node->id == DL) {
                return asmjit::x86::dl;
            } else if ( node->id == SPL) {
                return asmjit::x86::spl;
            } else if ( node->id == BPL) {
                return asmjit::x86::bpl;
            } else if ( node->id == SIL) {
                return asmjit::x86::sil;
            } else if ( node->id == DIL) {
                return asmjit::x86::dil;
            } else if ( node->id == R8B) {
                return asmjit::x86::r8b;
            } else if ( node->id == R9B) {
                return asmjit::x86::r9b;
            } else if ( node->id == R10B) {
                return asmjit::x86::r10b;
            } else if ( node->id == R11B) {
                return asmjit::x86::r11b;
            } else if ( node->id == R12B) {
                return asmjit::x86::r12b;
            } else if ( node->id == R13B) {
                return asmjit::x86::r13b;
            } else if ( node->id == R14B) {
                return asmjit::x86::r14b;
            } else if ( node->id == R15B) {
                return asmjit::x86::r15b;
            }
        }
        assert(false && "Unknown REG");
        return asmjit::x86::rax; //todo: error
    }
    
    
    asmjit::x86::Xmm interpret_register_sse(ir_node* node) {
        if ( node->id == XMM0) {
            return asmjit::x86::xmm0;
        }
        if ( node->id == XMM1) {
            return asmjit::x86::xmm1;
        }
        if ( node->id == XMM2) {
            return asmjit::x86::xmm2;
        }
        if ( node->id == XMM3) {
            return asmjit::x86::xmm3;
        }
        if ( node->id == XMM4) {
            return asmjit::x86::xmm4;
        }
        if ( node->id == XMM5) {
            return asmjit::x86::xmm5;
        }
        if ( node->id == XMM6) {
            return asmjit::x86::xmm6;
        }
        if ( node->id == XMM7) {
            return asmjit::x86::xmm7;
        }
        if ( node->id == XMM8) {
            return asmjit::x86::xmm8;
        }
        if ( node->id == XMM9) {
            return asmjit::x86::xmm9;
        }
        if ( node->id == XMM10) {
            return asmjit::x86::xmm10;
        }
        if ( node->id == XMM11) {
            return asmjit::x86::xmm11;
        }
        if ( node->id == XMM12) {
            return asmjit::x86::xmm12;
        }
        if ( node->id == XMM13) {
            return asmjit::x86::xmm13;
        }
        if ( node->id == XMM14) {
            return asmjit::x86::xmm14;
        }
        if ( node->id == XMM15) {
            return asmjit::x86::xmm15;
        }
        assert(false && "Unknown REG");
        return asmjit::x86::xmm0; // todo: error
    }


    asmjit::x86::Ymm interpret_register_avx(ir_node* node) {
        if ( node->id == YMM0) {
            return asmjit::x86::ymm0;
        }
        if ( node->id == YMM1) {
            return asmjit::x86::ymm1;
        }
        if ( node->id == YMM2) {
            return asmjit::x86::ymm2;
        }
        if ( node->id == YMM3) {
            return asmjit::x86::ymm3;
        }
        if ( node->id == YMM4) {
            return asmjit::x86::ymm4;
        }
        if ( node->id == YMM5) {
            return asmjit::x86::ymm5;
        }
        if ( node->id == YMM6) {
            return asmjit::x86::ymm6;
        }
        if ( node->id == YMM7) {
            return asmjit::x86::ymm7;
        }
        if ( node->id == YMM8) {
            return asmjit::x86::ymm8;
        }
        if ( node->id == YMM9) {
            return asmjit::x86::ymm9;
        }
        if ( node->id == YMM10) {
            return asmjit::x86::ymm10;
        }
        if ( node->id == YMM11) {
            return asmjit::x86::ymm11;
        }
        if ( node->id == YMM12) {
            return asmjit::x86::ymm12;
        }
        if ( node->id == YMM13) {
            return asmjit::x86::ymm13;
        }
        if ( node->id == YMM14) {
            return asmjit::x86::ymm14;
        }
        if ( node->id == YMM15) {
            return asmjit::x86::ymm15;
        }
        assert(false && "Unknown REG");
        return asmjit::x86::ymm0;
    }


    asmjit::x86::Zmm interpret_register_avx512(ir_node* node) {
        if ( node->id == ZMM0) {
            return asmjit::x86::zmm0;
        }
        if ( node->id == ZMM1) {
            return asmjit::x86::zmm1;
        }
        if ( node->id == ZMM2) {
            return asmjit::x86::zmm2;
        }
        if ( node->id == ZMM3) {
            return asmjit::x86::zmm3;
        }
        if ( node->id == ZMM4) {
            return asmjit::x86::zmm4;
        }
        if ( node->id == ZMM5) {
            return asmjit::x86::zmm5;
        }
        if ( node->id == ZMM6) {
            return asmjit::x86::zmm6;
        }
        if ( node->id == ZMM7) {
            return asmjit::x86::zmm7;
        }
        if ( node->id == ZMM8) {
            return asmjit::x86::zmm8;
        }
        if ( node->id == ZMM9) {
            return asmjit::x86::zmm9;
        }
        if ( node->id == ZMM10) {
            return asmjit::x86::zmm10;
        }
        if ( node->id == ZMM11) {
            return asmjit::x86::zmm11;
        }
        if ( node->id == ZMM12) {
            return asmjit::x86::zmm12;
        }
        if ( node->id == ZMM13) {
            return asmjit::x86::zmm13;
        }
        if ( node->id == ZMM14) {
            return asmjit::x86::zmm14;
        }
        if ( node->id == ZMM15) {
            return asmjit::x86::zmm15;
        }
        assert(false && "Unknown REG");
        return asmjit::x86::zmm0; // todo: error
    }


    asmjit::x86::Mem interpret_mem(ir_node* node) 
    {
        if (node->nChildren == 1) {
            if (node->firstChild->nodeType == NodeTypes::MEM_ADD) {
                assert(node->firstChild->nChildren == 2 && "MEM_ADD has != 2 children");
                return asmjit::x86::ptr(
                        this->interpret_register(node->firstChild->firstChild),
                        this->interpret_constant(node->firstChild->lastChild)
                );
            } else if(node->firstChild->nodeType == NodeTypes::MEM_SUB) {
                assert(node->firstChild->nChildren == 2 && "MEM_SUB has != 2 children");
                return asmjit::x86::ptr(
                        this->interpret_register(node->firstChild->firstChild),
                        -this->interpret_constant(node->firstChild->lastChild)
                );
            } else if( isReg ( node->firstChild ) ) {
                return asmjit::x86::ptr(this->interpret_register(node->firstChild));
            } else if( isConst ( node->firstChild ) ) {
                return asmjit::x86::ptr(this->interpret_constant(node->firstChild));
            } else {
                std::cout << "TODO: MEM_AT (" << node->firstChild->nodeType << ")"
                          << std::endl;
            }
        } else {
            std::cout << "TODO: MEM_AT (" << node->nChildren << " params)"
                      << std::endl;
        }
        assert(false);
        return asmjit::x86::ptr(0); // todo: error
    }
};
    
