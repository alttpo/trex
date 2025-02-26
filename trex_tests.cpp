
#include <string_view>
#include <array>
#include <iostream>
#include <iomanip>

extern "C" {
#include "trex.h"
#include "trex_opcodes.h"
}

constexpr std::array verify_status_names = {
    std::string_view{"UNVERIFIED"},
    std::string_view{"VERIFIED"},
    std::string_view{"INVALID_OPCODE"},
    std::string_view{"INVALID_OPCODE_INCOMPLETE"},
    std::string_view{"INVALID_STACK_OVERFLOW"},
    std::string_view{"INVALID_STACK_UNDERFLOW"},
    std::string_view{"INVALID_STACK_MUST_BE_EMPTY_ON_RETURN"},
    std::string_view{"INVALID_BRANCH_TARGET"},
    std::string_view{"INVALID_LOCAL"},
    std::string_view{"INVALID_STATE"},
    std::string_view{"INVALID_SYSCALL_NUMBER"},
    std::string_view{"INVALID_SYSCALL_UNMAPPED"},
};

uint32_t chip_curr = 0;
struct {
    uint32_t addr;
    uint8_t  mem[512];
} chips[2];

struct trex_syscall syscalls[] = {
    { // 0: chip-use
        .args = 1,
        .call = [](struct trex_sm *sm){
            uint32_t a;
            trex_sm_pop(sm, &a);
            chip_curr = a;
        },
    },
    { // 1: chip-address-set
        .args = 1,
        .call = [](struct trex_sm *sm){
            uint32_t a;
            trex_sm_pop(sm, &a);
            chips[chip_curr].addr = a;
        },
    },
    { // 2: chip-read-no-advance-byte
        .returns = 1,
        .call = [](struct trex_sm *sm){
            trex_sm_push(sm,
                chips[chip_curr].mem[chips[chip_curr].addr]
            );
        },
    },
    { // 3: chip-read-advance-byte
        .returns = 1,
        .call = [](struct trex_sm *sm){
            trex_sm_push(sm,
                chips[chip_curr].mem[chips[chip_curr].addr++]
            );
        },
    },
    { // 4: chip-read-dword
        .returns = 1,
        .call = [](struct trex_sm *sm){
            uint32_t a;
            a  = chips[chip_curr].mem[chips[chip_curr].addr++];
            a |= chips[chip_curr].mem[chips[chip_curr].addr++] << 8;
            a |= chips[chip_curr].mem[chips[chip_curr].addr++] << 16;
            a |= chips[chip_curr].mem[chips[chip_curr].addr++] << 24;
            trex_sm_push(sm, a);
        },
    },
    { // 5: chip-write-no-advance-byte
        .args = 1,
        .call = [](struct trex_sm *sm){
            uint32_t a;
            trex_sm_pop(sm, &a);
            chips[chip_curr].mem[chips[chip_curr].addr] = a;
        },
    },
    { // 6: chip-write-advance-byte
        .args = 1,
        .call = [](struct trex_sm *sm){
            uint32_t a;
            trex_sm_pop(sm, &a);
            chips[chip_curr].mem[chips[chip_curr].addr++] = a;
        },
    },
    { // 7: chip-write-dword
        .args = 1,
        .call = [](struct trex_sm *sm){
            uint32_t a;
            trex_sm_pop(sm, &a);
            chips[chip_curr].mem[chips[chip_curr].addr++] = a;
            chips[chip_curr].mem[chips[chip_curr].addr++] = a >> 8;
            chips[chip_curr].mem[chips[chip_curr].addr++] = a >> 16;
            chips[chip_curr].mem[chips[chip_curr].addr++] = a >> 24;
        },
    },
};

bool verify_sh(struct trex_sm &sm, struct trex_sh &sh) {
    trex_sh_verify(&sm, &sh);

    std::cout << "verify_status = " << sh.verify_status
        << " (" << verify_status_names[sh.verify_status] << ")"
        << std::endl;
    std::cout << "  branch_paths = " << sh.branch_paths << std::endl;
    std::cout << "  max_depth    = " << sh.max_depth << std::endl;
    if (sh.verify_status > VERIFIED) {
        std::cout << "  at pc = " << (sh.invalid_pc - sh.pc_start) << std::endl;
        return false;
    }

    return true;
}

