#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

enum exec_status {
    NOT_EXECUTABLE,
    READY,
    EXECUTING,
    IN_SYSCALL,
    HALTED,
    ERROR_UNVERIFIED,
    ERROR_SYSC_MISMATCHED_ARGS,
    ERROR_SYSC_MISMATCHED_RETS,
    ERROR_SYSC_INVALID_ARG,
    ERROR_SYSC_INVALID_STATE,
};

enum verify_status {
    UNVERIFIED,
    VERIFIED,
    INVALID_OPCODE,
    INVALID_OPCODE_INCOMPLETE,
    INVALID_STACK_OVERFLOW,
    INVALID_STACK_UNDERFLOW,
    INVALID_STACK_MUST_BE_EMPTY_ON_RETURN,
    INVALID_BRANCH_TARGET,
    INVALID_TOO_MANY_BRANCHES,
    INVALID_LOCAL,
    INVALID_STATE,
    INVALID_SYSCALL_NUMBER,
    INVALID_SYSCALL_UNMAPPED,
};

struct trex_context;
struct trex_sm;
struct trex_sh;
struct trex_syscall;

// state machine:
struct trex_sm {
    //// { readonly properties of state machine established on create:
    uint8_t      iterations; // number of iterations of state handlers to run

    // list of state handlers:
    struct trex_sh *handlers;
    uint16_t        handlers_count;

    // list of syscalls:
    const struct trex_syscall **syscalls;
    uint16_t                    syscalls_count;

    uint32_t    *locals;
    uint8_t      locals_count;
    //// }

    //// { mutable properties of state machine:
    // current execution status:
    enum exec_status exec_status;
    // current state number:
    uint16_t    st;
    // next state number:
    uint16_t    nxst;
    //// }
};

// state handler:
struct trex_sh {
    // points to where program code starts:
    uint8_t *pc_start;
    // points to one past last program byte:
    uint8_t *pc_end;

    // verification status:
    enum verify_status verify_status;

    uint8_t *invalid_pc;        // PC where invalidation occurred
    uint8_t *invalid_target_pc; // invalid target PC

    uint32_t branch_paths;      // count of distinct branching paths
    uint32_t max_depth;
    uint32_t max_targets;
    uint32_t depth;
};

// syscall descriptor:
struct trex_syscall {
    // name of the syscall
    const char *name;

    // how many args the syscall pops
    uint8_t  args;
    // how many return values the syscall pushes back
    uint8_t  returns;

    // call must pop `args` values, do work, and push `returns` values:
    void (*call)(struct trex_context *ctx);
};

// trex context to contain state machines, handlers, scheduler, and syscalls
struct trex_context {
    // opaque pointer for the host's use:
    void *hostdata;

    // points to lowest address of the stack region:
    uint32_t    *stack_min;
    // points to one past the end of the stack region:
    uint32_t    *stack_max;

    // current state handler execution state:
    unsigned curr_machine;
    signed iterations_remaining;
    struct trex_sm *sm;

    uint32_t    a;
    uint8_t     *pc;
    uint32_t    *sp;

    // expectations of current syscall to verify it behaves as stated:
    int expected_push;
    int expected_pops;

    // current list of all known state machines:
    struct trex_sm **machines;
    unsigned         machines_count;
};


// verify a state handler routine:
void trex_sh_verify(struct trex_context *ctx, struct trex_sm *sm, struct trex_sh *sh);

// for syscall usage; push a value onto the stack:
void trex_push(struct trex_context *ctx, uint32_t val);
// for syscall usage; pop a value off the stack:
void trex_pop(struct trex_context *ctx, uint32_t *o_val);

// initialize a state machine:
void trex_sm_init(
    struct trex_context *ctx,
    struct trex_sm *sm,
    uint8_t      iterations,
    struct trex_sh *handlers,
    uint16_t        handlers_count,
    const struct trex_syscall **syscalls,
    uint16_t                    syscalls_count,
    uint32_t    *locals,
    uint8_t      locals_count
);

// initialize a context with initial values for readonly properties:
void trex_context_init(
    struct trex_context *ctx,
    void *hostdata,
    uint32_t *stack,
    unsigned stack_size
);

// advance the scheduler to choose the next state machine, then execute the state machine for at most the specified number of cycles:
void trex_exec(struct trex_context *ctx, int cycles);

#ifdef __cplusplus
}
#endif
