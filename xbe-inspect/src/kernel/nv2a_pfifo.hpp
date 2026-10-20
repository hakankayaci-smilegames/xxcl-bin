#pragma once

#include <cstdint>
#include <cstddef>
#include <span>

namespace xbe::kernel {

enum class Nv2aUserRegister : uint8_t {
    dma_put,
    dma_get,
    reference
};

enum class Nv2aPacketStatus : uint8_t {
    ok,
    truncated,
    invalid_header
};

struct Nv2aMethodPacket {
    Nv2aPacketStatus status = Nv2aPacketStatus::invalid_header;
    uint32_t method = 0;
    uint8_t subchannel = 0;
    uint16_t method_count = 0;
    bool incrementing = false;
    std::size_t consumed_words = 0;
    std::span<const uint32_t> payload{};
};

inline Nv2aMethodPacket decode_nv2a_method_packet(std::span<const uint32_t> words) {
    if (words.empty()) return {};

    const uint32_t header = words.front();
    const uint32_t command = header & 0xE0030003;
    if (command != 0 && command != 0x40000000) return {};

    const auto count = static_cast<uint16_t>((header >> 18) & 0x7ff);
    if (count > words.size() - 1) {
        Nv2aMethodPacket result;
        result.status = Nv2aPacketStatus::truncated;
        return result;
    }

    Nv2aMethodPacket result;
    result.status = Nv2aPacketStatus::ok;
    result.method = header & 0x1ffc;
    result.subchannel = static_cast<uint8_t>((header >> 13) & 0x7);
    result.method_count = count;
    result.incrementing = command == 0;
    result.consumed_words = static_cast<std::size_t>(count) + 1;
    result.payload = words.subspan(1, count);
    return result;
}

class Nv2aPfifoState {
public:
    void set_channel_dma_enabled(uint8_t channel, bool enabled) {
        if (channel >= 32) return;
        const uint32_t mask = uint32_t{1} << channel;
        if (enabled) channel_modes_ |= mask;
        else channel_modes_ &= ~mask;
    }

    void set_active_channel(uint8_t channel) { active_channel_ = channel; }
    void write_dma_put(uint32_t value) { dma_put_ = value; }
    uint32_t dma_get() const { return dma_get_; }
    uint32_t dma_put() const { return dma_put_; }

    bool read_user_register(uint8_t channel, Nv2aUserRegister reg, uint32_t& value) const {
        if (!active_dma_channel(channel)) return false;
        switch (reg) {
        case Nv2aUserRegister::dma_put: value = dma_put_; return true;
        case Nv2aUserRegister::dma_get: value = dma_get_; return true;
        case Nv2aUserRegister::reference: value = reference_; return true;
        }
        return false;
    }

    bool write_user_register(uint8_t channel, Nv2aUserRegister reg, uint32_t value) {
        if (!active_dma_channel(channel)) return false;
        switch (reg) {
        case Nv2aUserRegister::dma_put: dma_put_ = value; return true;
        case Nv2aUserRegister::dma_get: dma_get_ = value; return true;
        case Nv2aUserRegister::reference: reference_ = value; return true;
        }
        return false;
    }

private:
    bool active_dma_channel(uint8_t channel) const {
        return channel < 32 && channel == active_channel_ &&
               (channel_modes_ & (uint32_t{1} << channel)) != 0;
    }

    uint32_t channel_modes_ = 0;
    uint8_t active_channel_ = 0;
    uint32_t dma_get_ = 0;
    uint32_t dma_put_ = 0;
    uint32_t reference_ = 0;
};

} // namespace xbe::kernel