int test_readme_program(struct trex_sm &sm) {
    struct trex_sh sh[3];

    sm.handlers = sh;
    sm.handlers_count = 3;

    uint8_t sh0_code[] = {
        //; we write all the asm for 2C00 handler except the first byte and then enable it with the final write to 2C00:
        //(chip-use nmix)
        IMM8, 1,
        PSH,
        SYSC, 0,
        //(chip-address-set 1)
        IMM8, 1,
        PSH,
        SYSC, 1,
        //; write this 65816 asm to fxpak's NMI handler:
        //; 2C00   9C 00 2C   STZ   $2C00
        //; 2C03   6C EA FF   JMP   ($FFEA)
        //(chip-write-dword 002C6CEA)
        IMM32, 0x00, 0x2C, 0x6C, 0xEA,
        PSH,
        SYSC, 7,
        //(chip-write-advance-byte     FF)
        IMM8, 0xFF,
        PSH,
        SYSC, 6,
        //; move to next state:
        //(set-state 1)
        SETST, 1, 0,
        //(return)
        RET,
    };

    sh[0].pc_start = sh0_code;
    sh[0].pc_end = sh0_code + sizeof(sh0_code);

    uint8_t sh1_code[] = {
        //; write the first byte of the asm routine to enable it:
        //(chip-use nmix)
        IMM8, 1,
        PSH,
        SYSC, 0,
        //(chip-address-set 0)
        IMM8, 0,
        PSH,
        SYSC, 1,
        //(chip-write-no-advance-byte 9C)
        IMM8, 0x9C,
        PSH,
        SYSC, 5,
        //; move to next state:
        //(set-state 2)
        SETST, 2,
        //(return)
        RET,
    };

    sh[1].pc_start = sh1_code;
    sh[1].pc_end = sh1_code + sizeof(sh1_code);

    uint8_t sh2_code[] = {
        //(chip-read-no-advance-byte)
        SYSC, 2,
        //(pop)
        POP,
        //(bz nmi)    ; branch if A is zero to "nmi" label
        BZ, 1,
        //(return)    ; else return
        RET,
        //(label nmi)
        //; NMI has fired! read 4 bytes from WRAM at $0010:
        //(chip-use wram)
        IMM8, 0,
        PSH,
        SYSC, 0,
        //(chip-address-set 10)
        IMM8, 0x10,
        PSH,
        SYSC, 1,
        //(chip-read-dword)
        SYSC, 4,
        //; append that data to a message and send it:
        //(message-append-dword)
        //(message-send)
        POP,
        STLOC, 0,
        //; set-state to 1 and return:
        //(set-state 1)
        SETST, 1,
        //(return)
        RET,
    };

    sh[2].pc_start = sh2_code;
    sh[2].pc_end = sh2_code + sizeof(sh2_code);

    // verify the handler programs:
    for (int i = 0; i < sm.handlers_count; i++) {
        if (!verify_sh(sm, sh[i])) {
            return 1;
        }
    }

    // insert some junk into WRAM to see if the program can read it:
    chips[0].mem[0x10] = 0x14;
    chips[0].mem[0x11] = 0x01;
    chips[0].mem[0x12] = 0x2B;
    chips[0].mem[0x13] = 0xAA;

    // execute the state machine:
    trex_sm_exec(&sm, 1024);
    std::cout << "exec_status = " << sm.exec_status << std::endl;
    trex_sm_exec(&sm, 1024);
    std::cout << "exec_status = " << sm.exec_status << std::endl;
    // run an iteration of state 2:
    trex_sm_exec(&sm, 1024);
    std::cout << "exec_status = " << sm.exec_status << std::endl;
    // pretend NMIX ran and reset $2C00 to 00:
    chips[1].mem[0] = 0;
    // rerun state 2:
    trex_sm_exec(&sm, 1024);
    std::cout << "exec_status = " << sm.exec_status << std::endl;

    // verify results:
    for (int i = 0; i < 2; i++) {
        std::cout << "chip " << i << std::endl;
        for (int j = 0; j < 32; j++) {
            std::cout << std::setw(2) << std::setfill('0') << std::hex << (int)chips[i].mem[j] << " ";
        }
        std::cout << std::endl;
    }

    std::cout << "locals:" << std::endl;
    for (int i = 0; i < 16; i++) {
        std::cout << std::setw(8) << std::setfill('0') << std::hex << sm.locals[i] << " ";
    }
    std::cout << std::endl;

    return 0;
}

int test_branch_verify(struct trex_sm &sm) {
    struct trex_sh sh[1];

    std::cout << "branch verify:" << std::endl;

    sm.handlers = sh;
    sm.handlers_count = 1;

    uint8_t p[] = {
        IMM8, 0,
        BZ, 2,
        BZ, 2,
        BNZ, 2,
        BZ, 2,
        BZ, 2,
        BNZ, 2,
        BZ, 2,
        BZ, 2,
        BNZ, 2,
        BZ, 2,
        BZ, 2,
        BNZ, 2,
        BZ, 2,
        BZ, 2,
        BNZ, 2,
        BZ, 2,
        RET,
        RET,
    };

    sh[0].pc_start = p;
    sh[0].pc_end = p + sizeof(p);

    if (!verify_sh(sm, sh[0])) {
        return 1;
    }

    return 0;
}

int main() {
    struct trex_sm sm;

    uint32_t stack[16] = {0};
    uint32_t locals[16] = {0};

    sm.st = 0;
    sm.nxst = 0;
    sm.exec_status = READY;

    sm.stack_min = stack;
    sm.stack_max = stack + sizeof(stack)/sizeof(uint32_t);
    sm.locals = locals;
    sm.locals_count = 16;
    sm.syscalls = syscalls;
    sm.syscalls_count = sizeof(syscalls)/sizeof(struct trex_syscall);

    test_branch_verify(sm);

//    test_readme_program(sm);

    return 0;
}
