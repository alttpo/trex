#include <stdint.h>
#include <stdbool.h>

#include "trex.h"
#include "trex_opcodes.h"
#include "trex_impl.h"

// for syscall usage; push a value onto the stack:
void trex_sm_push(struct trex_sm *sm, uint32_t val) {
    if (sm->exec_status == IN_SYSCALL) {
        sm->expected_push--;
    }
    *--sm->sp = val;
}

// for syscall usage; pop a value off the stack:
void trex_sm_pop(struct trex_sm *sm, uint32_t *o_val) {
    if (sm->exec_status == IN_SYSCALL) {
        sm->expected_pops--;
    }
    *o_val = *sm->sp++;
}

// execute cycles on the current state handler; this relies on the handler being verified such
// that no stack access is out of bounds and no local access is out of bounds and no PC access
// is out of bounds.
void trex_sm_exec(struct trex_sm* sm, int cycles) {
    const struct trex_sh  *sh;
    if (sm->exec_status == HALTED) {
        return;
    }

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

    // don't continue if we're in HALTED or ERRORED status:
    if (sm->exec_status != EXECUTING) {
        return;
    }

    // make sure the state handler has been verified:
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
            sm->exec_status = READY;
            break;
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
            uint8_t x = *pc++;
            struct trex_syscall *s = sm->syscalls + x;

            // switch to IN_SYSCALL status so we can verify push/pop calls:
            sm->exec_status = IN_SYSCALL;
            sm->expected_pops = s->args;
            sm->expected_push = s->returns;

            sm->sp = sp;
            s->call(sm);
            sp = sm->sp;

            // if we didn't error out, return to EXECUTING status:
            if (sm->exec_status == IN_SYSCALL) {
                if (sm->expected_pops != 0) {
                    sm->exec_status = ERRORED;
                    break;
                } else if (sm->expected_push != 0) {
                    sm->exec_status = ERRORED;
                    break;
                } else {
                    sm->exec_status = EXECUTING;
                }
            }
        } else if (i == RET) {
            sm->exec_status = READY;
            break;
        } else if (i == HALT) {
            sm->exec_status = HALTED;
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
