#include <stdio.h>
#include <stdlib.h>

#include "../inc/util.h"
#include "../inc/arm11.h"

#define ASSERT(expr, ...)                                \
    if(!(expr)) {                                        \
        fprintf(stderr, "%s:%d ", __FILE__, __LINE__);   \
        fprintf(stderr, __VA_ARGS__);                    \
        exit(1);                                         \
    }

int main() {
    arm11_Init();

    FILE* fd = fopen("tests/arm_instr.elf", "rb");
    if(fd == NULL) {
        fprintf(stderr, "Failed to open file.\n");
        return 1;
    }

    // Load file.
    if(loader_LoadFile(fd) != 0) {
        fprintf(stderr, "Failed to load file.\n");
        fclose(fd);
        return 1;
    }

    arm11_Step();
    ASSERT(arm11_R(0) == 0x31, "mov1 fail\n");
    arm11_Step();
    ASSERT(arm11_R(1) == 0x80000001, "mov2 fail\n");

    return 0;
}
