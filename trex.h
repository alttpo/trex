#include <stdint.h>
#include <stdbool.h>


enum exec_status {
    READY,
    EXECUTING,
};

enum verify_status {
    UNVERIFIED,
    VERIFIED,
    INVALID_OPCODE,
    INVALID_STACK_OVERFLOW,
    INVALID_STACK_UNDERFLOW,
    INVALID_OPCODE_INCOMPLETE,
    INVALID_BRANCH_TARGET,
    INVALID_LOCAL,
    INVALID_STATE,
    INVALID_STACK_MUST_BE_EMPTY_ON_RETURN,
};

struct trex_sh;

// state machine:
struct trex_sm {
    //// { readonly properties of state machine established on create:
    uint32_t        *locals;
    uint8_t         locals_count;
    uint32_t        *stack_min, *stack_max;

    // list of state handlers:
    struct trex_sh *handlers;
    uint16_t        handlers_count;
    //// }

    //// { mutable properties of state machine:
    enum exec_status exec_status;
    // current state:
    uint16_t    st;
    // next state:
    uint16_t    nxst;

    // current handler execution state:
    uint32_t    a;
    uint8_t     *pc;
    uint32_t    *sp;
    //// }
};

// state handler:
struct trex_sh {
    // where program code starts:
    uint8_t *pc_start;
    // where program code ends: (points to last instruction)
    uint8_t *pc_end;

    // verification status:
    enum verify_status verify_status;
    uint8_t *invalid_pc; // PC where invalidation occurred
};

void trex_sh_verify(struct trex_sm *sm, struct trex_sh* sh);
void trex_exec_sm(struct trex_sm* sm, int cycles);
