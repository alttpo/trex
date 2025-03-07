#pragma once

// opcodes for state handler:
enum {
    HALT,  RET,
    IMM8,  IMM16, IMM24, IMM32,
    PSH,   POP,
    BZ,    BNZ,
    LDLOC, STLOC,
    SETST,
    OR,    XOR,   AND,
    EQ,    NE,    LTU,   LTS,   GTU,   GTS,   LEU,   LES,   GEU,   GES,
    SHL,   SHRU,  SHRS,
    ADD,   SUB,   MUL,
    SYSC,
    __OPCODE_COUNT
};
