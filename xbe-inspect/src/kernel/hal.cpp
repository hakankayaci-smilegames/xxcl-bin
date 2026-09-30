#include "hal.hpp"
#include "../kernel.hpp"
#include <iostream>
#include <cstdlib>

namespace xbe {
namespace kernel {

extern "C" void __attribute__((__stdcall__)) xbox_HalReturnToFirmware(uint32_t Routine) {
    xbe::record_kernel_call(84);
    std::cerr << "\n\033[1;31m[KERNEL PANIC]\033[0m HalReturnToFirmware cagrildi! Kod: 0x" << std::hex << std::uppercase << Routine << std::dec << "\n";
    xbe::print_execution_stats();
    std::exit(1);
}

} // namespace kernel
} // namespace xbe
