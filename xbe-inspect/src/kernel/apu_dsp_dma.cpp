#include "apu_dsp_dma.hpp"

namespace xbe::kernel {

void ApuDspDma::reset() {
    configuration_ = 0;
    control_ = 0;
    start_block_ = 0;
    next_block_ = 0;
    control_read_count_ = 0;
}

uint32_t ApuDspDma::read(Register reg) {
    switch (reg) {
    case Register::configuration: return configuration_;
    case Register::control:
        if ((control_ & control_running) != 0) {
            if (++control_read_count_ > 2) {
                control_ &= ~control_running;
                control_ |= control_stopped;
                control_read_count_ = 0;
            }
        }
        return control_;
    case Register::start_block: return start_block_;
    case Register::next_block: return next_block_;
    }
    return 0;
}

void ApuDspDma::write(Register reg, uint32_t value) {
    switch (reg) {
    case Register::configuration:
        configuration_ = value;
        break;
    case Register::control:
        switch (value & action_mask) {
        case action_start:
            control_ |= control_running;
            control_ &= ~control_stopped;
            control_read_count_ = 0;
            break;
        case action_stop:
            control_ |= control_stopped;
            control_ &= ~control_running;
            control_read_count_ = 0;
            break;
        case action_freeze:
            control_ |= control_frozen;
            break;
        case action_unfreeze:
            control_ &= ~control_frozen;
            break;
        case action_abort:
            control_ &= ~control_running;
            control_ |= control_stopped;
            control_read_count_ = 0;
            break;
        default:
            break;
        }
        break;
    case Register::start_block:
        start_block_ = value;
        break;
    case Register::next_block:
        next_block_ = value;
        break;
    }
}

} // namespace xbe::kernel
