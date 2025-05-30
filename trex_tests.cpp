
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
    std::string_view{"INVALID_TOO_MANY_BRANCHES"},
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
    { // 0:
        .name = "chip-use",
        .args = 1,
        .call = [](struct trex_context *ctx){
            uint32_t a;
            trex_pop(ctx, &a);
            chip_curr = a;
        },
    },
    { // 1:
        .name = "chip-address-set",
        .args = 1,
        .call = [](struct trex_context *ctx){
            uint32_t a;
            trex_pop(ctx, &a);
            chips[chip_curr].addr = a;
        },
    },
    { // 2:
        .name = "chip-read-no-advance-byte",
        .returns = 1,
        .call = [](struct trex_context *ctx){
            trex_push(ctx,
                chips[chip_curr].mem[chips[chip_curr].addr]
            );
        },
    },
    { // 3:
        .name = "chip-read-advance-byte",
        .returns = 1,
        .call = [](struct trex_context *ctx){
            trex_push(ctx,
                chips[chip_curr].mem[chips[chip_curr].addr++]
            );
        },
    },
    { // 4:
        .name = "chip-read-dword",
        .returns = 1,
        .call = [](struct trex_context *ctx){
            uint32_t a;
            a  = chips[chip_curr].mem[chips[chip_curr].addr++];
            a |= chips[chip_curr].mem[chips[chip_curr].addr++] << 8;
            a |= chips[chip_curr].mem[chips[chip_curr].addr++] << 16;
            a |= chips[chip_curr].mem[chips[chip_curr].addr++] << 24;
            trex_push(ctx, a);
        },
    },
    { // 5:
        .name = "chip-write-no-advance-byte",
        .args = 1,
        .call = [](struct trex_context *ctx){
            uint32_t a;
            trex_pop(ctx, &a);
            chips[chip_curr].mem[chips[chip_curr].addr] = a;
        },
    },
    { // 6:
        .name = "chip-write-advance-byte",
        .args = 1,
        .call = [](struct trex_context *ctx){
            uint32_t a;
            trex_pop(ctx, &a);
            chips[chip_curr].mem[chips[chip_curr].addr++] = a;
        },
    },
    { // 7:
        .name = "chip-write-dword",
        .args = 1,
        .call = [](struct trex_context *ctx){
            uint32_t a;
            trex_pop(ctx, &a);
            chips[chip_curr].mem[chips[chip_curr].addr++] = a;
            chips[chip_curr].mem[chips[chip_curr].addr++] = a >> 8;
            chips[chip_curr].mem[chips[chip_curr].addr++] = a >> 16;
            chips[chip_curr].mem[chips[chip_curr].addr++] = a >> 24;
        },
    },
};

bool verify_sh(struct trex_context &ctx, struct trex_sm &sm, struct trex_sh &sh) {
    std::cout << "verify_status = " << sh.verify_status
        << " (" << verify_status_names[sh.verify_status] << ")"
        << std::endl;
    std::cout << "  max_targets  = " << sh.max_targets << std::endl;
    std::cout << "  branch_paths = " << sh.branch_paths << std::endl;
    std::cout << "  max_depth    = " << sh.max_depth << std::endl;
    if (sh.verify_status > VERIFIED) {
        std::cout << "  at pc = " << (sh.invalid_pc - sh.pc_start) << std::endl;
        if (sh.verify_status == INVALID_BRANCH_TARGET) {
            std::cout << "  to pc = " << (sh.invalid_target_pc - sh.pc_start) << std::endl;
        }
        return false;
    }

    return true;
}

