#pragma once
#include <cstdint>
#include "types.hpp"

namespace xbe {
namespace kernel {

extern "C" {
    uint32_t __attribute__((__stdcall__)) xbox_XeLoadSection(xbe::kernel::XBE_SECTION_HEADER* Section);
    uint32_t __attribute__((__stdcall__)) xbox_XeUnloadSection(xbe::kernel::XBE_SECTION_HEADER* Section);
}

} // namespace kernel
} // namespace xbe
