#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

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

// state handler:
struct trex_sh {
    // verification status:
    enum verify_status verify_status;

    uint8_t *invalid_pc;        // PC where invalidation occurred
    uint8_t *invalid_target_pc; // invalid target PC

    uint32_t branch_paths;      // count of distinct branching paths
    uint32_t max_depth;
    uint32_t max_targets;
    uint32_t depth;

    // points to where program code starts:
    uint8_t *pc_start;
    // points to one past last program byte:
    uint8_t *pc_end;
};

// state machine:
struct trex_sm {
    //// mutable properties of state machine:
    // current execution status:
    enum exec_status exec_status;
    // current state number:
    uint16_t         st;
    // next state number:
    uint16_t         nxst;

    //// readonly properties of state machine established on create:
    // number of iterations of state handlers to run
    uint8_t        iterations;

    // read/write area of memory for execution
    uint8_t        locals_count;
    uint32_t      *locals;

    // list of state handlers
    uint16_t        handlers_count;
    struct trex_sh *handlers;
};

struct trex_context;

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
    // current state handler execution state:
    uint32_t    a;
    uint8_t     *pc;
    uint32_t    *sp;

    unsigned curr_machine;
    struct trex_sm *sm;

    int iterations_remaining;

    // expectations of current syscall to verify it behaves as stated:
    int expected_push;
    int expected_pops;

    // points to lowest address of the stack region:
    uint32_t    *stack_min;
    // points to one past the end of the stack region:
    uint32_t    *stack_max;

    // list of all syscalls:
    uint16_t                   syscalls_count;
    const struct trex_syscall *syscalls;

    // list of all state machines:
    unsigned        machines_count;
    struct trex_sm *machines;

    // how many instructions to advance per trex_exec() call:
    int cycles_per_exec;

    // opaque pointer for the host's use:
    void *hostdata;
};

// initialize a context with initial values for readonly properties:
void trex_context_init(
    struct trex_context *ctx,
    void *hostdata,
    uint32_t *stack,
    unsigned stack_size,
    int cycles_per_exec,
    uint16_t                   syscalls_count,
    const struct trex_syscall *syscalls
);

// initialize a state machine:
void trex_sm_init(
    struct trex_context *ctx,
    struct trex_sm *sm,
    uint8_t      iterations,
    uint8_t      locals_count,
    uint32_t    *locals
);

// provide state handlers to a state machine and verify them all:
void trex_sm_verify(
    const struct trex_context *ctx,
    struct trex_sm *sm,
    uint16_t        handlers_count,
    struct trex_sh *handlers
);

// advance the scheduler to choose the next state machine, then execute the state machine for at most the specified number of cycles:
void trex_exec(struct trex_context *ctx);

// for syscall usage; push a value onto the stack:
void trex_push(struct trex_context *ctx, uint32_t val);
// for syscall usage; pop a value off the stack:
void trex_pop(struct trex_context *ctx, uint32_t *o_val);

#ifdef __cplusplus
}
#endif
