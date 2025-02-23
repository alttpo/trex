
#include <vector>

extern "C" {
#include "trex.h"
#include "trex_opcodes.h"
}

int main() {
    struct trex_sm sm;
    struct trex_sh sh;
    uint8_t sh_code[] = {
        RET,
    };

    sh.pc_start = sh_code;
    sh.pc_end = sh_code + sizeof(sh_code);
    sm.handlers = &sh;
    sm.handlers_count = 1;

    trex_sh_verify(&sm, &sh);
    printf("%d\n", sh.verify_status);

    return 0;
}
