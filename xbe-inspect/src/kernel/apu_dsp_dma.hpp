#pragma once

#include <cstdint>

namespace xbe::kernel {

// MCPX GP/EP DSP DMA register state, independent from instruction execution.
class ApuDspDma {
public:
    enum class Register : uint8_t {
        configuration,
        control,
        start_block,
        next_block,
    };

    static constexpr uint32_t action_mask = 0x7;
    static constexpr uint32_t action_start = 1;
    static constexpr uint32_t action_stop = 2;
    static constexpr uint32_t action_freeze = 3;
    static constexpr uint32_t action_unfreeze = 4;
    static constexpr uint32_t action_abort = 5;
    static constexpr uint32_t control_frozen = 1u << 3;
    static constexpr uint32_t control_running = 1u << 4;
    static constexpr uint32_t control_stopped = 1u << 5;

    void reset();
    uint32_t read(Register reg);
    void write(Register reg, uint32_t value);

private:
    uint32_t configuration_ = 0;
    uint32_t control_ = 0;
    uint32_t start_block_ = 0;
    uint32_t next_block_ = 0;
    uint32_t control_read_count_ = 0;
};

} // namespace xbe::kernel
