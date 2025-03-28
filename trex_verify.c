
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

#include "trex.h"
#include "trex_opcodes.h"
#include "trex_impl.h"

// insertion sort into two arrays using a1 as the main
static inline int insert_sorted(uint8_t* a1[], uint8_t* a2[], int n, int cap, uint8_t* e1, uint8_t* e2) {
    int i, j;

    // find the correct position for the new element:
    for (i = n - 1; i >= 0; i--) {
        // don't insert duplicates:
        if (a1[i] == e1) return n;
        // found the insertion spot:
        if (a1[i] < e1) break;
    }

    // no space left for insert?
    if (n >= cap) {
        return -1;
    }

    // shift elements to the right:
    for (j = n - 1; j > i; j--) {
        a1[j + 1] = a1[j];
        a2[j + 1] = a2[j];
    }

    // insert the new element at the found position:
    a1[i + 1] = e1;
    a2[i + 1] = e2;

    // return new size:
    return n + 1;
}

static void trex_sh_verify_pass1(struct trex_sm *sm, struct trex_sh* sh) {
    uint8_t     *pc = sh->pc_start;

    // sorted list of branch-target PCs to verify must be pointed at opcodes
#define vcap 128
    uint8_t     *vto[vcap];
    uint8_t     *vfr[vcap];
    int         vn = 0; // size of the list

#define verify_pc(n) if (pc+(n) >= sh->pc_end) { sh->verify_status = INVALID_OPCODE_INCOMPLETE; return; }

    sh->max_targets = 0;
    while (pc < sh->pc_end) {
        // verify the current branch-target PC:
        while (vn > 0) {
            if (vto[0] < pc) {
                // did we pass this PC already? it must be inside an opcode:
                sh->verify_status = INVALID_BRANCH_TARGET;
                sh->invalid_target_pc = vto[0];
                sh->invalid_pc = vfr[0];
                return;
            } else if (vto[0] == pc) {
                // this PC is valid; strike it from the list:
                for (int n = 1; n < vn; n++) {
                    vto[n-1] = vto[n];
                    vfr[n-1] = vfr[n];
                }
                vn--;
            } else {
                // this PC is ahead of us; ignore it for now
                break;
            }
        }

        sh->invalid_pc = pc;

        // load opcode:
        uint8_t i = ld8(&pc);

        // PC and stack ops:
        if (i == SYS1 || i == SYS2) {
            // syscall:
            verify_pc(i == SYS2 ? 1 : 0);
            uint16_t x = i == SYS2 ? ld16(&pc) : ld8(&pc);

            // verify the syscall number is in range:
            if (x >= sm->syscalls_count) {
                sh->verify_status = INVALID_SYSCALL_NUMBER;
                return;
            }

            // verify the syscall call function is provided:
            const struct trex_syscall *s = sm->syscalls[x];
            if (!s->call) {
                sh->verify_status = INVALID_SYSCALL_UNMAPPED;
                return;
            }
        }
        else if (i == IMM1)  {                                  // load immediate u8
            verify_pc(0);
            pc++;
        }
        else if (i == IMM2) {                                  // load immediate u16
            verify_pc(1);
            pc += 2;
        }
        else if (i == IMM3) {                                  // load immediate u24
            verify_pc(2);
            pc += 3;
        }
        else if (i == IMM4) {                                  // load immediate u32
            verify_pc(3);
            pc += 4;
        }
        else if (i == LDL1                                     // load from local
              || i == STL1) {                                  // store to local
            verify_pc(0);
            if (!sm->locals) {
                sh->verify_status = INVALID_LOCAL;
                return;
            }
            if (ld8(&pc) >= sm->locals_count) {
                sh->verify_status = INVALID_LOCAL;
                return;
            }
        }
        else if (i == LDL2                                     // load from local
              || i == STL2) {                                  // store to local
            verify_pc(1);
            if (!sm->locals) {
                sh->verify_status = INVALID_LOCAL;
                return;
            }
            if (ld16(&pc) >= sm->locals_count) {
                sh->verify_status = INVALID_LOCAL;
                return;
            }
        }
        else if (i == SST1) {                                  // set-state
            verify_pc(0);
            if (ld8(&pc) >= sm->handlers_count) {
                sh->verify_status = INVALID_STATE;
                return;
            }
        }
        else if (i == SST2) {                                  // set-state
            verify_pc(1);
            if (ld16(&pc) >= sm->handlers_count) {
                sh->verify_status = INVALID_STATE;
                return;
            }
        }
        else if (i == PSH1) { verify_pc(0);  ld8(&pc); }        // push immediate u8
        else if (i == PSH2) { verify_pc(1); ld16(&pc); }        // push immediate u16
        else if (i == PSH3) { verify_pc(2); ld24(&pc); }        // push immediate u24
        else if (i == PSH4) { verify_pc(3); ld32(&pc); }        // push immediate u32
        else if (i == BZ                                        // branch forward if A zero
              || i == BNZ) {                                    // branch forward if A not zero
            verify_pc(0);
            uint8_t *targetpc = (pc + *pc) + 1;
            // target out of range?
            if (targetpc >= sh->pc_end+1) {
                sh->verify_status = INVALID_BRANCH_TARGET;
                sh->invalid_target_pc = targetpc;
                return;
            }

            // record branch target PC for verification but only record distinct target PCs:
            int new_vn = insert_sorted(vto, vfr, vn, vcap, targetpc, sh->invalid_pc);
            if (new_vn < 0) {
                // not really invalid, just too many branches in flight for this tiny verifier to handle.
                sh->verify_status = INVALID_TOO_MANY_BRANCHES;
                sh->invalid_target_pc = targetpc;
                return;
            }
            vn = new_vn;
            if (vn > sh->max_targets) {
                sh->max_targets = vn;
            }
            pc++;
        }
        else if (i == PSHA) {                                   // push
            // stack analysis happens in trex_sh_verify_branch_path
        }
        else if (i == POP) {                                    // pop
            // stack analysis happens in trex_sh_verify_branch_path
        }

        // stack ops:
        else if (i == OR)   { }
        else if (i == XOR)  { }
        else if (i == AND)  { }
        else if (i == EQ)   { }
        else if (i == NE)   { }
        else if (i == LTU)  { }
        else if (i == LTS)  { }
        else if (i == GTU)  { }
        else if (i == GTS)  { }
        else if (i == LEU)  { }
        else if (i == LES)  { }
        else if (i == GEU)  { }
        else if (i == GES)  { }
        else if (i == SHL)  { }
        else if (i == SHRU) { }
        else if (i == SHRS) { }
        else if (i == ADD)  { }
        else if (i == SUB)  { }
        else if (i == MUL)  { }

        else if (i == RET)  { }
        else if (i == HALT) { }
        else {
            // unknown opcode:
            sh->verify_status = INVALID_OPCODE;
            return;
        }
    }

    if (pc > sh->pc_end) {
        sh->verify_status = INVALID_BRANCH_TARGET;
        sh->invalid_pc = pc;
        return;
    }

    // validate remaining branch targets:
    for (int n = 0; n < vn; n++) {
        if (vto[n] != pc) {
            sh->verify_status = INVALID_BRANCH_TARGET;
            sh->invalid_pc = vfr[n];
            sh->invalid_target_pc = vto[n];
            return;
        }
    }

#undef verify_pc
}

