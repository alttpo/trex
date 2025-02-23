#include <stdint.h>
#include <stdbool.h>

#include "trex.h"
#include "trex_opcodes.h"

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

// verifies that:
// * no PC access is out of bounds
// * no stack access is out of bounds
// * no local access is out of bounds
// * stack is empty on return
// * all branches point to opcode start
// result is put in sh->verified;
void trex_sh_verify(struct trex_sm *sm, struct trex_sh* sh) {
    uint8_t     *pc = sh->pc_start;
    uint32_t    *sp = sm->stack_max;

    // track list of branch-target PCs to verify must be pointed at opcodes
#define pcva_size 8
    uint8_t     *pcva[pcva_size];   // theoretical max of 128 branches
    int         pcv = 0;            // where the next PC to verify is inserted

#define verify_pc(n) if (pc+(n) >= sh->pc_end) { sh->verify_status = INVALID_OPCODE_INCOMPLETE; goto invalid; }
#define verify_stack if (sp <   sm->stack_min) { sh->verify_status = INVALID_STACK_OVERFLOW;    goto invalid; } \
                else if (sp >=  sm->stack_max) { sh->verify_status = INVALID_STACK_UNDERFLOW;   goto invalid; }

    sh->verify_status = UNVERIFIED;
    while (pc < sh->pc_end) {
        // verify the current branch-target PC:
        if (pcv > 0) {
            if (pcva[0] < pc) {
                // did we pass this PC already? it must be inside an opcode:
                sh->verify_status = INVALID_BRANCH_TARGET;
                goto invalid;
            } else if (pcva[0] == pc) {
                // this PC is valid; strike it from the list:
                for (int n = 1; n <= pcv; n++) {
                    pcva[n-1] = pcva[n];
                }
                pcv--;
            } else {
                // this PC is ahead of us; ignore it for now
            }
        }

        uint8_t i = *pc++;

        // PC and stack ops:
        if      (i == IMM8)  {                                  // load immediate u8
            verify_pc(0);
            pc++;
        }
        else if (i == IMM16) {                                  // load immediate u16
            verify_pc(1);
            pc += 2;
        }
        else if (i == IMM24) {                                  // load immediate u24
            verify_pc(2);
            pc += 3;
        }
        else if (i == IMM32) {                                  // load immediate u32
            verify_pc(3);
            pc += 4;
        }
        else if (i == PSH) {                                    // push
            --sp;
            verify_stack;
        }
        else if (i == POP) {                                    // pop
            verify_stack;
            sp++;
        }
        else if (i == BZ                                        // branch forward if A zero
              || i == BNZ) {                                    // branch forward if A not zero
            verify_pc(0);
            uint8_t *targetpc = (pc + *pc) + 1;
            if (targetpc >= sh->pc_end+1) {
                sh->verify_status = INVALID_BRANCH_TARGET;
                goto invalid;
            }
            // record branch destination PC for verification:
            pcva[pcv++] = targetpc;
            if (pcv > pcva_size) {
                // not really invalid, just too many branches in flight for this tiny verifier to handle.
                sh->verify_status = INVALID_BRANCH_TARGET;
                goto invalid;
            }
            pc++;
        }
        else if (i == LDLOC                                     // load from local
              || i == STLOC) {                                  // store to local
            verify_pc(0);
            if (*pc++ >= sm->locals_count) {
                sh->verify_status = INVALID_LOCAL;
                goto invalid;
            }
        }
        else if (i == SETST) {                                  // set-state
            verify_pc(1);
            if (ld16(&pc) >= sm->handlers_count) {
                sh->verify_status = INVALID_STATE;
                goto invalid;
            }
        }

        // stack ops:
        else if (i == OR)   { verify_stack; sp++; }
        else if (i == XOR)  { verify_stack; sp++; }
        else if (i == AND)  { verify_stack; sp++; }
        else if (i == EQ)   { verify_stack; sp++; }
        else if (i == NE)   { verify_stack; sp++; }
        else if (i == LTU)  { verify_stack; sp++; }
        else if (i == LTS)  { verify_stack; sp++; }
        else if (i == GTU)  { verify_stack; sp++; }
        else if (i == GTS)  { verify_stack; sp++; }
        else if (i == LEU)  { verify_stack; sp++; }
        else if (i == LES)  { verify_stack; sp++; }
        else if (i == GEU)  { verify_stack; sp++; }
        else if (i == GES)  { verify_stack; sp++; }
        else if (i == SHL)  { verify_stack; sp++; }
        else if (i == SHRU) { verify_stack; sp++; }
        else if (i == SHRS) { verify_stack; sp++; }
        else if (i == ADD)  { verify_stack; sp++; }
        else if (i == SUB)  { verify_stack; sp++; }
        else if (i == MUL)  { verify_stack; sp++; }

        else if (i == SYSC) {
            // TODO: syscall
        } else if (i == RET) {
            if (sp != sm->stack_max) {
                // stack must be empty on return:
                sh->verify_status = INVALID_STACK_MUST_BE_EMPTY_ON_RETURN;
                goto invalid;
            }
        } else {
            // unknown opcode:
            sh->verify_status = INVALID_OPCODE;
            goto invalid;
        }
    }

#undef verify_stack
#undef verify_pc

    sh->verify_status = VERIFIED;
    sh->invalid_pc = 0;
    return;

invalid:
    sh->invalid_pc = pc;
}

