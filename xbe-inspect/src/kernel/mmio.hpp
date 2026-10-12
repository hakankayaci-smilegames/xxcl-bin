#pragma once

#include <cstdint>
#include <ucontext.h>

namespace xbe::kernel {

bool mmio_initialize_interrupt_controller();
bool mmio_initialize_apu();
bool mmio_handle_write_fault(uintptr_t fault_address, ucontext_t* context);
bool mmio_complete_write(ucontext_t* context);

} // namespace xbe::kernel
