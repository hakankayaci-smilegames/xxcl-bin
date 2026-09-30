#include <cstdint>
#include <cstdio>

extern "C" {
    uint32_t __attribute__((__stdcall__)) xbox_Ordinal187(uint32_t arg1) {
        printf("[KERNEL] Ordinal 187 cagrildi. Arg1: 0x%08X\n", arg1);
        return 0; // STATUS_SUCCESS
    }
}
