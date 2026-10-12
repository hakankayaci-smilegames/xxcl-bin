#include "mmio.hpp"

#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace xbe::kernel {

namespace {

constexpr uintptr_t controller_base = 0xFEC00000;
constexpr size_t controller_page_size = 0x1000;
constexpr uintptr_t controller_ready = controller_base + 0x130;
constexpr uintptr_t command_ack_0 = controller_base + 0x11B;
constexpr uintptr_t command_ack_1 = controller_base + 0x17B;
thread_local uintptr_t pending_write = 0;

} // namespace

bool mmio_initialize_interrupt_controller() {
    *reinterpret_cast<volatile uint32_t*>(controller_ready) |= 0x100;
    return mprotect(reinterpret_cast<void*>(controller_base), controller_page_size, PROT_READ) == 0;
}

bool mmio_handle_write_fault(uintptr_t fault_address, ucontext_t* context) {
    if (fault_address < controller_base || fault_address >= controller_base + controller_page_size ||
        pending_write != 0 || (context->uc_mcontext.gregs[REG_ERR] & 0x2) == 0) {
        return false;
    }
    if (syscall(SYS_mprotect, controller_base, controller_page_size, PROT_READ | PROT_WRITE) != 0) {
        return false;
    }
    pending_write = fault_address;
    context->uc_mcontext.gregs[REG_EFL] |= 0x100; // Single-step the original MMIO write.
    return true;
}

bool mmio_complete_write(ucontext_t* context) {
    if (pending_write == 0) return false;

    if (pending_write == command_ack_0 || pending_write == command_ack_1) {
        *reinterpret_cast<volatile uint8_t*>(pending_write) &= static_cast<uint8_t>(~0x2);
    }
    if (syscall(SYS_mprotect, controller_base, controller_page_size, PROT_READ) != 0) return false;
    pending_write = 0;
    context->uc_mcontext.gregs[REG_EFL] &= ~0x100;
    return true;
}

} // namespace xbe::kernel
