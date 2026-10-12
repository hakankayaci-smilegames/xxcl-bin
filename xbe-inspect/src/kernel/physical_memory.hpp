#pragma once

#include <cstddef>
#include <cstdint>

namespace xbe::kernel {

uint32_t physical_memory_register(void* host_address, size_t size, size_t alignment,
                                  uint32_t lowest_address, uint32_t highest_address);
uint32_t physical_memory_map_page(void* host_address);
uint32_t physical_memory_from_host(const void* host_address);
void* physical_memory_to_host(uint32_t physical_address, size_t size = 1);
bool physical_memory_unregister(void* host_address);

} // namespace xbe::kernel
