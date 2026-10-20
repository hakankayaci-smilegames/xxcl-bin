#include "../src/kernel/physical_memory.hpp"

#include <cstdint>
#include <cstdlib>
#include <cstring>

int main() {
    void* allocation = nullptr;
    if (posix_memalign(&allocation, 4096, 0x5000) != 0 || !allocation) return 1;

    auto base = xbe::kernel::physical_memory_register(allocation, 0x5000, 0x4000, 0x20000, 0x3fffffff);
    if (base == 0 || (base & 0x3fff) != 0 || base < 0x20000 || base + 0x5000 - 1 > 0x3fffffff) return 2;
    if (xbe::kernel::physical_memory_from_host(static_cast<char*>(allocation) + 0x234) != base + 0x234) return 3;
    if (xbe::kernel::physical_memory_to_host(base + 0x234, 0x100) != static_cast<char*>(allocation) + 0x234) return 4;
    const auto dma_alias = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(allocation) & 0x0fffffffu);
    if (xbe::kernel::physical_memory_to_host_dma_alias(dma_alias, 0x100) != allocation) return 13;
    if (xbe::kernel::physical_memory_to_host_dma_alias(dma_alias + 0x4ff0, 0x20) != nullptr) return 14;
    if (xbe::kernel::physical_memory_to_host(base + 0x4ff0, 0x20) != nullptr) return 5;

    auto next = xbe::kernel::physical_memory_register(allocation, 0x5000, 0x1000, 0x20000, 0x3fffffff);
    if (next != base) return 6;
    if (!xbe::kernel::physical_memory_unregister(allocation)) return 7;
    if (xbe::kernel::physical_memory_from_host(allocation) != 0) return 8;
    if (xbe::kernel::physical_memory_to_host(base, 1) != nullptr) return 9;

    auto constrained = xbe::kernel::physical_memory_register(allocation, 0x1000, 0x1000, 0x800000, 0x80ffff);
    if (constrained != 0x800000) return 10;
    xbe::kernel::physical_memory_unregister(allocation);

    auto page_physical = xbe::kernel::physical_memory_map_page(static_cast<char*>(allocation) + 0x2210);
    if (page_physical == 0 || xbe::kernel::physical_memory_to_host(page_physical + 0x210, 4) !=
                                   static_cast<char*>(allocation) + 0x2210) return 11;
    if (xbe::kernel::physical_memory_map_page(static_cast<char*>(allocation) + 0x2fff) != page_physical) return 12;
    xbe::kernel::physical_memory_unregister(static_cast<char*>(allocation) + 0x2000);
    std::free(allocation);
    return 0;
}
