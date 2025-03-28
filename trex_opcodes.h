#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// opcodes for state handler:
enum {
    HALT,  RET,

    SYS1,  SYS2,
    IMM1,  IMM2,  IMM3,  IMM4,
    PSH1,  PSH2,  PSH3,  PSH4,
    LDL1,  LDL2,
    STL1,  STL2,
    SST1,  SST2,
    BZ,    BNZ,

    PSHA,  POP,
    OR,    XOR,   AND,
    EQ,    NE,    LTU,   LTS,   GTU,   GTS,   LEU,   LES,   GEU,   GES,
    SHL,   SHRU,  SHRS,
    ADD,   SUB,   MUL,
    __OPCODE_COUNT
};

#ifdef __cplusplus
}
#endif
