#include <stdint.h>

// opcodes for state handler:
enum {
    IMM8,IMM16,IMM24,IMM32,PSH,POP,JMP,BZ,BNZ,LDLOC,STLOC,SETST,
    OR,XOR,AND,EQ,NE,LTU,LTS,GTU,GTS,LEU,LES,GEU,GES,
    SHL,SHRU,SHRS,ADD,SUB,MUL,
    SYSC,RET
};

// state machine:
struct trex_sm {
    // current state:
    uint32_t    st;

    // persistent:
    uint32_t    *locals;
    uint32_t    *stack;

    // current handler execution state:
    uint32_t    a;
    uint8_t     *pc;
    uint32_t    *sp;
};

static inline uint32_t ld16(uint8_t **p) {
    uint32_t a = *(*p)++;
    a |= (uint32_t)(*(*p)++) << 8;
    return a;
}

static inline uint32_t ld24(uint8_t **p) {
    uint32_t a = *(*p)++;
    a |= (uint32_t)(*(*p)++) << 8;
    a |= (uint32_t)(*(*p)++) << 16;
    return a;
}

static inline uint32_t ld32(uint8_t **p) {
    uint32_t a = *(*p)++;
    a |= (uint32_t)(*(*p)++) << 8;
    a |= (uint32_t)(*(*p)++) << 16;
    a |= (uint32_t)(*(*p)++) << 24;
    return a;
}

// execute cycles on the current state handler; this relies on the handler being verified such
// that no stack access is out of bounds and no local access is out of bounds and no PC access
// is out of bounds.
void trex_exec_sm(struct trex_sm* sm, int cycles) {
    uint8_t     *pc = sm->pc;
    uint32_t    *sp = sm->sp;
    uint32_t    a = sm->a;

    for (int n = cycles - 1; n >= 0; n--) {
        uint8_t i = *pc++;

        // PC and stack ops:
        if      (i == IMM8)  a = *pc++;                         // load immediate u8
        else if (i == IMM16) a = ld16(&pc);                     // load immediate u16
        else if (i == IMM24) a = ld24(&pc);                     // load immediate u24
        else if (i == IMM32) a = ld32(&pc);                     // load immediate u32
        else if (i == PSH)   *--sp = a;                         // push
        else if (i == POP)   a = *sp++;                         // pop
        else if (i == JMP)   pc = (pc + *pc) + 1;               // jump forward
        else if (i == BZ)    pc = (a ? pc : pc + *pc) + 1;      // branch forward if A zero
        else if (i == BNZ)   pc = (a ? pc + *pc : pc) + 1;      // branch forward if A not zero
        else if (i == LDLOC) a = sm->locals[*pc++];             // load from local
        else if (i == STLOC) sm->locals[*pc++] = a;             // store to local
        else if (i == SETST) sm->st = a;                        // set-state

        // stack ops:
        else if (i == OR)   a = *sp++ |  a;
        else if (i == XOR)  a = *sp++ ^  a;
        else if (i == AND)  a = *sp++ &  a;
        else if (i == EQ)   a = *sp++ == a;
        else if (i == NE)   a = *sp++ != a;
        else if (i == LTU)  a = *sp++ <  a;
        else if (i == LTS)  a = (int32_t)*sp++ <  (int32_t)a;
        else if (i == GTU)  a = *sp++ >  a;
        else if (i == GTS)  a = (int32_t)*sp++ >  (int32_t)a;
        else if (i == LEU)  a = *sp++ <= a;
        else if (i == LES)  a = (int32_t)*sp++ <= (int32_t)a;
        else if (i == GEU)  a = *sp++ >= a;
        else if (i == GES)  a = (int32_t)*sp++ >= (int32_t)a;
        else if (i == SHL)  a = *sp++ << a;
        else if (i == SHRU) a = *sp++ >> a;
        else if (i == SHRS) a = (int32_t)*sp++ >> a;
        else if (i == ADD)  a = *sp++ +  a;
        else if (i == SUB)  a = *sp++ -  a;
        else if (i == MUL)  a = *sp++ *  a;

        else if (i == SYSC) {
            // TODO: syscall
        } else if (i == RET)  {
            // TODO: return
            break;
        } else {
            // TODO: unknown opcode
            break;
        }
    }

    sm->a = a;
    sm->pc = pc;
    sm->sp = sp;
}