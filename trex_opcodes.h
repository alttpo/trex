#pragma once

// opcodes for state handler:
enum {
    HALT,  RET,

    SYS1,  SYS2,
    IMM1,  IMM2,  IMM3,  IMM4,
    LDL1,  LDL2,
    STL1,  STL2,
    SST1,  SST2,
    BZ,    BNZ,

    PSH,   POP,
    OR,    XOR,   AND,
    EQ,    NE,    LTU,   LTS,   GTU,   GTS,   LEU,   LES,   GEU,   GES,
    SHL,   SHRU,  SHRS,
    ADD,   SUB,   MUL,
    __OPCODE_COUNT
};
