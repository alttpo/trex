
#include <string_view>
#include <array>
#include <iostream>

extern "C" {
#include "trex.h"
#include "trex_opcodes.h"
}

constexpr std::array verify_status_names = {
    std::string_view{"UNVERIFIED"},
    std::string_view{"VERIFIED"},
    std::string_view{"INVALID_OPCODE"},
    std::string_view{"INVALID_STACK_OVERFLOW"},
    std::string_view{"INVALID_STACK_UNDERFLOW"},
    std::string_view{"INVALID_OPCODE_INCOMPLETE"},
    std::string_view{"INVALID_BRANCH_TARGET"},
    std::string_view{"INVALID_LOCAL"},
    std::string_view{"INVALID_STATE"},
    std::string_view{"INVALID_STACK_MUST_BE_EMPTY_ON_RETURN"},
};


int main() {
    struct trex_sm sm;
    struct trex_sh sh;
    uint32_t stack[16] = {0};
    uint8_t sh_code[] = {
        IMM8, 1,
        BZ,   1,
        RET,
        PSH,
    };

    sh.pc_start = sh_code;
    sh.pc_end = sh_code + sizeof(sh_code);
    sm.handlers = &sh;
    sm.handlers_count = 1;

    sm.st = 0;
    sm.nxst = 0;
    sm.exec_status = READY;
    sm.stack_min = stack;
    sm.stack_max = stack + sizeof(stack)/sizeof(uint32_t);

    // verify the handler program:
    trex_sh_verify(&sm, &sh);

    std::cout << "verify_status = " << sh.verify_status
        << " (" << verify_status_names[sh.verify_status] << ")"
        << std::endl;
    if (sh.verify_status > VERIFIED) {
        std::cout << "  at pc = " << (sh.invalid_pc - sh.pc_start) << std::endl;
        return 0;
    }

    // execute the handler:
    trex_sm_exec(&sm, 4);
    std::cout << "exec_status = " << sm.exec_status << std::endl;

    return 0;
}