// execute cycles on the current state handler; this relies on the handler being verified such
// that no stack access is out of bounds and no local access is out of bounds and no PC access
// is out of bounds.
void trex_sm_exec(struct trex_sm* sm, int cycles) {
    struct trex_sh  *sh;

    if (sm->exec_status == READY) {
        // move to next state:
        sm->exec_status = EXECUTING;
        sm->st = sm->nxst;
        sh = sm->handlers + sm->st;

        // reset registers:
        sm->pc = sh->pc_start;
        sm->sp = sm->stack_max;
        sm->a = 0;
    } else {
        // EXECUTING status:
        sh = sm->handlers + sm->st;
    }

    // make sure the state handler has been verified and is valid:
    if (sh->verify_status != VERIFIED) {
        // TODO: set error
        return;
    }

    uint8_t         *pc = sm->pc;
    uint8_t         *pc_end = sh->pc_end;
    uint32_t        *sp = sm->sp;
    uint32_t        a = sm->a;

    for (int n = cycles - 1; n >= 0; n--) {
        if (pc >= pc_end) {
            goto handle_return;
        }

        uint8_t i = *pc++;

        // PC and stack ops:
        if      (i == IMM8)  a = *pc++;                         // load immediate u8
        else if (i == IMM16) a = ld16(&pc);                     // load immediate u16
        else if (i == IMM24) a = ld24(&pc);                     // load immediate u24
        else if (i == IMM32) a = ld32(&pc);                     // load immediate u32
        else if (i == PSH)   *--sp = a;                         // push
        else if (i == POP)   a = *sp++;                         // pop
        else if (i == BZ)    pc = (a ? pc : pc + *pc) + 1;      // branch forward if A zero
        else if (i == BNZ)   pc = (a ? pc + *pc : pc) + 1;      // branch forward if A not zero
        else if (i == LDLOC) a = sm->locals[*pc++];             // load from local
        else if (i == STLOC) sm->locals[*pc++] = a;             // store to local
        else if (i == SETST) sm->nxst = ld16(&pc);              // set-state

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
        } else if (i == RET) {
            goto handle_return;
        } else {
            // TODO: unknown opcode
            break;
        }
    }
    goto done;

handle_return:
    sm->exec_status = READY;
    // fall through

done:
    sm->a = a;
    sm->pc = pc;
    sm->sp = sp;
}
