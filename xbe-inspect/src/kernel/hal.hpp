#pragma once
#include <cstdint>

namespace xbe {
namespace kernel {

extern "C" {
    void __attribute__((__stdcall__)) xbox_HalReturnToFirmware(uint32_t Routine);
}

} // namespace kernel
} // namespace xbe
