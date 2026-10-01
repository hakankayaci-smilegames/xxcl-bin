#include "ob.hpp"
#include <cstdio>

extern "C" {
    NTSTATUS __attribute__((__stdcall__)) xbox_NtClose(uint32_t Handle) {
        printf("[KERNEL] NtClose(Handle: 0x%08X) cagrildi.\n", Handle);
        return 0; // STATUS_SUCCESS
    }
}
