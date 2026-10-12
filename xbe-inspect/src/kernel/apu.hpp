#pragma once

#include <cstddef>
#include <cstdint>

namespace xbe::kernel {

// Initialize register defaults for the memory-mapped APU device.
bool apu_initialize();
bool apu_queue_mmio_write(uintptr_t address, uint32_t value);
size_t apu_process_mmio_writes();

} // namespace xbe::kernel
