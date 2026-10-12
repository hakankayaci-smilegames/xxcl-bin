#pragma once

#include <array>
#include <cstdint>

#include "apu_dsp_dma.hpp"

namespace xbe::kernel {

class ApuDspCore {
public:
    enum class StepResult {
        executed,
        unsupported_instruction,
        invalid_program_counter,
    };

    static constexpr uint32_t program_word_count = 0x1000;
    static constexpr uint32_t bootstrap_word_count = 0x800;

    bool bootstrap(uint32_t sge_table_physical, uint32_t max_sge);
    uint32_t program_word(uint32_t address) const;
    uint32_t peripheral_word(bool y_space, uint8_t address);
    uint32_t register_word(uint8_t index) const;
    uint32_t data_word(bool y_space, uint16_t address) const;
    uint32_t program_counter() const { return program_counter_; }
    void start_frame();
    StepResult step();

private:
    struct DoLoop {
        uint32_t start = 0;
        uint32_t end = 0;
        uint32_t remaining = 0;
    };

    void advance_program_counter(uint32_t next_address);
    void write_peripheral_word(bool y_space, uint8_t address, uint32_t value);
    uint32_t read_data_register(uint8_t index) const;
    void write_data_register(uint8_t index, uint32_t value);

    std::array<uint32_t, program_word_count> program_{};
    std::array<uint32_t, 16> return_stack_{};
    std::array<uint32_t, 64> x_peripherals_{};
    std::array<uint32_t, 64> y_peripherals_{};
    ApuDspDma dma_;
    uint32_t interrupts_ = 0;
    std::array<uint32_t, 64> registers_{};
    std::array<uint32_t, 0x10000> x_memory_{};
    std::array<uint32_t, 0x10000> y_memory_{};
    uint32_t program_counter_ = 0;
    uint32_t stack_depth_ = 0;
    uint32_t repeat_address_ = 0;
    uint32_t repeat_remaining_ = 0;
    bool repeat_active_ = false;
    std::array<DoLoop, 16> do_loops_{};
    uint32_t do_loop_depth_ = 0;
};

} // namespace xbe::kernel
