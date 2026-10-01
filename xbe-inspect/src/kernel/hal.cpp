#include "hal.hpp"
#include "../kernel.hpp"
#include <cstdio>
#include <cstdlib>

namespace xbe {
namespace kernel {

extern "C" void __attribute__((__stdcall__)) xbox_HalReturnToFirmware(uint32_t Routine) {
    xbe::record_kernel_call(49);
    printf("[KERNEL] HalReturnToFirmware(Routine: %u) cagrildi. Oyun kapaniyor/reboot ediliyor!\n", Routine);
    uint32_t ret_addr;
    asm volatile("mov 4(%%ebp), %0" : "=r"(ret_addr));
    printf("[DEBUG] HalReturnToFirmware called from 0x%08X\n", ret_addr);
    uint32_t* ebp_ptr;
    asm volatile("mov %%ebp, %0" : "=r"(ebp_ptr));
    printf("--- FATAL ERROR STACK TRACE ---\n");
    for(int i = 0; i < 5; i++) {
        if (!ebp_ptr) break;
        printf("Frame %d: EBP=%p, RET=0x%08X\n", i, ebp_ptr, ebp_ptr[1]);
        ebp_ptr = (uint32_t*)ebp_ptr[0];
    }
    printf("-------------------------------\n");
    exit(0);
}

} // namespace kernel
} // namespace xbe
