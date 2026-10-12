#include "apu_dsp_dma.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <vector>

namespace xbe::kernel {

void ApuDspDma::reset() {
    configuration_ = 0;
    control_ = 0;
    start_block_ = 0;
    next_block_ = 0;
    control_read_count_ = 0;
    end_of_list_ = false;
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
            end_of_list_ = false;
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

bool ApuDspDma::run_descriptors(std::span<uint32_t> x_memory,
                                std::span<uint32_t> y_memory,
                                std::span<uint32_t> program_memory,
                                BufferTransfer transfer, void* context) {
    if ((control_ & control_running) == 0 || (control_ & control_frozen) != 0 ||
        transfer == nullptr) return false;

    struct LocatedMemory {
        std::span<uint32_t> words;
        size_t offset;
    };
    const auto locate = [&](uint32_t address) -> LocatedMemory {
        if (address < 0x1800) return {x_memory, address};
        if (address >= 0x1800 && address < 0x2000) return {y_memory, address - 0x1800};
        if (address >= 0x2800 && address < 0x3800) return {program_memory, address - 0x2800};
        return {{}, 0};
    };
    const auto valid_range = [](const LocatedMemory& memory, size_t offset, size_t count) {
        return !memory.words.empty() && offset <= memory.words.size() &&
               count <= memory.words.size() - offset;
    };
    const auto load_le = [](const std::byte* data, size_t size) {
        uint32_t value = 0;
        for (size_t byte = 0; byte < size; ++byte) {
            value |= std::to_integer<uint32_t>(data[byte]) << (byte * 8);
        }
        return value;
    };
    const auto store_le = [](std::byte* data, size_t size, uint32_t value) {
        for (size_t byte = 0; byte < size; ++byte) {
            data[byte] = static_cast<std::byte>((value >> (byte * 8)) & 0xFF);
        }
    };

    constexpr uint32_t eol_mask = 1u << 14;
    uint32_t node = next_block_;
    for (size_t descriptor_count = 0;
             (node & eol_mask) == 0 && descriptor_count < 1024;
             ++descriptor_count) {
        const uint32_t address = node & 0x3FFF;
        const LocatedMemory descriptor_memory = locate(address);
        const size_t descriptor_offset = address < 0x1800 ? address :
            (address < 0x2000 ? address - 0x1800 : address - 0x2800);
        if (!valid_range(descriptor_memory, descriptor_offset, 7)) return false;
        auto descriptor = descriptor_memory.words.subspan(descriptor_offset, 7);

        const uint32_t next = descriptor[0];
        const uint32_t control = descriptor[1];
        const uint32_t count = descriptor[2];
        const uint32_t dsp_offset = descriptor[3];
        uint32_t scratch_offset = descriptor[4];
        const uint32_t scratch_base = descriptor[5];
        const uint32_t ring_size = descriptor[6] + 1;
        const uint32_t interleave = control & 1;
        const uint32_t direction = (control >> 1) & 1;
        const uint32_t buffer_id = (control >> 5) & 0xF;
        const uint32_t format = (control >> 10) & 7;
        if ((control & ((3u << 2) | (1u << 13))) != 0 || buffer_id > 0xF) return false;

        size_t item_size = 0;
        uint32_t item_mask = 0xFFFFFFFF;
        switch (format) {
        case 0: item_size = 1; item_mask = 0xFF; break;
        case 1: item_size = 2; item_mask = 0xFFFF; break;
        case 2:
        case 6: item_size = 4; item_mask = 0x00FFFFFF; break;
        default: return false;
        }
        if (item_size == 1) return false;

        const size_t channels = (count & 0xF) + 1;
        const size_t blocks = count >> 4;
        const size_t samples = (direction != 0 && interleave != 0)
            ? blocks * channels
            : count;
        const size_t transfer_samples = (direction != 0 && interleave != 0)
            ? blocks * channels
            : count;
        if (samples == 0 || transfer_samples > std::numeric_limits<size_t>::max() / item_size) {
            if (samples == 0) {
                next_block_ = next;
                node = next;
                if ((node & eol_mask) != 0) end_of_list_ = true;
                continue;
            }
            return false;
        }
        const size_t dsp_address_extent = direction != 0 && interleave != 0
            ? blocks * channels
            : count;
        if (dsp_offset > 0x3FFF || dsp_address_extent == 0) return false;
        const LocatedMemory dsp_memory = locate(dsp_offset);
        const size_t dsp_index = dsp_offset < 0x1800 ? dsp_offset :
            (dsp_offset < 0x2000 ? dsp_offset - 0x1800 : dsp_offset - 0x2800);
        if (!valid_range(dsp_memory, dsp_index, dsp_address_extent)) return false;

        const size_t byte_count = transfer_samples * item_size;
        std::vector<std::byte> bytes(byte_count);
        if (direction != 0) {
            if (interleave != 0) {
                for (size_t block = 0; block < blocks; ++block) {
                    for (size_t channel = 0; channel < channels; ++channel) {
                        const uint32_t value = dsp_memory.words[dsp_index + channel * blocks + block];
                        store_le(bytes.data() + (block * channels + channel) * item_size,
                                 item_size, item_size == 2 ? value >> 8 : value);
                    }
                }
            } else {
                for (size_t sample = 0; sample < count; ++sample) {
                    const uint32_t value = dsp_memory.words[dsp_index + sample];
                    store_le(bytes.data() + sample * item_size, item_size,
                             item_size == 2 ? value >> 8 : value);
                }
            }
        }

        const bool circular = buffer_id == 0xE;
        if (buffer_id == 0xF || circular || buffer_id <= 3) {
            size_t byte_offset = 0;
            size_t remaining = bytes.size();
            uint32_t ring_cursor = scratch_offset;
            while (remaining != 0) {
                const size_t chunk = circular
                    ? std::min<size_t>(remaining, ring_size - (ring_cursor % ring_size))
                    : remaining;
                const uint64_t absolute_offset = uint64_t{scratch_base} + ring_cursor;
                if (absolute_offset > UINT32_MAX ||
                    !transfer(context, buffer_id, static_cast<uint32_t>(absolute_offset),
                              ring_size, std::span<std::byte>(bytes).subspan(byte_offset, chunk),
                              direction != 0)) return false;
                byte_offset += chunk;
                remaining -= chunk;
                if (circular) ring_cursor = static_cast<uint32_t>((ring_cursor + chunk) % ring_size);
            }
            if (circular) scratch_offset = ring_cursor;
        } else {
            return false;
        }

        if (direction == 0) {
            for (size_t sample = 0; sample < count; ++sample) {
                dsp_memory.words[dsp_index + sample] =
                    load_le(bytes.data() + sample * item_size, item_size) & item_mask;
            }
        }
        if ((control & (1u << 4)) != 0) descriptor[4] = scratch_offset;

        node = next;
        next_block_ = node;
        if ((node & eol_mask) != 0) end_of_list_ = true;
    }
    return (node & eol_mask) != 0;
}

} // namespace xbe::kernel
