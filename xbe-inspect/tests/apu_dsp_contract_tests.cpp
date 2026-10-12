#include "../src/kernel/apu_dsp.hpp"
#include "../src/kernel/physical_memory.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

int main() {
    constexpr uint32_t table_physical = 0x00970000;
    constexpr uint32_t scratch_physical = 0x00971000;
    void* table = nullptr;
    void* scratch = nullptr;
    if (posix_memalign(&table, 0x1000, 0x1000) != 0 ||
        posix_memalign(&scratch, 0x1000, 0x2000) != 0) return 1;
    std::memset(table, 0, 0x1000);
    std::memset(scratch, 0, 0x2000);
    if (xbe::kernel::physical_memory_register(table, 0x1000, 0x1000,
                                               table_physical, 0x00970FFF) != table_physical ||
        xbe::kernel::physical_memory_register(scratch, 0x2000, 0x1000,
                                               scratch_physical, 0x00972FFF) != scratch_physical) return 1;

    auto* sge = static_cast<uint32_t*>(table);
    sge[0] = scratch_physical;
    sge[2] = scratch_physical + 0x1000;
    auto* words = static_cast<uint32_t*>(scratch);
    words[0] = 0xAB123456;
    words[0x7FF] = 0xFFABCDEF;

    xbe::kernel::ApuDspCore core;
    if (!core.bootstrap(table_physical, 1)) return 1;
    if (core.program_word(0) != 0x00123456 ||
        core.program_word(0x7FF) != 0x00ABCDEF ||
        core.program_word(0x800) != 0) return 1;

    // The bootstrap must reject a missing second page before changing P memory.
    if (core.bootstrap(table_physical, 0)) return 1;
    if (core.program_word(0) != 0x00123456 || core.program_word(0x7FF) != 0x00ABCDEF) return 1;

    if (core.program_word(xbe::kernel::ApuDspCore::program_word_count) != 0) return 1;

    xbe::kernel::physical_memory_unregister(scratch);
    xbe::kernel::physical_memory_unregister(table);
    std::free(scratch);
    std::free(table);
    return 0;
}