static void trex_sh_verify_branch_path(
    struct trex_sm *sm,
    struct trex_sh *sh,
    uint8_t *pc,
    long sp,
    long stack_max,
    uint32_t a,
    uint32_t aknown
) {
    // a = 0 if the "A zero" branch was taken to get here, 1 if the "A not zero" branch was taken

#define verify_stko  if (sp <   0) { sh->verify_status = INVALID_STACK_OVERFLOW;    return; }
#define verify_stku  if (sp >=  stack_max) { sh->verify_status = INVALID_STACK_UNDERFLOW;   return; }

    sh->branch_paths++;
    if (++sh->depth > sh->max_depth) { sh->max_depth = sh->depth; }

    while (pc < sh->pc_end) {
        sh->invalid_pc = pc;

        // load opcode:
        uint8_t i = ld8(&pc);

        // PC and stack ops:
        if (i == SYS1 || i == SYS2) {
            // syscall:
            uint16_t x = i == SYS2 ? ld16(&pc) : ld8(&pc);
            const struct trex_syscall *s = sm->syscalls[x];

            // verify we can pop args:
            for (int n = 0; n < s->args; n++) {
                verify_stku;
                sp++;
            }
            // verify we can push returns:
            for (int n = 0; n < s->returns; n++) {
                --sp;
                verify_stko;
            }
            // no way to predict the return values here that go on the stack.
        }
        else if (i == IMM1) {                                   // load immediate u8
            a = ld8(&pc);
            aknown = 1;
        }
        else if (i == IMM2) {                                   // load immediate u16
            a = ld16(&pc);
            aknown = 1;
        }
        else if (i == IMM3) {                                   // load immediate u24
            a = ld24(&pc);
            aknown = 1;
        }
        else if (i == IMM4) {                                   // load immediate u32
            a = ld32(&pc);
            aknown = 1;
        }
        else if (i == PSH1) {  ld8(&pc); --sp; verify_stko; }   // push immediate u8
        else if (i == PSH2) { ld16(&pc); --sp; verify_stko; }   // push immediate u16
        else if (i == PSH3) { ld24(&pc); --sp; verify_stko; }   // push immediate u24
        else if (i == PSH4) { ld32(&pc); --sp; verify_stko; }   // push immediate u32
        else if (i == LDL1) {                                   // load from local
            pc++;
            aknown = 0;
        }
        else if (i == LDL2) {                                   // load from local
            pc += 2;
            aknown = 0;
        }
        else if (i == STL1) {                                   // store to local
            pc++;
        }
        else if (i == STL2) {                                   // store to local
            pc += 2;
        }
        else if (i == SST1) {                                   // set-state
            pc++;
        }
        else if (i == SST2) {                                   // set-state
            pc += 2;
        }
        else if (i == BZ) {                                     // branch forward if A zero
            uint8_t offs = *pc;
            if (offs == 0) {
                // no branching is to be done, just move to the next PC:
                pc++;
            } else {
                // find the target PC of the "A is zero" branch:
                uint8_t *targetpc = (pc + offs) + 1;
                if (aknown) {
                    // A is a known value:
                    pc = a ? pc + 1 : targetpc;
                } else {
                    // split off to verify the "A known to be zero" branch path:
                    trex_sh_verify_branch_path(sm, sh, targetpc, sp, stack_max, 0, 1);
                    if (sh->verify_status != UNVERIFIED) {
                        // any error means we do not need to continue:
                        return;
                    }

                    // continue verifying the "A known to be NOT zero" branch:
                    a = 1;
                    aknown = 1;
                    pc++;
                }
            }
        }
        else if (i == BNZ) {                                    // branch forward if A not zero
            uint8_t offs = *pc;
            if (offs == 0) {
                // no branching is to be done, just move to the next PC:
                pc++;
            } else {
                // find the target PC of the "A is zero" branch:
                uint8_t *targetpc = (pc + offs) + 1;
                if (aknown) {
                    // A is a known value:
                    pc = a ? targetpc : pc + 1;
                } else {
                    // split off to verify the "A known to be NON zero" branch path:
                    trex_sh_verify_branch_path(sm, sh, targetpc, sp, stack_max, 1, 1);
                    if (sh->verify_status != UNVERIFIED) {
                        // any error means we do not need to continue:
                        return;
                    }
                    // continue verifying the "A known to be zero" branch:
                    a = 0; // we know A is zero along this branch
                    aknown = 1;
                    pc++;
                }
            }
        }
        else if (i == PSHA) {                                   // push
            --sp;
            verify_stko;
        }
        else if (i == POP) {                                    // pop
            verify_stku;
            sp++;
            // we do not track stack values so we must consider A unknown:
            aknown = 0;
        }

        // stack ops; we do not track stack values so we cannot predict the value of A afterwards:
        else if (i == OR)   { verify_stku; sp++; aknown = 0; }
        else if (i == XOR)  { verify_stku; sp++; aknown = 0; }
        else if (i == AND)  { verify_stku; sp++; aknown = 0; }
        else if (i == EQ)   { verify_stku; sp++; aknown = 0; }
        else if (i == NE)   { verify_stku; sp++; aknown = 0; }
        else if (i == LTU)  { verify_stku; sp++; aknown = 0; }
        else if (i == LTS)  { verify_stku; sp++; aknown = 0; }
        else if (i == GTU)  { verify_stku; sp++; aknown = 0; }
        else if (i == GTS)  { verify_stku; sp++; aknown = 0; }
        else if (i == LEU)  { verify_stku; sp++; aknown = 0; }
        else if (i == LES)  { verify_stku; sp++; aknown = 0; }
        else if (i == GEU)  { verify_stku; sp++; aknown = 0; }
        else if (i == GES)  { verify_stku; sp++; aknown = 0; }
        else if (i == SHL)  { verify_stku; sp++; aknown = 0; }
        else if (i == SHRU) { verify_stku; sp++; aknown = 0; }
        else if (i == SHRS) { verify_stku; sp++; aknown = 0; }
        else if (i == ADD)  { verify_stku; sp++; aknown = 0; }
        else if (i == SUB)  { verify_stku; sp++; aknown = 0; }
        else if (i == MUL)  { verify_stku; sp++; aknown = 0; }

        else if (i == RET) {
            // return stops branch path verification:
            break;
        } else if (i == HALT) {
            // halt stops branch path verification:
            break;
        } else {
            // unknown opcode:
            sh->verify_status = INVALID_OPCODE;
            return;
        }
    }

#undef verify_stku
#undef verify_stko

    --sh->depth;

    // stack must be empty on return:
    if (sp != stack_max) {
        sh->verify_status = INVALID_STACK_MUST_BE_EMPTY_ON_RETURN;
        return;
    }
}

// verifies that:
// * no PC access is out of bounds
// * no stack access is out of bounds
// * no local access is out of bounds
// * stack is empty on return for all branch paths
// * all branches point to opcode start
void trex_sh_verify(struct trex_context *ctx, struct trex_sm *sm, struct trex_sh* sh) {
    if (sh->verify_status == VERIFIED) {
        return;
    }

    // start out unverified:
    sh->verify_status = UNVERIFIED;
    sh->branch_paths = 0;
    sh->max_depth = 0;
    sh->depth = 0;

    // run pass 1 which does not follow branch paths:
    trex_sh_verify_pass1(sm, sh);
    if (sh->verify_status != UNVERIFIED) {
        // any error means we do not move to pass 2:
        return;
    }

    // recursively verify all branch paths to a RET instruction:
    // start with A known to be 0.
    long stack_max = ctx->stack_max - ctx->stack_min;
    trex_sh_verify_branch_path(sm, sh, sh->pc_start, stack_max, stack_max, 0, 1);

    // if we didn't error out then we've verified successfully:
    if (sh->verify_status == UNVERIFIED) {
        sh->verify_status = VERIFIED;
        sh->invalid_pc = 0;
    }
}

#ifdef __cplusplus
}
#endif
