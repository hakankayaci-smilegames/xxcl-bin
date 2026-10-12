#include "../src/kernel/apu.hpp"

#include <cstdint>
#include <sys/mman.h>

int main() {
    constexpr uintptr_t base = 0xFE800000;
    void* mapping = mmap(reinterpret_cast<void*>(base), 0x80000,
                         PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    if (mapping == MAP_FAILED) return 1;
    if (!xbe::kernel::apu_initialize()) return 1;

    auto* pio_free = reinterpret_cast<volatile uint32_t*>(base + 0x20010);
    return *pio_free == 0x80 ? 0 : 1;
}
