cat << 'HAL_EOF' > xbe-inspect/src/kernel/hal.cpp
#include "hal.hpp"
#include "../kernel.hpp"
#include <cstdio>
#include <cstdlib>

namespace xbe {
namespace kernel {

extern "C" void __attribute__((__stdcall__)) xbox_HalReturnToFirmware(uint32_t Routine) {
    xbe::record_kernel_call(49);
    printf("[KERNEL] HalReturnToFirmware(Routine: %u) cagrildi. Oyun kapaniyor/reboot ediliyor!\n", Routine);
    exit(0);
}

} // namespace kernel
} // namespace xbe
HAL_EOF