int test_readme_program(struct trex_context &ctx) {
    struct trex_sh sh[3];

    auto &sm = ctx.machines[0];

    std::cout << "readme:" << std::endl;

    uint8_t sh0_code[] = {
        //; we write all the asm for 2C00 handler except the first byte and then enable it with the final write to 2C00:
        //(chip-use nmix)
        PSH1, 1,
        SYS1, 0,
        //(chip-address-set 1)
        PSH1, 1,
        SYS1, 1,
        //; write this 65816 asm to fxpak's NMI handler:
        //; 2C00   9C 00 2C   STZ   $2C00
        //; 2C03   6C EA FF   JMP   ($FFEA)
        //(chip-write-dword 002C6CEA)
        PSH4, 0x00, 0x2C, 0x6C, 0xEA,
        SYS1, 7,
        //(chip-write-advance-byte     FF)
        PSH1, 0xFF,
        SYS1, 6,
        //; move to next state:
        //(set-state 1)
        SST1, 1,
        //(return)
        RET,
    };

    sh[0].pc_start = sh0_code;
    sh[0].pc_end = sh0_code + sizeof(sh0_code);

    uint8_t sh1_code[] = {
        //; write the first byte of the asm routine to enable it:
        //(chip-use nmix)
        PSH1, 1,
        SYS1, 0,
        //(chip-address-set 0)
        PSH1, 0,
        SYS1, 1,
        //(chip-write-no-advance-byte 9C)
        PSH1, 0x9C,
        SYS1, 5,
        //; move to next state:
        //(set-state 2)
        SST1, 2,
        //(return)
        RET,
    };

    sh[1].pc_start = sh1_code;
    sh[1].pc_end = sh1_code + sizeof(sh1_code);

    uint8_t sh2_code[] = {
        //(chip-read-no-advance-byte)
        SYS1, 2,
        //(pop)
        POP,
        //(bz nmi)    ; branch if A is zero to "nmi" label
        BZ, 1,
        //(return)    ; else return
        RET,
        //(label nmi)
        //; NMI has fired! read 4 bytes from WRAM at $0010:
        //(chip-use wram)
        PSH1, 0,
        SYS1, 0,
        //(chip-address-set 10)
        PSH1, 0x10,
        SYS1, 1,
        //(chip-read-dword)
        SYS1, 4,
        //; append that data to a message and send it:
        //(message-append-dword)
        //(message-send)
        POP,
        STL1, 0,
        //; set-state to 1 and return:
        //(set-state 1)
        SST1, 1,
        //(return)
        RET,
    };

    sh[2].pc_start = sh2_code;
    sh[2].pc_end = sh2_code + sizeof(sh2_code);

    // verify the handler programs:
    trex_sm_verify(&ctx, &sm, 3, sh);
    for (int i = 0; i < sm.handlers_count; i++) {
        if (!verify_sh(ctx, sm, sh[i])) {
            return 1;
        }
    }

    // insert some junk into WRAM to see if the program can read it:
    chips[0].mem[0x10] = 0x14;
    chips[0].mem[0x11] = 0x01;
    chips[0].mem[0x12] = 0x2B;
    chips[0].mem[0x13] = 0xAA;

    // execute the state machine:
    trex_exec(&ctx);
    std::cout << "exec_status = " << sm.exec_status << std::endl;
    trex_exec(&ctx);
    std::cout << "exec_status = " << sm.exec_status << std::endl;
    // run an iteration of state 2:
    trex_exec(&ctx);
    std::cout << "exec_status = " << sm.exec_status << std::endl;
    // pretend NMIX ran and reset $2C00 to 00:
    chips[1].mem[0] = 0;
    // rerun state 2:
    trex_exec(&ctx);
    std::cout << "exec_status = " << sm.exec_status << std::endl;

    // verify results:
    for (int i = 0; i < 2; i++) {
        std::cout << "chip " << i << ": ";
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

int test_branch_verify(struct trex_context &ctx) {
    auto &sm = ctx.machines[0];

    struct trex_sh sh[1];

    std::cout << "branch verify:" << std::endl;

    uint8_t p0[] = {
        IMM1, 0,
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

    sh[0].pc_start = p0;
    sh[0].pc_end = p0 + sizeof(p0);

    trex_sm_verify(&ctx, &sm, 1, sh);
    if (!verify_sh(ctx, sm, sh[0])) {
        return 1;
    }

    uint8_t p1[] = {
        BZ,  0xFE,
        BNZ, 0xFC,
        BZ,  0xFA,
        BNZ, 0xF8,
        BZ,  0xF6,
        BNZ, 0xF4,
        BZ,  0xF2,
        BNZ, 0xF0,
        BZ,  0xEE,
        BNZ, 0xEC,
        BZ,  0xEA,
        BNZ, 0xE8,
        BZ,  0xE6,
        BNZ, 0xE4,
        BZ,  0xE2,
        BNZ, 0xE0,
        BZ,  0xDE,
        BNZ, 0xDC,
        BZ,  0xDA,
        BNZ, 0xD8,
        BZ,  0xD6,
        BNZ, 0xD4,
        BZ,  0xD2,
        BNZ, 0xD0,
        BZ,  0xCE,
        BNZ, 0xCC,
        BZ,  0xCA,
        BNZ, 0xC8,
        BZ,  0xC6,
        BNZ, 0xC4,
        BZ,  0xC2,
        BNZ, 0xC0,
        BZ,  0xBE,
        BNZ, 0xBC,
        BZ,  0xBA,
        BNZ, 0xB8,
        BZ,  0xB6,
        BNZ, 0xB4,
        BZ,  0xB2,
        BNZ, 0xB0,
        BZ,  0xAE,
        BNZ, 0xAC,
        BZ,  0xAA,
        BNZ, 0xA8,
        BZ,  0xA6,
        BNZ, 0xA4,
        BZ,  0xA2,
        BNZ, 0xA0,
        BZ,  0x9E,
        BNZ, 0x9C,
        BZ,  0x9A,
        BNZ, 0x98,
        BZ,  0x96,
        BNZ, 0x94,
        BZ,  0x92,
        BNZ, 0x90,
        BZ,  0x8E,
        BNZ, 0x8C,
        BZ,  0x8A,
        BNZ, 0x88,
        BZ,  0x86,
        BNZ, 0x84,
        BZ,  0x82,
        BNZ, 0x80,
        BZ,  0x7E,
        BNZ, 0x7C,
        BZ,  0x7A,
        BNZ, 0x78,
        BZ,  0x76,
        BNZ, 0x74,
        BZ,  0x72,
        BNZ, 0x70,
        BZ,  0x6E,
        BNZ, 0x6C,
        BZ,  0x6A,
        BNZ, 0x68,
        BZ,  0x66,
        BNZ, 0x64,
        BZ,  0x62,
        BNZ, 0x60,
        BZ,  0x5E,
        BNZ, 0x5C,
        BZ,  0x5A,
        BNZ, 0x58,
        BZ,  0x56,
        BNZ, 0x54,
        BZ,  0x52,
        BNZ, 0x50,
        BZ,  0x4E,
        BNZ, 0x4C,
        BZ,  0x4A,
        BNZ, 0x48,
        BZ,  0x46,
        BNZ, 0x44,
        BZ,  0x42,
        BNZ, 0x40,
        BZ,  0x3E,
        BNZ, 0x3C,
        BZ,  0x3A,
        BNZ, 0x38,
        BZ,  0x36,
        BNZ, 0x34,
        BZ,  0x32,
        BNZ, 0x30,
        BZ,  0x2E,
        BNZ, 0x2C,
        BZ,  0x2A,
        BNZ, 0x28,
        BZ,  0x26,
        BNZ, 0x24,
        BZ,  0x22,
        BNZ, 0x20,
        BZ,  0x1E,
        BNZ, 0x1C,
        BZ,  0x1A,
        BNZ, 0x18,
        BZ,  0x16,
        BNZ, 0x14,
        BZ,  0x12,
        BNZ, 0x10,
        BZ,  0x0E,
        BNZ, 0x0C,
        BZ,  0x0A,
        BNZ, 0x08,
        BZ,  0x06,
        BNZ, 0x04,
        BZ,  0x02,
        RET,
        RET,
    };

    sh[0].pc_start = p1;
    sh[0].pc_end = p1 + sizeof(p1);

    trex_sm_verify(&ctx, &sm, 1, sh);
    if (!verify_sh(ctx, sm, sh[0])) {
        return 1;
    }

    // 128 independent branch targets:
    uint8_t p3[] = {
        BZ,  0xFE,
        BNZ, 0xFE,
        BZ,  0xFE,
        BNZ, 0xFE,
        BZ,  0xFE,
        BNZ, 0xFE,
        BZ,  0xFE,
        BNZ, 0xFE,
        BZ,  0xFE,
        BNZ, 0xFE,
        BZ,  0xFE,
        BNZ, 0xFE,
        BZ,  0xFE,
        BNZ, 0xFE,
        BZ,  0xFE,
        BNZ, 0xFE,
        BZ,  0xFE,
        BNZ, 0xFE,
        BZ,  0xFE,
        BNZ, 0xFE,
        BZ,  0xFE,
        BNZ, 0xFE,
        BZ,  0xFE,
        BNZ, 0xFE,
        BZ,  0xFE,
        BNZ, 0xFE,
        BZ,  0xFE,
        BNZ, 0xFE,
        BZ,  0xFE,
        BNZ, 0xFE,
        BZ,  0xFE,
        BNZ, 0xFE,
        BZ,  0xFE,
        BNZ, 0xFE,
        BZ,  0xFE,
        BNZ, 0xFE,
        BZ,  0xFE,
        BNZ, 0xFE,
        BZ,  0xFE,
        BNZ, 0xFE,
        BZ,  0xFE,
        BNZ, 0xFE,
        BZ,  0xFE,
        BNZ, 0xFE,
        BZ,  0xFE,
        BNZ, 0xFE,
        BZ,  0xFE,
        BNZ, 0xFE,
        BZ,  0xFE,
        BNZ, 0xFE,
        BZ,  0xFE,
        BNZ, 0xFE,
        BZ,  0xFE,
        BNZ, 0xFE,
        BZ,  0xFE,
        BNZ, 0xFE,
        BZ,  0xFE,
        BNZ, 0xFE,
        BZ,  0xFE,
        BNZ, 0xFE,
        BZ,  0xFE,
        BNZ, 0xFE,
        BZ,  0xFE,
        BNZ, 0xFE,
        BZ,  0xFE,
        BNZ, 0xFE,
        BZ,  0xFE,
        BNZ, 0xFE,
        BZ,  0xFE,
        BNZ, 0xFE,
        BZ,  0xFE,
        BNZ, 0xFE,
        BZ,  0xFE,
        BNZ, 0xFE,
        BZ,  0xFE,
        BNZ, 0xFE,
        BZ,  0xFE,
        BNZ, 0xFE,
        BZ,  0xFE,
        BNZ, 0xFE,
        BZ,  0xFE,
        BNZ, 0xFE,
        BZ,  0xFE,
        BNZ, 0xFE,
        BZ,  0xFE,
        BNZ, 0xFE,
        BZ,  0xFE,
        BNZ, 0xFE,
        BZ,  0xFE,
        BNZ, 0xFE,
        BZ,  0xFE,
        BNZ, 0xFE,
        BZ,  0xFE,
        BNZ, 0xFE,
        BZ,  0xFE,
        BNZ, 0xFE,
        BZ,  0xFE,
        BNZ, 0xFE,
        BZ,  0xFE,
        BNZ, 0xFE,
        BZ,  0xFE,
        BNZ, 0xFE,
        BZ,  0xFE,
        BNZ, 0xFE,
        BZ,  0xFE,
        BNZ, 0xFE,
        BZ,  0xFE,
        BNZ, 0xFE,
        BZ,  0xFE,
        BNZ, 0xFE,
        BZ,  0xFE,
        BNZ, 0xFE,
        BZ,  0xFE,
        BNZ, 0xFE,
        BZ,  0xFE,
        BNZ, 0xFE,
        BZ,  0xFE,
        BNZ, 0xFE,
        BZ,  0xFE,
        BNZ, 0xFE,
        BZ,  0xFE,
        BNZ, 0xFE,
        BZ,  0xFE,
        BNZ, 0xFE,
        BZ,  0xFE,
        BNZ, 0xFE,
        BZ,  0xFE,
        BNZ, 0xFE,
        BZ,  0xFE,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
        RET,
    };

    sh[0].pc_start = p3;
    sh[0].pc_end = p3 + sizeof(p3);

    trex_sm_verify(&ctx, &sm, 1, sh);
    if (!verify_sh(ctx, sm, sh[0])) {
        return 1;
    }

    // verify that overlapping branches out of order is ok:
    uint8_t p2[] = {
        BNZ, 0x20,
        BZ,  0x0E,
        BNZ, 0x10,
        BZ,  0x1A,
        BNZ, 0x18,
        BZ,  0x16,
        BNZ, 0x14,
        BZ,  0x12,
        BNZ, 0x08,
        BZ,  0x0E,
        BNZ, 0x0C,
        BZ,  0x0A,
        BNZ, 0x0A,
        BZ,  0x06,
        BNZ, 0x04,
        BZ,  0x02,
        RET,
        RET,
        RET,
        RET,
        RET,
    };

    sh[0].pc_start = p2;
    sh[0].pc_end = p2 + sizeof(p2);

    trex_sm_verify(&ctx, &sm, 1, sh);
    if (!verify_sh(ctx, sm, sh[0])) {
        return 1;
    }

    return 0;
}

int main() {
    struct trex_context ctx;
    struct trex_sm machines[1];

    uint32_t stack[16]  = {0};
    uint32_t locals[16] = {0};

    trex_context_init(
        &ctx,
        nullptr,
        stack,
        16,
        1024,
        sizeof(syscalls)/sizeof(struct trex_syscall),
        syscalls
    );

    ctx.machines_count = 1;
    ctx.machines = machines;

    trex_sm_init(
        &ctx,
        &machines[0],
        1,
        16,
        locals
    );

    test_branch_verify(ctx);

    test_readme_program(ctx);

    return 0;
}
