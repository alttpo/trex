#include <stdint.h>
#include <stdbool.h>

#include "trex.h"
#include "trex_opcodes.h"
#include "trex_impl.h"

// for syscall usage; push a value onto the stack:
void trex_push(struct trex_context *ctx, uint32_t val) {
    if (ctx->sm->exec_status == IN_SYSCALL) {
        ctx->expected_push--;
    }
    *--ctx->sp = val;
}

// for syscall usage; pop a value off the stack:
void trex_pop(struct trex_context *ctx, uint32_t *o_val) {
    if (ctx->sm->exec_status == IN_SYSCALL) {
        ctx->expected_pops--;
    }
    *o_val = *ctx->sp++;
}

// execute cycles on the current state handler; this relies on the handler being verified such
// that no stack access is out of bounds and no local access is out of bounds and no PC access
// is out of bounds.
int trex_sm_exec(struct trex_context *ctx, int cycles) {
    const struct trex_sh  *sh;
    struct trex_sm *sm = ctx->sm;
    if (!sm) {
        return cycles;
    }

    if (sm->exec_status == HALTED) {
        return cycles;
    }

    if (sm->exec_status == READY) {
        // move to next state:
        sm->exec_status = EXECUTING;
        sm->st = sm->nxst;
        sh = sm->handlers + sm->st;

        // reset registers:
        ctx->pc = sh->pc_start;
        ctx->sp = ctx->stack_max;
        ctx->a = 0;
    } else {
        // EXECUTING status:
        sh = sm->handlers + sm->st;
    }

    // don't continue if we're in HALTED or ERRORED status:
    if (sm->exec_status != EXECUTING) {
        return cycles;
    }

    // make sure the state handler has been verified:
    if (sh->verify_status != VERIFIED) {
        // TODO: set error
        return cycles;
    }

    uint8_t         *pc = ctx->pc;
    uint8_t         *pc_end = sh->pc_end;
    uint32_t        *sp = ctx->sp;
    uint32_t        a = ctx->a;

    while (cycles > 0) {
        if (pc >= pc_end) {
            sm->exec_status = READY;
            break;
        }

        cycles--;
        uint8_t i = ld8(&pc);

        // PC and stack ops:
        if (i == SYS1 || i == SYS2) {
            uint16_t x;
            x = (i == SYS2) ? ld16(&pc) : ld8(&pc);
            const struct trex_syscall *s = sm->syscalls + x;

            // switch to IN_SYSCALL status so we can verify push/pop calls:
            sm->exec_status = IN_SYSCALL;
            ctx->expected_pops = s->args;
            ctx->expected_push = s->returns;

            ctx->sp = sp;
            s->call(ctx);
            sp = ctx->sp;

            // if we didn't error out, return to EXECUTING status:
            if (sm->exec_status == IN_SYSCALL) {
                if (ctx->expected_pops != 0) {
                    sm->exec_status = ERRORED;
                    break;
                } else if (ctx->expected_push != 0) {
                    sm->exec_status = ERRORED;
                    break;
                } else {
                    sm->exec_status = EXECUTING;
                }
            }
        }
        else if (i == IMM1) a = ld8(&pc);                       // load immediate u8
        else if (i == IMM2) a = ld16(&pc);                      // load immediate u16
        else if (i == IMM3) a = ld24(&pc);                      // load immediate u24
        else if (i == IMM4) a = ld32(&pc);                      // load immediate u32
        else if (i == LDL1) a = sm->locals[ld8(&pc)];           // load from local
        else if (i == LDL2) a = sm->locals[ld16(&pc)];          // load from local
        else if (i == STL1) sm->locals[ld8(&pc)] = a;           // store to local
        else if (i == STL2) sm->locals[ld16(&pc)] = a;          // store to local
        else if (i == SST1) sm->nxst = ld8(&pc);                // set-state
        else if (i == SST2) sm->nxst = ld16(&pc);               // set-state
        else if (i == BZ)   pc = (a ? pc : pc + *pc) + 1;       // branch forward if A zero
        else if (i == BNZ)  pc = (a ? pc + *pc : pc) + 1;       // branch forward if A not zero
        else if (i == PSH)  *--sp = a;                          // push
        else if (i == POP)  a = *sp++;                          // pop

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

        else if (i == RET) {
            sm->exec_status = READY;
            break;
        }
        else if (i == HALT) {
            sm->exec_status = HALTED;
            break;
        }
        else {
            // TODO: unknown opcode
            break;
        }
    }

    ctx->a = a;
    ctx->pc = pc;
    ctx->sp = sp;

    return cycles;
}

// advance the scheduler to choose the next state machine, then execute the state machine for at most the specified number of cycles:
void trex_exec(struct trex_context *ctx, int cycles) {
    int last_cycles = 0;
    while (cycles > 0 && cycles != last_cycles) {
        // if necessary, find the next machine to execute:
        if (!ctx->sm) {
            for (int i = 0; i < ctx->machines_count && !ctx->sm; ++i, ++ctx->curr_machine) {
                if (ctx->curr_machine >= ctx->machines_count) {
                    ctx->curr_machine = 0;
                }

                ctx->sm = ctx->machines[ctx->curr_machine];
                if (ctx->sm && ctx->sm->exec_status >= HALTED) {
                    ctx->sm = 0;
                }
            }
            if (!ctx->sm) {
                // no machines to run:
                return;
            }

            // reset the iteration counter:
            ctx->iterations_remaining = ctx->sm->iterations;
        }

        if (ctx->sm->exec_status == READY) {
            if (--ctx->iterations_remaining < 0) {
                ctx->sm = 0;
                continue;
            }
        } else if (ctx->sm->exec_status >= HALTED) {
            ctx->sm = 0;
            continue;
        }

        last_cycles = cycles;
        cycles = trex_sm_exec(ctx, cycles);
    }
}


void trex_context_init(
    struct trex_context *ctx,
    void *hostdata,
    uint32_t *stack,
    unsigned stack_size
) {
    ctx->hostdata = hostdata;
    ctx->stack_min = stack;
    ctx->stack_max = stack + stack_size;

    ctx->curr_machine = 0;
    ctx->sm = 0;
    ctx->iterations_remaining = 0;

    ctx->a = 0;
    ctx->pc = 0;
    ctx->sp = 0;

    ctx->expected_pops = 0;
    ctx->expected_push = 0;

    ctx->machines = 0;
    ctx->machines_count = 0;
}
