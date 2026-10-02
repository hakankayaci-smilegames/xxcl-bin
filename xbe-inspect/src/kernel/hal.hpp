#pragma once
#include <cstdint>

namespace xbe {
namespace kernel {

extern "C" {
    void __attribute__((__stdcall__)) xbox_HalReturnToFirmware(uint32_t Routine);
    uint32_t __attribute__((__stdcall__)) xbox_HalGetInterruptVector(uint32_t BusInterruptLevel, uint8_t* Irql);
    void __attribute__((__stdcall__)) xbox_HalRegisterShutdownNotification(void* ShutdownRegistration, uint8_t Register);
}

} // namespace kernel
} // namespace xbe
