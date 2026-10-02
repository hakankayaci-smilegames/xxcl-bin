#pragma once
#include <cstdint>

namespace xbe {
namespace kernel {

extern "C" {
    void __attribute__((__stdcall__)) xbox_HalReturnToFirmware(uint32_t Routine);
    uint32_t __attribute__((__stdcall__)) xbox_HalGetInterruptVector(uint32_t BusInterruptLevel, uint8_t* Irql);
}

} // namespace kernel
} // namespace xbe
