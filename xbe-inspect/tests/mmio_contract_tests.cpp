#include "../src/kernel/mmio.hpp"

#include <cstdint>
#include <csignal>
#include <sys/mman.h>
#include <unistd.h>
#include <ucontext.h>

static void fault_handler(int, siginfo_t* info, void* raw_context) {
    auto* context = static_cast<ucontext_t*>(raw_context);
    if (!xbe::kernel::mmio_handle_write_fault(reinterpret_cast<uintptr_t>(info->si_addr), context)) _exit(2);
}

static void trap_handler(int, siginfo_t*, void* raw_context) {
    if (!xbe::kernel::mmio_complete_write(static_cast<ucontext_t*>(raw_context))) _exit(3);
}

int main() {
    constexpr uintptr_t base = 0xFEC00000;
    void* page = mmap(reinterpret_cast<void*>(base), 0x1000, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    if (page == MAP_FAILED) return 1;

    struct sigaction action{};
    action.sa_flags = SA_SIGINFO;
    sigemptyset(&action.sa_mask);
    action.sa_sigaction = fault_handler;
    if (sigaction(SIGSEGV, &action, nullptr) != 0) return 1;
    action.sa_sigaction = trap_handler;
    if (sigaction(SIGTRAP, &action, nullptr) != 0) return 1;

    if (!xbe::kernel::mmio_initialize_interrupt_controller()) return 1;
    auto* status = reinterpret_cast<volatile uint32_t*>(base + 0x130);
    if ((*status & 0x100) == 0) return 1;

    auto* command0 = reinterpret_cast<volatile uint8_t*>(base + 0x11B);
    auto* command1 = reinterpret_cast<volatile uint8_t*>(base + 0x17B);
    *command0 = 2;
    if ((*command0 & 2) != 0) return 1;
    *command1 = 2;
    if ((*command1 & 2) != 0) return 1;

    auto* ordinary_register = reinterpret_cast<volatile uint32_t*>(base + 0x12C);
    *ordinary_register = 0x1234;
    if (*ordinary_register != 0x1234) return 1;
    return 0;
}
