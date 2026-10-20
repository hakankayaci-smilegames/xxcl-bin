#pragma once

#include <cstdint>

namespace xbe::kernel {

constexpr uint32_t guest_dma_memory_start = 0x04000000;
constexpr uint32_t guest_dma_memory_limit = 0x07ffafff;

inline bool find_guest_dma_memory_address(uint32_t size, uint32_t alignment,
                                          uint32_t& address,
                                          uint32_t candidate = guest_dma_memory_start) {
    if (size == 0 || alignment == 0 || (alignment & (alignment - 1)) != 0) return false;
    if (candidate < guest_dma_memory_start) candidate = guest_dma_memory_start;

    const uint64_t mask = alignment - 1;
    const uint64_t aligned = (static_cast<uint64_t>(candidate) + mask) & ~mask;
    if (aligned > guest_dma_memory_limit ||
        static_cast<uint64_t>(size) - 1 > guest_dma_memory_limit - aligned) return false;
    address = static_cast<uint32_t>(aligned);
    return true;
}

} // namespace xbe::kernel
