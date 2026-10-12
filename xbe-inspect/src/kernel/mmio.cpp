#include "mmio.hpp"
#include "apu.hpp"

#include <sys/mman.h>
#include <sys/syscall.h>
#include <cstdlib>
#include <unistd.h>

namespace xbe::kernel {

namespace {

constexpr uintptr_t controller_base = 0xFEC00000;
constexpr size_t controller_page_size = 0x1000;
constexpr uintptr_t apu_base = 0xFE800000;
constexpr size_t apu_region_size = 0x80000;
constexpr uintptr_t controller_ready = controller_base + 0x130;
constexpr uintptr_t command_ack_0 = controller_base + 0x11B;
constexpr uintptr_t command_ack_1 = controller_base + 0x17B;
constexpr uintptr_t page_mask = ~static_cast<uintptr_t>(0xFFF);
thread_local uintptr_t pending_write = 0;
thread_local uintptr_t pending_page = 0;
bool trace_apu_writes = false;

bool is_apu_address(uintptr_t address) {
    return address >= apu_base && address < apu_base + apu_region_size;
}

void trace_apu_write(uintptr_t address) {
    if (!trace_apu_writes) return;

    const uint32_t value = *reinterpret_cast<volatile uint32_t*>(address & ~uintptr_t{3});
    char line[48] = "[APU MMIO] write 0x";
    size_t position = sizeof("[APU MMIO] write 0x") - 1;
    auto append_hex = [&line, &position](uint32_t number) {
        for (int shift = 28; shift >= 0; shift -= 4) {
            const uint32_t digit = (number >> shift) & 0xf;
            line[position++] = static_cast<char>(digit < 10 ? '0' + digit : 'A' + digit - 10);
        }
    };
    append_hex(static_cast<uint32_t>(address));
    line[position++] = ' ';
    line[position++] = '=';
    line[position++] = ' ';
    line[position++] = '0';
    line[position++] = 'x';
    append_hex(value);
    line[position++] = '\n';
    write(STDERR_FILENO, line, position);
}

} // namespace

bool mmio_initialize_interrupt_controller() {
    *reinterpret_cast<volatile uint32_t*>(controller_ready) |= 0x100;
    return mprotect(reinterpret_cast<void*>(controller_base), controller_page_size, PROT_READ) == 0;
}

bool mmio_initialize_apu() {
    const char* trace_setting = std::getenv("XXCL_TRACE_APU");
    trace_apu_writes = trace_setting && trace_setting[0] == '1';
    return mprotect(reinterpret_cast<void*>(apu_base), apu_region_size, PROT_READ) == 0;
}

bool mmio_handle_write_fault(uintptr_t fault_address, ucontext_t* context) {
    const bool controller_write = fault_address >= controller_base &&
                                  fault_address < controller_base + controller_page_size;
    if ((!controller_write && !is_apu_address(fault_address)) || pending_write != 0 ||
        (context->uc_mcontext.gregs[REG_ERR] & 0x2) == 0) {
        return false;
    }
    const uintptr_t page = fault_address & page_mask;
    if (syscall(SYS_mprotect, page, controller_page_size, PROT_READ | PROT_WRITE) != 0) {
        return false;
    }
    pending_write = fault_address;
    pending_page = page;
    context->uc_mcontext.gregs[REG_EFL] |= 0x100; // Single-step the original MMIO write.
    return true;
}

bool mmio_complete_write(ucontext_t* context) {
    if (pending_write == 0) return false;

    if (pending_write == command_ack_0 || pending_write == command_ack_1) {
        *reinterpret_cast<volatile uint8_t*>(pending_write) &= static_cast<uint8_t>(~0x2);
    } else if (is_apu_address(pending_write)) {
        trace_apu_write(pending_write);
        const uint32_t value = *reinterpret_cast<volatile uint32_t*>(pending_write & ~uintptr_t{3});
        if (!apu_queue_mmio_write(pending_write, value)) return false;
    }
    if (syscall(SYS_mprotect, pending_page, controller_page_size, PROT_READ) != 0) return false;
    pending_write = 0;
    pending_page = 0;
    context->uc_mcontext.gregs[REG_EFL] &= ~0x100;
    return true;
}

} // namespace xbe::kernel
