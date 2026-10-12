#include "apu_dsp.hpp"

#include "apu.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <span>

namespace xbe::kernel {
namespace {

constexpr uint32_t dsp_word_mask = 0x00FFFFFF;
constexpr std::array<uint8_t, 64> register_widths = {
    0, 0, 0, 0, 24, 24, 24, 24, 24, 24, 8, 8, 24, 24, 24, 24,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 16, 8, 6, 16, 16, 16, 16,
};

uint32_t load_le_word(const std::byte* bytes) {
    return std::to_integer<uint32_t>(bytes[0]) |
           (std::to_integer<uint32_t>(bytes[1]) << 8) |
           (std::to_integer<uint32_t>(bytes[2]) << 16) |
           (std::to_integer<uint32_t>(bytes[3]) << 24);
}

ApuDspDma::Register dma_register_for_peripheral(uint8_t address) {
    switch (address) {
    case 0x14: return ApuDspDma::Register::next_block; // X:$D4
    case 0x15: return ApuDspDma::Register::start_block; // X:$D5
    case 0x16: return ApuDspDma::Register::control; // X:$D6
    default: return ApuDspDma::Register::configuration; // X:$D7
    }
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
    sge_table_physical_ = sge_table_physical;
    max_sge_ = max_sge;
    x_peripherals_.fill(0);
    y_peripherals_.fill(0);
    dma_.reset();
    interrupts_ = 0;
    registers_.fill(0);
    for (uint32_t index = 32; index < 40; ++index) registers_[index] = 0xFFFF;
    program_counter_ = 0;
    stack_depth_ = 0;
    repeat_address_ = 0;
    repeat_remaining_ = 0;
    repeat_active_ = false;
    do_loop_depth_ = 0;
    do_loops_.fill({});
    return true;
}

uint32_t ApuDspCore::program_word(uint32_t address) const {
    if (address >= program_word_count) return 0;
    return program_[address] & dsp_word_mask;
}

uint32_t ApuDspCore::peripheral_word(bool y_space, uint8_t address) {
    address &= 0x3F;
    if (!y_space && address == 0x05) return interrupts_;
    if (!y_space && address >= 0x14 && address <= 0x17) {
        const auto reg = dma_register_for_peripheral(address);
        const uint32_t value = dma_.read(reg);
        if (address == 0x16 && std::getenv("XXCL_TRACE_APU")) {
            static uint32_t logged_control_reads = 0;
            if (logged_control_reads++ < 8) {
                std::fprintf(stderr, "[APU DSP DMA] control read = 0x%06X\n", value);
            }
        }
        return value;
    }
    return (y_space ? y_peripherals_ : x_peripherals_)[address];
}

void ApuDspCore::write_peripheral_word(bool y_space, uint8_t address, uint32_t value) {
    address &= 0x3F;
    if (!y_space && address == 0x05) {
        interrupts_ &= ~value;
        return;
    }
    if (!y_space && address >= 0x14 && address <= 0x17) {
        const auto reg = dma_register_for_peripheral(address);
        dma_.write(reg, value);
        if (address == 0x16 && (value & ApuDspDma::action_mask) ==
                                   ApuDspDma::action_start) {
            const bool completed = dma_.run_descriptors(
                x_memory_, y_memory_, program_, transfer_dma_buffer, this);
            if (completed && dma_.end_of_list()) interrupts_ |= 1u << 7;
            if (std::getenv("XXCL_TRACE_APU")) {
                std::fprintf(stderr,
                             "[APU DSP DMA] descriptor run %s; next=0x%06X EOL=%u\n",
                             completed ? "completed" : "stopped",
                             dma_.read(ApuDspDma::Register::next_block),
                             dma_.end_of_list() ? 1u : 0u);
            }
        }
        if (std::getenv("XXCL_TRACE_APU")) {
            std::fprintf(stderr, "[APU DSP DMA] register X:$%02X write = 0x%06X\n",
                         address, value);
        }
        return;
    }
    (y_space ? y_peripherals_ : x_peripherals_)[address] = value;
}

bool ApuDspCore::transfer_dma_buffer(void* context, uint32_t buffer_id,
                                    uint32_t offset, uint32_t ring_size,
                                    std::span<std::byte> bytes,
                                    bool write_to_buffer) {
    auto& core = *static_cast<ApuDspCore*>(context);
    (void)ring_size;
    if (buffer_id != 0xE && buffer_id != 0xF) return false;
    return apu_sge_transfer(core.sge_table_physical_, core.max_sge_, offset,
                            bytes, write_to_buffer);
}

void ApuDspCore::start_frame() {
    interrupts_ |= 1u << 1;
}

uint32_t ApuDspCore::register_word(uint8_t index) const {
    return registers_[index & 0x3F];
}

uint32_t ApuDspCore::read_data_register(uint8_t index) const {
    index &= 0x3F;
    if (index != 14 && index != 15) return registers_[index];

    const uint32_t base = index == 14 ? 8 : 9;
    const uint64_t accumulator = (uint64_t{registers_[base + 2] & 0xFF} << 24) |
                                 (registers_[base + 4] & dsp_word_mask);
    const uint32_t scale = (registers_[57] >> 10) & 3;
    uint64_t value = accumulator;
    if (scale == 1) value >>= 1;
    else if (scale == 2) value = (value << 1) | ((registers_[base] >> 23) & 1);
    return static_cast<uint32_t>(value) & dsp_word_mask;
}

void ApuDspCore::write_data_register(uint8_t index, uint32_t value) {
    index &= 0x3F;
    value &= dsp_word_mask;
    if (index != 14 && index != 15) {
        const uint8_t width = register_widths[index];
        registers_[index] = width == 0 || width >= 24
            ? value
            : value & ((uint32_t{1} << width) - 1);
        return;
    }

    const uint32_t base = index == 14 ? 8 : 9;
    registers_[base] = 0;
    registers_[base + 4] = value;
    registers_[base + 2] = (value & (1u << 23)) ? 0xFF : 0;
}

uint32_t ApuDspCore::data_word(bool y_space, uint16_t address) const {
    return (y_space ? y_memory_ : x_memory_)[address];
}

void ApuDspCore::advance_program_counter(uint32_t next_address) {
    if (repeat_active_ && program_counter_ == repeat_address_) {
        if (repeat_remaining_ > 1) {
            --repeat_remaining_;
            program_counter_ = repeat_address_;
            return;
        }
        repeat_active_ = false;
        repeat_remaining_ = 0;
    }
    while (do_loop_depth_ != 0 && next_address == do_loops_[do_loop_depth_ - 1].end) {
        DoLoop& loop = do_loops_[do_loop_depth_ - 1];
        if (loop.remaining > 1) {
            --loop.remaining;
            next_address = loop.start;
            break;
        }
        --do_loop_depth_;
    }
    program_counter_ = next_address;
}

ApuDspCore::StepResult ApuDspCore::step() {
    if (program_counter_ >= program_word_count) {
        return StepResult::invalid_program_counter;
    }

    const uint32_t instruction = program_[program_counter_] & dsp_word_mask;
    if (instruction == 0x000D1080) { // BSR xxxx
        if (program_counter_ + 1 >= program_word_count ||
            stack_depth_ >= return_stack_.size()) {
            return StepResult::unsupported_instruction;
        }
        const uint32_t target = (program_counter_ +
            (program_[program_counter_ + 1] & 0xFFFF)) & 0x0FFF;
        return_stack_[stack_depth_++] = program_counter_ + 2;
        program_counter_ = target;
        return StepResult::executed;
    }

    constexpr uint32_t bsr_short_mask = 0x00FFFC20;
    constexpr uint32_t bsr_short_value = 0x00050800;
    if ((instruction & bsr_short_mask) == bsr_short_value) { // BSR xxx
        if (stack_depth_ >= return_stack_.size()) {
            return StepResult::unsupported_instruction;
        }
        const uint32_t encoded_displacement = (instruction & 0x1F) +
            ((instruction & 0x3C0) >> 1);
        const int32_t displacement = (encoded_displacement & 0x100) != 0
            ? static_cast<int32_t>(encoded_displacement) - 0x200
            : static_cast<int32_t>(encoded_displacement);
        return_stack_[stack_depth_++] = program_counter_ + 1;
        program_counter_ = static_cast<uint32_t>(
            static_cast<int32_t>(program_counter_) + displacement) & 0x0FFF;
        return StepResult::executed;
    }

    constexpr uint32_t peripheral_branch_mask = 0x00FFC0A0;
    const uint32_t peripheral_branch = instruction & peripheral_branch_mask;
    if (peripheral_branch == 0x000CC000 || peripheral_branch == 0x000CC020) {
        if (program_counter_ + 1 >= program_word_count) {
            return StepResult::unsupported_instruction;
        }
        const bool y_space = (instruction & (1u << 6)) != 0;
        const uint32_t address = (instruction >> 8) & 0x3F;
        const uint32_t bit = instruction & 0x1F;
        const uint32_t value = peripheral_word(y_space, static_cast<uint8_t>(address));
        const bool is_set = (value & (1u << bit)) != 0;
        const bool branch = peripheral_branch == 0x000CC020 ? is_set : !is_set;
        const uint32_t offset = program_[program_counter_ + 1] & 0xFFFF;
        advance_program_counter(branch
            ? (program_counter_ + offset) & 0x0FFF
            : program_counter_ + 2);
        return StepResult::executed;
    }

    // JCLR/JSET #n,X/Y:$pp,P:xxxx polls an on-chip peripheral bit and uses
    // an absolute P-space target when the condition is satisfied.
    constexpr uint32_t peripheral_jump_mask = 0x00FFC0B0;
    constexpr uint32_t jclr_peripheral_value = 0x000A8080;
    constexpr uint32_t jset_peripheral_value = 0x000A8090;
    const uint32_t peripheral_jump = instruction & peripheral_jump_mask;
    if (peripheral_jump == jclr_peripheral_value ||
        peripheral_jump == jset_peripheral_value) {
        if (program_counter_ + 1 >= program_word_count) {
            return StepResult::unsupported_instruction;
        }
        const bool y_space = (instruction & (1u << 6)) != 0;
        const uint32_t address = (instruction >> 8) & 0x3F;
        const uint32_t bit = instruction & 0xF;
        const bool is_set = (peripheral_word(y_space, static_cast<uint8_t>(address)) &
                             (1u << bit)) != 0;
        const bool branch = peripheral_jump == jset_peripheral_value ? is_set : !is_set;
        const uint32_t target = program_[program_counter_ + 1] & 0x0FFF;
        advance_program_counter(branch ? target : program_counter_ + 2);
        return StepResult::executed;
    }

    // MOVEP X/Y:$pp <-> DSP register. The observed GP firmware uses this form
    // to program DMA start/next-block registers from its internal state.
    constexpr uint32_t movep_register_mask = 0x00FE40C0;
    constexpr uint32_t movep_register_value = 0x00084000;
    if ((instruction & movep_register_mask) == movep_register_value) {
        const bool write_peripheral = (instruction & (1u << 15)) != 0;
        const bool y_space = (instruction & (1u << 16)) != 0;
        const uint32_t register_index = (instruction >> 8) & 0x3F;
        const uint8_t address = static_cast<uint8_t>(instruction & 0x3F);
        if (write_peripheral) {
            write_peripheral_word(y_space, address, read_data_register(register_index));
        } else {
            write_data_register(static_cast<uint8_t>(register_index),
                                peripheral_word(y_space, address));
        }
        ++program_counter_;
        return StepResult::executed;
    }

    // MOVEP #xxxxxx,X/Y:$pp uses the absolute-address EA with a nonzero
    // address-register field to encode an immediate peripheral write.
    constexpr uint32_t movep_ea_peripheral_mask = 0x00FF4080;
    constexpr uint32_t movep_ea_peripheral_value = 0x00074000;
    if ((instruction & movep_ea_peripheral_mask) == movep_ea_peripheral_value) {
        const bool write_peripheral = (instruction & (1u << 15)) != 0;
        const bool peripheral_y_space = (instruction & (1u << 6)) != 0;
        const uint32_t effective_address = (instruction >> 8) & 0x3F;
        const uint32_t mode = effective_address >> 3;
        const uint32_t address_register = effective_address & 7;
        const uint8_t peripheral_address = static_cast<uint8_t>(instruction & 0x3F);
        if (mode != 6 || address_register == 0 || !write_peripheral ||
            program_counter_ + 1 >= program_word_count) {
            return StepResult::unsupported_instruction;
        }
        write_peripheral_word(peripheral_y_space, peripheral_address,
                              program_[program_counter_ + 1] & dsp_word_mask);
        advance_program_counter(program_counter_ + 2);
        return StepResult::executed;
    }

    constexpr uint32_t jsr_variable_bits = 0x003F00;
    constexpr uint32_t jsr_fixed_bits = dsp_word_mask & ~jsr_variable_bits;
    if ((instruction & jsr_fixed_bits) == (0x000BF080 & jsr_fixed_bits)) {
        const uint32_t effective_address = (instruction >> 8) & 0x3F;
        // The firmware's observed JSR uses the absolute (aa) addressing mode.
        if (effective_address != 0x30 || program_counter_ + 1 >= program_word_count ||
            stack_depth_ >= return_stack_.size()) {
            return StepResult::unsupported_instruction;
        }
        const uint32_t target = program_[program_counter_ + 1] & 0x0FFF;
        return_stack_[stack_depth_++] = program_counter_ + 2;
        program_counter_ = target;
        return StepResult::executed;
    }

    // MOVEP immediate -> X/Y peripheral register, used by the observed GP
    // firmware to initialize its internal control registers.
    constexpr uint32_t movep_fixed_mask = 0x00FE4080;
    constexpr uint32_t movep_fixed_value = 0x00084080;
    if ((instruction & movep_fixed_mask) == movep_fixed_value) {
        const uint32_t effective_address = (instruction >> 8) & 0x3F;
        const bool write_peripheral = (instruction & (1u << 15)) != 0;
        if (!write_peripheral || ((effective_address >> 3) != 6) ||
            (effective_address & 7) == 0 || program_counter_ + 1 >= program_word_count) {
            return StepResult::unsupported_instruction;
        }
        const bool y_space = (instruction & (1u << 16)) != 0;
        const uint32_t address = instruction & 0x3F;
        const uint32_t value = program_[program_counter_ + 1] & dsp_word_mask;
        write_peripheral_word(y_space, static_cast<uint8_t>(address), value);
        advance_program_counter(program_counter_ + 2);
        return StepResult::executed;
    }

    // REP S repeats the following instruction the selected register's count
    // (zero represents 65,536 iterations on the DSP56300).
    constexpr uint32_t rep_register_variable_bits = 0x003F00;
    constexpr uint32_t rep_register_fixed_bits = dsp_word_mask & ~rep_register_variable_bits;
    if ((instruction & rep_register_fixed_bits) ==
        (0x0006C420 & rep_register_fixed_bits)) {
        const uint32_t source_register = (instruction >> 8) & 0x3F;
        repeat_remaining_ = registers_[source_register] & 0xFFFF;
        if (repeat_remaining_ == 0) repeat_remaining_ = 0x10000;
        repeat_address_ = program_counter_ + 1;
        repeat_active_ = true;
        ++program_counter_;
        return StepResult::executed;
    }

    // DOR #count,label establishes a counted DO loop over subsequent words.
    constexpr uint32_t dor_fixed_mask = 0x00FF00F0;
    constexpr uint32_t dor_fixed_value = 0x00060090;
    if ((instruction & dor_fixed_mask) == dor_fixed_value) {
        if (program_counter_ + 1 >= program_word_count ||
            do_loop_depth_ >= do_loops_.size()) {
            return StepResult::unsupported_instruction;
        }
        uint32_t count = ((instruction >> 8) & 0xFF) + ((instruction & 0xF) << 8);
        if (count == 0) count = 0x10000;
        const uint32_t relative_end = program_[program_counter_ + 1] & 0xFFFF;
        const uint32_t start = program_counter_ + 2;
        const uint32_t end = (program_counter_ + relative_end + 1) & 0xFFFF;
        if (end <= start) return StepResult::unsupported_instruction;
        do_loops_[do_loop_depth_++] = {start, end, count};
        program_counter_ = start;
        return StepResult::executed;
    }

    // AND #xxxx,D on the accumulator's 24-bit middle word.
    constexpr uint32_t and_long_variable_bits = 1u << 3;
    constexpr uint32_t and_long_fixed_bits = dsp_word_mask & ~and_long_variable_bits;
    if ((instruction & and_long_fixed_bits) == (0x00140C6 & and_long_fixed_bits)) {
        if (program_counter_ + 1 >= program_word_count) {
            return StepResult::unsupported_instruction;
        }
        const uint32_t destination = (instruction >> 3) & 1;
        const uint32_t accumulator = destination == 0 ? 12 : 13;
        const uint32_t immediate = program_[program_counter_ + 1] & dsp_word_mask;
        registers_[accumulator] = (registers_[accumulator] & immediate) & dsp_word_mask;
        constexpr uint32_t sr_v = 1u << 1;
        constexpr uint32_t sr_z = 1u << 2;
        constexpr uint32_t sr_n = 1u << 3;
        registers_[57] &= 0xFFFFu & ~(sr_n | sr_z | sr_v);
        registers_[57] |= ((registers_[accumulator] >> 23) & 1) * sr_n;
        if (registers_[accumulator] == 0) registers_[57] |= sr_z;
        advance_program_counter(program_counter_ + 2);
        return StepResult::executed;
    }

    // ADD #xxxx,D updates the selected 56-bit accumulator and DSP condition
    // flags. The immediate is a signed 24-bit operand.
    if ((instruction & and_long_fixed_bits) == (0x00140C0 & and_long_fixed_bits)) {
        if (program_counter_ + 1 >= program_word_count) {
            return StepResult::unsupported_instruction;
        }
        const uint32_t destination = (instruction >> 3) & 1;
        const uint32_t a0 = destination == 0 ? 8 : 9;
        const uint32_t a1 = destination == 0 ? 12 : 13;
        const uint32_t a2 = destination == 0 ? 10 : 11;
        const uint32_t immediate = program_[program_counter_ + 1] & dsp_word_mask;
        const bool negative_source = (immediate & 0x00800000) != 0;
        const uint32_t source_top = negative_source ? 0xFF : 0;
        const uint32_t source_mid = negative_source ? 0xFFFFFF : 0;
        const uint64_t low_sum = uint64_t{registers_[a0]} + immediate;
        const uint64_t mid_sum = uint64_t{registers_[a1]} + source_mid + (low_sum >> 24);
        const uint64_t top_sum = uint64_t{registers_[a2]} + source_top + (mid_sum >> 24);
        const uint32_t result_low = static_cast<uint32_t>(low_sum) & 0xFFFFFF;
        const uint32_t result_mid = static_cast<uint32_t>(mid_sum) & 0xFFFFFF;
        const uint32_t result_top = static_cast<uint32_t>(top_sum) & 0xFF;
        registers_[a0] = result_low;
        registers_[a1] = result_mid;
        registers_[a2] = result_top;

        const uint32_t source_sign = (source_top >> 7) & 1;
        const uint32_t destination_sign = (top_sum - source_top - (mid_sum >> 24)) >> 7 & 1;
        const uint32_t result_sign = (result_top >> 7) & 1;
        const uint32_t overflow = (source_sign ^ result_sign) & (destination_sign ^ result_sign);
        constexpr uint32_t sr_c = 1u << 0;
        constexpr uint32_t sr_v = 1u << 1;
        constexpr uint32_t sr_z = 1u << 2;
        constexpr uint32_t sr_n = 1u << 3;
        constexpr uint32_t sr_u = 1u << 4;
        constexpr uint32_t sr_e = 1u << 5;
        constexpr uint32_t sr_l = 1u << 6;
        uint32_t status = registers_[57] & ~(sr_c | sr_v | sr_z | sr_n | sr_u | sr_e | sr_l);
        status |= (top_sum >> 8) & sr_c;
        status |= overflow ? (sr_v | sr_l) : 0;
        const uint32_t scaling = (status >> 10) & 3;
        if (scaling == 0) {
            const uint32_t extension = (result_top << 1) + (result_mid >> 23);
            if (extension != 0 && extension != 0x1FF) status |= sr_e;
            if ((result_mid & 0xC00000) == 0 || (result_mid & 0xC00000) == 0xC00000) {
                status |= sr_u;
            }
        } else if (scaling == 1) {
            if (result_top != 0 && result_top != 0xFF) status |= sr_e;
            const uint32_t unnormalized = ((result_top << 1) + (result_mid >> 23)) & 3;
            if (unnormalized == 0 || unnormalized == 3) status |= sr_u;
        } else if (scaling == 2) {
            const uint32_t extension = (result_top << 2) + (result_mid >> 22);
            if (extension != 0 && extension != 0x3FF) status |= sr_e;
            if ((result_mid & 0x600000) == 0 || (result_mid & 0x600000) == 0x600000) {
                status |= sr_u;
            }
        }
        if (result_top == 0 && result_mid == 0 && result_low == 0) status |= sr_z;
        status |= (result_top >> 4) & sr_n;
        registers_[57] = status;
        advance_program_counter(program_counter_ + 2);
        return StepResult::executed;
    }

    // OR #xxxx,D uses the same immediate and status-flag contract as AND.
    if ((instruction & and_long_fixed_bits) == (0x00140C2 & and_long_fixed_bits)) {
        if (program_counter_ + 1 >= program_word_count) {
            return StepResult::unsupported_instruction;
        }
        const uint32_t destination = (instruction >> 3) & 1;
        const uint32_t accumulator = destination == 0 ? 12 : 13;
        const uint32_t immediate = program_[program_counter_ + 1] & dsp_word_mask;
        registers_[accumulator] = (registers_[accumulator] | immediate) & dsp_word_mask;
        constexpr uint32_t sr_v = 1u << 1;
        constexpr uint32_t sr_z = 1u << 2;
        constexpr uint32_t sr_n = 1u << 3;
        registers_[57] &= 0xFFFFu & ~(sr_n | sr_z | sr_v);
        registers_[57] |= ((registers_[accumulator] >> 23) & 1) * sr_n;
        if (registers_[accumulator] == 0) registers_[57] |= sr_z;
        advance_program_counter(program_counter_ + 2);
        return StepResult::executed;
    }

    // MOVE X/Y:(Rn+xxx) <-> R with the encoded seven-bit displacement.
    constexpr uint32_t move_xy_displacement_mask = 0x00FE00A0;
    constexpr uint32_t move_x_displacement_value = 0x00020080;
    const uint32_t move_xy_fixed = instruction & move_xy_displacement_mask;
    if (move_xy_fixed == move_x_displacement_value ||
        move_xy_fixed == (move_x_displacement_value | 0x20)) {
        uint32_t displacement = (((instruction >> 11) & 0x3F) << 1) |
                                ((instruction >> 6) & 1);
        if (displacement & 0x40) displacement |= 0xFFFFFF80;
        const uint32_t register_number = (instruction >> 8) & 7;
        const uint32_t data_register = instruction & 0xF;
        const uint16_t address = static_cast<uint16_t>(
            registers_[16 + register_number] + static_cast<int32_t>(displacement));
        const bool y_space = (instruction & 0x20) != 0;
        auto& memory = y_space ? y_memory_ : x_memory_;
        if ((instruction & 0x10) == 0) {
            const uint32_t value = (data_register == 14 || data_register == 15)
                ? registers_[data_register == 14 ? 12 : 13] & dsp_word_mask
                : registers_[data_register];
            memory[address] = value & dsp_word_mask;
        } else {
            const uint32_t value = memory[address] & dsp_word_mask;
            if (data_register == 14 || data_register == 15) {
                const uint32_t accumulator = data_register == 14 ? 8 : 9;
                registers_[accumulator] = 0;
                registers_[accumulator + 4] = value;
                registers_[accumulator + 2] = value & (1u << 23) ? 0xFF : 0;
            } else {
                const uint8_t width = register_widths[data_register];
                if (width != 0) {
                    registers_[data_register] = value & ((uint32_t{1} << width) - 1);
                }
            }
        }
        advance_program_counter(program_counter_ + 1);
        return StepResult::executed;
    }

    // MOVE X/Y:(Rn+xxxx) <-> R with a full program-word displacement.
    constexpr uint32_t move_xy_long_mask = 0x00FFF880;
    const uint32_t move_xy_long_fixed = instruction & move_xy_long_mask;
    const bool move_y_long = move_xy_long_fixed == 0x000B7080;
    if (move_xy_long_fixed == 0x000A7080 || move_y_long) {
        if (program_counter_ + 1 >= program_word_count) {
            return StepResult::unsupported_instruction;
        }
        const uint32_t data_register = instruction & 0x3F;
        const uint32_t offset_register = (instruction >> 8) & 7;
        const uint32_t address = (registers_[16 + offset_register] +
            (program_[program_counter_ + 1] & dsp_word_mask)) & 0xFFFF;
        auto& memory = move_y_long ? y_memory_ : x_memory_;
        if ((instruction & (1u << 6)) == 0) {
            const uint32_t value = (data_register == 14 || data_register == 15)
                ? registers_[data_register == 14 ? 12 : 13] & dsp_word_mask
                : registers_[data_register];
            memory[address] = value & dsp_word_mask;
        } else {
            const uint32_t value = memory[address] & dsp_word_mask;
            if (data_register == 14 || data_register == 15) {
                const uint32_t accumulator = data_register == 14 ? 8 : 9;
                registers_[accumulator] = 0;
                registers_[accumulator + 4] = value;
                registers_[accumulator + 2] = value & (1u << 23) ? 0xFF : 0;
            } else {
                const uint8_t width = register_widths[data_register];
                if (width != 0) {
                    registers_[data_register] = value & ((uint32_t{1} << width) - 1);
                }
            }
        }
        advance_program_counter(program_counter_ + 2);
        return StepResult::executed;
    }

    // MOVEM P:ea <-> R. The common (Rn)+ and (Rn) forms transfer between
    // program memory and the DSP's general register file.
    constexpr uint32_t movem_ea_mask = 0x00FF40C0;
    constexpr uint32_t movem_ea_value = 0x00074080;
    if ((instruction & movem_ea_mask) == movem_ea_value) {
        const uint32_t effective_address = (instruction >> 8) & 0x3F;
        const uint32_t mode = effective_address >> 3;
        const uint32_t address_register = effective_address & 7;
        const uint32_t data_register = instruction & 0x3F;
        uint32_t* reg = &registers_[16 + address_register];
        uint32_t address = 0;
        uint32_t next_address = program_counter_ + 1;
        switch (mode) {
            case 0: address = *reg; *reg = (*reg - registers_[24 + address_register]) & 0xFFFF; break;
            case 1: address = *reg; *reg = (*reg + registers_[24 + address_register]) & 0xFFFF; break;
            case 2: address = *reg; *reg = (*reg - 1) & 0xFFFF; break;
            case 3: address = *reg; *reg = (*reg + 1) & 0xFFFF; break;
            case 4: address = *reg; break;
            case 6:
                if (address_register != 0 || program_counter_ + 1 >= program_word_count) {
                    return StepResult::unsupported_instruction;
                }
                address = program_[program_counter_ + 1] & 0xFFFF;
                next_address = program_counter_ + 2;
                break;
            case 7: *reg = (*reg - 1) & 0xFFFF; address = *reg; break;
            default: return StepResult::unsupported_instruction;
        }
        if (address >= program_word_count) return StepResult::unsupported_instruction;
        if ((instruction & (1u << 15)) != 0) {
            const uint32_t value = program_[address] & dsp_word_mask;
            if (data_register == 14 || data_register == 15) {
                const uint32_t accumulator = data_register == 14 ? 8 : 9;
                registers_[accumulator] = 0;
                registers_[accumulator + 4] = value;
                registers_[accumulator + 2] = value & (1u << 23) ? 0xFF : 0;
            } else {
                const uint8_t width = register_widths[data_register];
                registers_[data_register] = width == 0
                    ? 0 : value & ((uint32_t{1} << width) - 1);
            }
        } else {
            const uint32_t value = (data_register == 14 || data_register == 15)
                ? registers_[data_register == 14 ? 12 : 13] & dsp_word_mask
                : registers_[data_register];
            program_[address] = value & dsp_word_mask;
        }
        advance_program_counter(next_address);
        return StepResult::executed;
    }

    // PM3: parallel MOVE #xx,R with the MOVE ALU operation.
    if ((instruction >> 20) == 3 && (instruction & 0xFF) == 0) {
        const uint32_t destination = (instruction >> 16) & 0x1F;
        uint32_t value = (instruction >> 8) & 0xFF;
        if (destination == 4 || destination == 5 || destination == 6 ||
            destination == 7 || destination == 14 || destination == 15) {
            value <<= 16;
        }
        if (destination == 14 || destination == 15) {
            const uint32_t accumulator = destination == 14 ? 8 : 9;
            registers_[accumulator] = 0;
            registers_[accumulator + 4] = value;
            registers_[accumulator + 2] = value & (1u << 23) ? 0xFF : 0;
        } else {
            const uint8_t width = register_widths[destination];
            if (width != 0) {
                registers_[destination] = value & ((uint32_t{1} << width) - 1);
            }
        }
        advance_program_counter(program_counter_ + 1);
        return StepResult::executed;
    }

    // PM2 register transfer with MOVE as the ALU operation (S,D).
    // The GP firmware uses CMP X0,A with a parallel register transfer. Compare
    // the signed 56-bit accumulator against the sign-extended 24-bit X0 value.
    if ((instruction & 0x00FC0000) == 0x00200000 &&
        (instruction & 0xFF) == 0x45) {
        const uint32_t source_register = (instruction >> 13) & 0x1F;
        const uint32_t destination_register = (instruction >> 8) & 0x1F;
        const uint32_t parallel_value = read_data_register(
            static_cast<uint8_t>(source_register));

        const uint32_t x0 = registers_[4] & dsp_word_mask;
        const uint32_t source_top = (x0 & (1u << 23)) != 0 ? 0xFF : 0;
        const uint32_t source_mid = (x0 & (1u << 23)) != 0 ? dsp_word_mask : 0;
        const uint32_t a0 = registers_[8] & dsp_word_mask;
        const uint32_t a1 = registers_[12] & dsp_word_mask;
        const uint32_t a2 = registers_[10] & 0xFF;
        const uint32_t result_low = a0;
        const uint32_t mid_difference = a1 - source_mid;
        const uint32_t result_mid = mid_difference & dsp_word_mask;
        const uint32_t top_difference = a2 - source_top - ((mid_difference >> 24) & 1);
        const uint32_t result_top = top_difference & 0xFF;
        const uint32_t source_sign = (source_top >> 7) & 1;
        const uint32_t destination_sign = (a2 >> 7) & 1;
        const uint32_t result_sign = (result_top >> 7) & 1;
        const uint32_t overflow = (source_sign ^ destination_sign) &
                                  (result_sign ^ destination_sign);

        constexpr uint32_t sr_c = 1u << 0;
        constexpr uint32_t sr_v = 1u << 1;
        constexpr uint32_t sr_z = 1u << 2;
        constexpr uint32_t sr_n = 1u << 3;
        constexpr uint32_t sr_l = 1u << 6;
        uint32_t status = registers_[57] & ~(sr_c | sr_v | sr_z | sr_n | sr_l);
        status |= (top_difference >> 8) & sr_c;
        if (overflow != 0) status |= sr_v | sr_l;
        if (result_sign != 0) status |= sr_n;
        if (result_top == 0 && result_mid == 0 && result_low == 0) status |= sr_z;
        registers_[57] = status;
        write_data_register(static_cast<uint8_t>(destination_register), parallel_value);
        advance_program_counter(program_counter_ + 1);
        return StepResult::executed;
    }

    if ((instruction & 0x00FC0000) == 0x00200000 && (instruction & 0xFF) == 0) {
        const uint32_t source = (instruction >> 13) & 0x1F;
        const uint32_t destination = (instruction >> 8) & 0x1F;
        const uint32_t value = (source == 14 || source == 15)
            ? registers_[source == 14 ? 12 : 13] & dsp_word_mask
            : registers_[source];
        if (destination == 14 || destination == 15) {
            const uint32_t accumulator = destination == 14 ? 8 : 9;
            registers_[accumulator] = 0;
            registers_[accumulator + 4] = value & dsp_word_mask;
            registers_[accumulator + 2] = value & (1u << 23) ? 0xFF : 0;
        } else {
            const uint8_t width = register_widths[destination];
            if (width != 0) {
                registers_[destination] = value & ((uint32_t{1} << width) - 1);
            }
        }
        advance_program_counter(program_counter_ + 1);
        return StepResult::executed;
    }

    // PM2 parallel MOVE #xx,D form. GP firmware uses it for ordinary
    // register initialization (for example P[0x1E] = MOVE #$00,R4).
    if ((instruction & 0x00E00000) == 0x00200000 && (instruction & 0xFF) == 0) {
        const uint32_t destination = (instruction >> 16) & 0x1F;
        const uint32_t value = (instruction >> 8) & 0xFF;
        const uint8_t width = register_widths[destination];
        if (width != 0) {
            registers_[destination] = value & ((uint32_t{1} << width) - 1);
        }
        advance_program_counter(program_counter_ + 1);
        return StepResult::executed;
    }

    // Parallel MOVE #xxxxxx,D with the MOVE ALU opcode appears in the
    // observed startup routine. The null destination is architecturally inert;
    // other supported destinations update their width-masked DSP register.
    const uint32_t parallel_class = instruction >> 20;
    const uint32_t parallel_ea = (instruction >> 8) & 0x3F;
    const uint32_t parallel_destination = ((instruction >> 16) & 7) |
        ((instruction >> 17) & 0x18);
    if (parallel_class >= 4 && parallel_class <= 7 &&
        (instruction & 0xFF) == 0) {
        const bool write_destination = (instruction & (1u << 15)) != 0;
        const uint32_t mode = parallel_ea >> 3;
        const uint32_t register_index = parallel_ea & 7;
        uint32_t value = 0;
        uint32_t next_address = program_counter_ + 1;
        const bool y_space = (instruction & (1u << 19)) != 0;
        auto& memory = y_space ? y_memory_ : x_memory_;

        // PM5 absolute-address MOVE uses the same top nibble as EA-based
        // parallel moves, but bit 14 is clear and the six-bit EA is absolute.
        if ((instruction & (1u << 14)) == 0 &&
            (instruction & 0x00F40000) != 0x00400000) {
            const uint32_t address = (instruction >> 8) & 0x3F;
            if (write_destination) {
                write_data_register(static_cast<uint8_t>(parallel_destination),
                                    memory[address]);
            } else {
                memory[address] = read_data_register(
                    static_cast<uint8_t>(parallel_destination)) & dsp_word_mask;
            }
            advance_program_counter(program_counter_ + 1);
            return StepResult::executed;
        }

        if ((instruction & (1u << 14)) == 0 || register_index >= 8) {
            return StepResult::unsupported_instruction;
        }
        const bool immediate = mode == 6 && register_index != 0;
        uint32_t address = 0;
        auto* address_register = &registers_[16 + register_index];
        const uint32_t modifier = registers_[24 + register_index];
        if (immediate) {
            if (program_counter_ + 1 >= program_word_count) {
                return StepResult::unsupported_instruction;
            }
            value = program_[program_counter_ + 1] & dsp_word_mask;
            next_address = program_counter_ + 2;
        } else {
            switch (mode) {
                case 0: address = *address_register; *address_register = (*address_register - modifier) & 0xFFFF; break;
                case 1: address = *address_register; *address_register = (*address_register + modifier) & 0xFFFF; break;
                case 2: address = *address_register; *address_register = (*address_register - 1) & 0xFFFF; break;
                case 3: address = *address_register; *address_register = (*address_register + 1) & 0xFFFF; break;
                case 4: address = *address_register; break;
                case 5: address = (*address_register + modifier) & 0xFFFF; break;
                case 6:
                    if (program_counter_ + 1 >= program_word_count) return StepResult::unsupported_instruction;
                    address = program_[program_counter_ + 1] & 0xFFFF;
                    next_address = program_counter_ + 2;
                    break;
                case 7: *address_register = (*address_register - 1) & 0xFFFF; address = *address_register; break;
            }
            if (!write_destination) {
                const uint32_t source = parallel_destination;
                value = (source == 14 || source == 15)
                    ? registers_[source == 14 ? 12 : 13] & dsp_word_mask
                    : registers_[source];
                memory[address] = value & dsp_word_mask;
            } else {
                value = memory[address] & dsp_word_mask;
            }
        }

        if (write_destination) {
            if (parallel_destination == 14 || parallel_destination == 15) {
                const uint32_t accumulator = parallel_destination == 14 ? 8 : 9;
                registers_[accumulator] = 0;
                registers_[accumulator + 4] = value & dsp_word_mask;
                registers_[accumulator + 2] = value & (1u << 23) ? 0xFF : 0;
            } else {
                const uint8_t width = register_widths[parallel_destination];
                if (width != 0) {
                    const uint32_t mask = (uint32_t{1} << width) - 1;
                    registers_[parallel_destination] = value & mask;
                }
            }
        }

        advance_program_counter(next_address);
        return StepResult::executed;
    }

    // The first parallel ALU instruction in the firmware startup path is CLR A.
    if (instruction == 0x200013) {
        registers_[8] = 0;  // A0
        registers_[12] = 0; // A1
        registers_[10] = 0; // A2
        constexpr uint32_t sr_e = 1u << 5;
        constexpr uint32_t sr_n = 1u << 3;
        constexpr uint32_t sr_v = 1u << 1;
        constexpr uint32_t sr_u = 1u << 4;
        constexpr uint32_t sr_z = 1u << 2;
        registers_[57] &= 0xFFFFu & ~(sr_e | sr_n | sr_v);
        registers_[57] |= sr_u | sr_z;
        advance_program_counter(program_counter_ + 1);
        return StepResult::executed;
    }

    if (instruction == 0x00000C) { // RTS
        if (stack_depth_ == 0) return StepResult::unsupported_instruction;
        program_counter_ = return_stack_[--stack_depth_];
        return StepResult::executed;
    }

    return StepResult::unsupported_instruction;
}

} // namespace xbe::kernel
