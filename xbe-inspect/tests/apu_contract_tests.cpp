#include "../src/kernel/apu.hpp"
#include "../src/kernel/physical_memory.hpp"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <array>
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

    constexpr uint32_t dma_table_physical = 0x00950000;
    constexpr uint32_t dma_pages_physical = 0x00960000;
    void* dma_table = nullptr;
    void* dma_pages = nullptr;
    if (posix_memalign(&dma_table, 0x1000, 0x1000) != 0 ||
        posix_memalign(&dma_pages, 0x1000, 0x2000) != 0) return 1;
    std::memset(dma_table, 0, 0x1000);
    std::memset(dma_pages, 0, 0x2000);
    if (xbe::kernel::physical_memory_register(dma_table, 0x1000, 0x1000,
                                               dma_table_physical, 0x00950FFF) != dma_table_physical ||
        xbe::kernel::physical_memory_register(dma_pages, 0x2000, 0x1000,
                                               dma_pages_physical, 0x00961FFF) != dma_pages_physical) return 1;
    auto* dma_entries = static_cast<uint32_t*>(dma_table);
    dma_entries[0] = dma_pages_physical;
    dma_entries[2] = dma_pages_physical + 0x1000;
    auto* guest_bytes = static_cast<uint8_t*>(dma_pages);
    guest_bytes[0xFFE] = 0x11;
    guest_bytes[0xFFF] = 0x22;
    guest_bytes[0x1000] = 0x33;
    guest_bytes[0x1001] = 0x44;

    std::array<std::byte, 4> transfer{};
    if (!xbe::kernel::apu_sge_transfer(dma_table_physical, 1, 0xFFE, transfer, false)) return 1;
    if (transfer != std::array<std::byte, 4>{std::byte{0x11}, std::byte{0x22},
                                             std::byte{0x33}, std::byte{0x44}}) return 1;
    transfer = {std::byte{0xA1}, std::byte{0xA2}, std::byte{0xA3}, std::byte{0xA4}};
    if (!xbe::kernel::apu_sge_transfer(dma_table_physical, 1, 0xFFE, transfer, true)) return 1;
    if (guest_bytes[0xFFE] != 0xA1 || guest_bytes[0xFFF] != 0xA2 ||
        guest_bytes[0x1000] != 0xA3 || guest_bytes[0x1001] != 0xA4) return 1;
    transfer.fill(std::byte{0x55});
    if (xbe::kernel::apu_sge_transfer(dma_table_physical, 0, 0xFFE, transfer, false)) return 1;
    if (transfer != std::array<std::byte, 4>{std::byte{0x55}, std::byte{0x55},
                                             std::byte{0x55}, std::byte{0x55}}) return 1;
    if (xbe::kernel::apu_sge_transfer(dma_table_physical, 0, 0xFFE, transfer, true)) return 1;
    if (guest_bytes[0xFFE] != 0xA1 || guest_bytes[0xFFF] != 0xA2 ||
        guest_bytes[0x1000] != 0xA3 || guest_bytes[0x1001] != 0xA4) return 1;

    const uint32_t bootstrap_words[] = {0xAB123456, 0xFFABCDEF};
    std::memcpy(guest_bytes, bootstrap_words, sizeof(bootstrap_words));
    if (!xbe::kernel::apu_queue_mmio_write(base + 0x2040, dma_table_physical) ||
        !xbe::kernel::apu_queue_mmio_write(base + 0x20D4, 1) ||
        !xbe::kernel::apu_queue_mmio_write(base + 0x3FFFC, 0) ||
        !xbe::kernel::apu_queue_mmio_write(base + 0x3FFFC, 1) ||
        !xbe::kernel::apu_queue_mmio_write(base + 0x3FFFC, 3)) return 1;
    if (xbe::kernel::apu_process_mmio_writes() != 5) return 1;
    uint32_t dsp_word = 0;
    if (!xbe::kernel::apu_gp_program_read(0, dsp_word) || dsp_word != 0x00123456) return 1;
    if (!xbe::kernel::apu_gp_program_read(1, dsp_word) || dsp_word != 0x00ABCDEF) return 1;
    if (xbe::kernel::apu_gp_program_read(0x1000, dsp_word)) return 1;

    xbe::kernel::physical_memory_unregister(dma_pages);
    xbe::kernel::physical_memory_unregister(dma_table);
    std::free(dma_pages);
    std::free(dma_table);

    xbe::kernel::physical_memory_unregister(sge_table);
    std::free(sge_table);
    return 0;
}
