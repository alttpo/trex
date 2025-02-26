#pragma once

#include <stdint.h>
#include <stdbool.h>

enum exec_status {
    READY,
    EXECUTING,
    IN_SYSCALL,
    ERRORED,
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
    INVALID_LOCAL,
    INVALID_STATE,
    INVALID_SYSCALL_NUMBER,
    INVALID_SYSCALL_UNMAPPED,
};

struct trex_sh;
struct trex_syscall;

// state machine:
struct trex_sm {
    //// { readonly properties of state machine established on create:
    uint32_t       *locals;
    uint8_t         locals_count;
    uint32_t       *stack_min, *stack_max;

    // list of state handlers:
    struct trex_sh *handlers;
    uint16_t        handlers_count;

    // list of syscalls:
    struct trex_syscall *syscalls;
    uint8_t              syscalls_count;
    //// }

    //// { mutable properties of state machine:
    // current execution status:
    enum exec_status exec_status;
    // current state number:
    uint16_t    st;
    // next state number:
    uint16_t    nxst;

    // current state handler execution state:
    uint32_t    a;
    uint8_t     *pc;
    uint32_t    *sp;

    int expected_push;
    int expected_pops;
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
    uint8_t *invalid_pc; // PC where invalidation occurred
    uint32_t branch_paths; // count of distinct branching paths
    uint32_t max_depth;
    uint32_t depth;
};

// syscall descriptor:
struct trex_syscall {
    uint8_t  args;
    uint8_t  returns;

    // call must pop `args` values, do work, and push `returns` values:
    void (*call)(struct trex_sm *sm);
};


// verify a state handler routine:
void trex_sh_verify(struct trex_sm *sm, struct trex_sh *sh);
// execute the state machine for a specified number of cycles:
void trex_sm_exec(struct trex_sm *sm, int cycles);


// for syscall usage; push a value onto the stack:
void trex_sm_push(struct trex_sm *sm, uint32_t val);
// for syscall usage; pop a value off the stack:
void trex_sm_pop(struct trex_sm *sm, uint32_t *o_val);
