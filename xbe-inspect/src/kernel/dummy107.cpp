#include <cstdint>
#include <cstdio>

extern "C" {
    uint32_t __attribute__((__stdcall__)) xbox_Ordinal107() {
        printf("[KERNEL] Ordinal 107 cagrildi.\n");
        return 0; // STATUS_SUCCESS / Old IRQL
    }
}
