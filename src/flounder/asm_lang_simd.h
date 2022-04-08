/**
 * @file
 * Definition of x86_64 simd extenstions.
 * @author Henning Funke <henning.funke@cs.tu-dortmund.de>
 */
enum SimdNodeTypes {
    XMM           = 300,
    YMM           = 301,
    ZMM           = 302,
    MOVDQA        = 303,
    VMOVDQA       = 304,
    VMOVDQA32     = 305,
    MOVDQU        = 306,
    VMOVDQU       = 307,
    VMOVDQU32     = 308,
    VEXTRACTF128  = 309,
    VPEXTRQ       = 310,
    PEXTRQ        = 311,
    VEXTRACTI64X2 = 312
};


enum AvxRegName {
    YMM0  =  0,
    YMM1  =  1,
    YMM2  =  2,
    YMM3  =  3,
    YMM4  =  4,
    YMM5  =  5,
    YMM6  =  6,
    YMM7  =  7,
    YMM8  =  8,
    YMM9  =  9,
    YMM10 = 10,
    YMM11 = 11,
    YMM12 = 12,
    YMM13 = 13,
    YMM14 = 14,
    YMM15 = 15
};


static char* ymmNames[] = { "ymm0",  "ymm1",  "ymm2",  "ymm3", 
                            "ymm4",  "ymm5",  "ymm6",  "ymm7",
                            "ymm8",  "ymm9",  "ymm10", "ymm11",
                            "ymm12", "ymm13", "ymm14", "ymm15" };


static ir_node* ymm ( enum AvxRegName id ) {
    M_Assert ( ((int)id >= 0 && (int)id < 16), "Use ymm register 0-15." );
    ir_node* res = literal ( ymmNames[id], YMM );
    res->id = id;
    res->nodeType = YMM;
    return res;
}

static ir_node* ymm ( int id ) {
    return ymm ( (AvxRegName) id );
}


static ir_node* vmovdqa ( ir_node* op1, ir_node* op2 ) {
    return binaryInstr ( "vmovdqa", op1, op2, VMOVDQA );
}


static ir_node* vmovdqu ( ir_node* op1, ir_node* op2 ) {
    return binaryInstr ( "vmovdqu", op1, op2, VMOVDQU );
}


static ir_node* vextracti64x2 ( ir_node* op1, ir_node* op2, ir_node* op3 ) {
    // xmm, zmm, imm8
    return ternaryInstr ( "vextracti64x2", op1, op2, op3, VEXTRACTI64X2 );
}


static ir_node* vextractf128 ( ir_node* op1, ir_node* op2, ir_node* op3 ) {
    // xmm, ymm, imm8
    return ternaryInstr ( "vextractf128", op1, op2, op3, VEXTRACTF128 );
}


static ir_node* vpextrq ( ir_node* op1, ir_node* op2, ir_node* op3 ) {
    // r64, xmm, imm8
    return ternaryInstr ( "vpextrq", op1, op2, op3, VPEXTRQ );
}


static ir_node* pextrq ( ir_node* op1, ir_node* op2, ir_node* op3 ) {
    // r64, xmm, imm8
    return ternaryInstr ( "pextrq", op1, op2, op3, PEXTRQ );
}


enum Avx512RegName {
    ZMM0  =  0,
    ZMM1  =  1,
    ZMM2  =  2,
    ZMM3  =  3,
    ZMM4  =  4,
    ZMM5  =  5,
    ZMM6  =  6,
    ZMM7  =  7,
    ZMM8  =  8,
    ZMM9  =  9,
    ZMM10 = 10,
    ZMM11 = 11,
    ZMM12 = 12,
    ZMM13 = 13,
    ZMM14 = 14,
    ZMM15 = 15
};

static char* zmmNames[] = { "zmm0",  "zmm1",  "zmm2",  "zmm3", 
                            "zmm4",  "zmm5",  "zmm6",  "zmm7",
                            "zmm8",  "zmm9",  "zmm10", "zmm11",
                            "zmm12", "zmm13", "zmm14", "zmm15" };


static ir_node* zmm ( enum Avx512RegName id ) {
    M_Assert ( ((int)id >= 0 && (int)id < 16), "Use zmm register 0-15." );
    ir_node* res = literal ( zmmNames[id], ZMM );
    res->id = id;
    res->nodeType = ZMM;
    return res;
}


static ir_node* zmm ( int id ) {
    return zmm ( (Avx512RegName) id );
}


static ir_node* vmovdqa32 ( ir_node* op1, ir_node* op2 ) {
    return binaryInstr ( "vmovdqa32", op1, op2, VMOVDQA32 );
}


static ir_node* vmovdqu32 ( ir_node* op1, ir_node* op2 ) {
    return binaryInstr ( "vmovdqu32", op1, op2, VMOVDQU32 );
}


enum SseRegName {
    XMM0  =  0,
    XMM1  =  1,
    XMM2  =  2,
    XMM3  =  3,
    XMM4  =  4,
    XMM5  =  5,
    XMM6  =  6,
    XMM7  =  7,
    XMM8  =  8,
    XMM9  =  9,
    XMM10 = 10,
    XMM11 = 11,
    XMM12 = 12,
    XMM13 = 13,
    XMM14 = 14,
    XMM15 = 15
};

static char* xmmNames[] = { "xmm0",  "xmm1",  "xmm2",  "xmm3", 
                            "xmm4",  "xmm5",  "xmm6",  "xmm7",
                            "xmm8",  "xmm9",  "xmm10", "xmm11",
                            "xmm12", "xmm13", "xmm14", "xmm15" };


static ir_node* xmm ( enum SseRegName id ) {
    printf ( "xmm id: %i\n", id );
    M_Assert ( (int)id >= 0 && (int)id < 16, "Use xmm register 0-15." );
    ir_node* res = literal ( xmmNames[id], XMM );
    res->id = id;
    res->nodeType = XMM;
    return res;
}


static ir_node* xmm ( int id ) {
    return xmm ( (SseRegName) id );
}


static ir_node* movdqa ( ir_node* op1, ir_node* op2 ) {
    return binaryInstr ( "movdqa", op1, op2, MOVDQA );
}


static ir_node* movdqu ( ir_node* op1, ir_node* op2 ) {
    return binaryInstr ( "movdqu", op1, op2, MOVDQU );
}

