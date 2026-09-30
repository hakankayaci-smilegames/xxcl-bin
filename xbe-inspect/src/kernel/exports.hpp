#pragma once
#include <cstdint>

namespace xbe {
namespace kernel {

// Get the actual function pointer for an Xbox kernel ordinal
void* get_kernel_export(uint32_t ordinal);

} // namespace kernel
} // namespace xbe
