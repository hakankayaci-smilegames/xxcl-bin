#include "../src/kernel/apu.hpp"
#include "../src/kernel/physical_memory.hpp"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <sys/mman.h>

int main() {
    constexpr uintptr_t base = 0xFE800000;
    constexpr uint32_t sge_physical = 0x00940000;
    void* sge_table = nullptr;
    if (posix_memalign(&sge_table, 0x4000, 0x4000) != 0) return 1;
    std::memset(sge_table, 0, 0x4000);
    if (xbe::kernel::physical_memory_register(sge_table, 0x4000, 0x4000,
                                               sge_physical, 0x0094FFFF) != sge_physical) return 1;

    void* mapping = mmap(reinterpret_cast<void*>(base), 0x80000,
                         PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    if (mapping == MAP_FAILED) return 1;
    if (!xbe::kernel::apu_initialize()) return 1;

    auto* pio_free = reinterpret_cast<volatile uint32_t*>(base + 0x20010);
    if (*pio_free != 0x80) return 1;

    auto* registers = reinterpret_cast<volatile uint32_t*>(base);
    registers[0x2030 / 4] = sge_physical;
    if (!xbe::kernel::apu_queue_mmio_write(base + 0x2030, sge_physical)) return 1;
    if (!xbe::kernel::apu_queue_mmio_write(base + 0x20804, 0x7FF)) return 1;
    if (!xbe::kernel::apu_queue_mmio_write(base + 0x20808, 0x00964003)) return 1;
    if (!xbe::kernel::apu_queue_mmio_write(base + 0x21800, 1)) return 1;
    if (!xbe::kernel::apu_queue_mmio_write(base + 0x21808, 0x009260F3)) return 1;
    if (xbe::kernel::apu_process_mmio_writes() != 5) return 1;

    auto* entries = static_cast<volatile uint32_t*>(sge_table);
    if (entries[0x7FF * 2] != 0x00964000) return 1;
    if (entries[1 * 2] != 0x00926000) return 1;

    if (!xbe::kernel::apu_queue_mmio_write(base + 0x20804, 0x8000)) return 1;
    if (!xbe::kernel::apu_queue_mmio_write(base + 0x20808, 0x0096A000)) return 1;
    if (xbe::kernel::apu_process_mmio_writes() != 2) return 1;
    if (entries[0x7FF * 2] != 0x00964000) return 1;

    xbe::kernel::physical_memory_unregister(sge_table);
    std::free(sge_table);
    return 0;
}
