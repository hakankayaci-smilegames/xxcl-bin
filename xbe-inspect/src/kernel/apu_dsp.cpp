#include "apu_dsp.hpp"

#include "apu.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>

namespace xbe::kernel {
namespace {

constexpr uint32_t dsp_word_mask = 0x00FFFFFF;

uint32_t load_le_word(const std::byte* bytes) {
    return std::to_integer<uint32_t>(bytes[0]) |
           (std::to_integer<uint32_t>(bytes[1]) << 8) |
           (std::to_integer<uint32_t>(bytes[2]) << 16) |
           (std::to_integer<uint32_t>(bytes[3]) << 24);
}

} // namespace

bool ApuDspCore::bootstrap(uint32_t sge_table_physical, uint32_t max_sge) {
    constexpr size_t scratch_size = bootstrap_word_count * sizeof(uint32_t);
    std::array<std::byte, scratch_size> scratch{};
    if (!apu_sge_transfer(sge_table_physical, max_sge, 0, scratch, false)) return false;

    std::array<uint32_t, bootstrap_word_count> loaded_program{};
    for (uint32_t index = 0; index < bootstrap_word_count; ++index) {
        loaded_program[index] = load_le_word(scratch.data() + index * sizeof(uint32_t)) & dsp_word_mask;
    }
    std::copy(loaded_program.begin(), loaded_program.end(), program_.begin());
    return true;
}

uint32_t ApuDspCore::program_word(uint32_t address) const {
    if (address >= program_word_count) return 0;
    return program_[address] & dsp_word_mask;
}

} // namespace xbe::kernel
