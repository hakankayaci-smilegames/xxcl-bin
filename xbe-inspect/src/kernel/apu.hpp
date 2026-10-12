#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace xbe::kernel {

// Initialize register defaults for the memory-mapped APU device.
bool apu_initialize();
bool apu_queue_mmio_write(uintptr_t address, uint32_t value);
size_t apu_process_mmio_writes(size_t max_writes = static_cast<size_t>(-1));
// Run up to instruction_budget GP DSP instructions; execution stops at the
// first unsupported instruction so missing firmware support stays observable.
size_t apu_run_gp_dsp(uint32_t instruction_budget);
bool apu_start_worker();
void apu_stop_worker();

// Copy between an APU scratch buffer and guest pages described by an SGE table.
// max_sge is the inclusive final SGE index, matching the MCPX register.
bool apu_sge_transfer(uint32_t sge_table_physical, uint32_t max_sge,
                      uint32_t scratch_offset, std::span<std::byte> buffer,
                      bool write_to_guest);
bool apu_gp_program_read(uint32_t address, uint32_t& value);

} // namespace xbe::kernel
