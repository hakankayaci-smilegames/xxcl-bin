#pragma once

#include <array>
#include <cstdint>

namespace xbe::kernel {

class ApuDspCore {
public:
    static constexpr uint32_t program_word_count = 0x1000;
    static constexpr uint32_t bootstrap_word_count = 0x800;

    bool bootstrap(uint32_t sge_table_physical, uint32_t max_sge);
    uint32_t program_word(uint32_t address) const;

private:
    std::array<uint32_t, program_word_count> program_{};
};

} // namespace xbe::kernel
