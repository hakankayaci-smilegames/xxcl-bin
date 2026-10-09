#pragma once
#include <cstdint>

namespace xbe {
namespace kernel {

extern "C" {
    void hal_init_usb_mmio();
    void __attribute__((__stdcall__)) xbox_HalReturnToFirmware(uint32_t Routine);
    uint32_t __attribute__((__stdcall__)) xbox_HalGetInterruptVector(uint32_t BusInterruptLevel, uint8_t* Irql);
    void __attribute__((__stdcall__)) xbox_HalRegisterShutdownNotification(void* ShutdownRegistration, uint8_t Register);
    uint32_t __attribute__((__stdcall__)) xbox_HalReadWritePCISpace(uint32_t BusNumber, uint32_t SlotNumber, uint32_t RegisterNumber, void* Buffer, uint32_t Length, uint8_t WritePCISpace);
    uint32_t __attribute__((__stdcall__)) xbox_AvSendTVEncoderOption(void* OptionData, uint32_t OptionSize, uint32_t OptionType, void* ResultData);

}

} // namespace kernel
} // namespace xbe
