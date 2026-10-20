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

enum class Nv2aDmaObjectStatus : uint8_t {
    ok,
    invalid_offset,
    truncated
};

struct Nv2aDmaObjectWords {
    Nv2aDmaObjectStatus status = Nv2aDmaObjectStatus::invalid_offset;
    uint32_t flags = 0;
    uint32_t limit = 0;
    uint32_t frame = 0;
};

inline uint32_t nv2a_dma_object_base_address(const Nv2aDmaObjectWords& object) {
    return (object.frame & 0xfffff000u) | (object.flags & 0xfff00000u);
}

inline Nv2aDmaObjectWords decode_nv2a_dma_object(std::span<const uint32_t> ramin,
                                                  uint32_t byte_offset) {
    if ((byte_offset & 3) != 0) return {};
    const std::size_t word_offset = byte_offset / sizeof(uint32_t);
    if (word_offset > ramin.size() || ramin.size() - word_offset < 3) {
        Nv2aDmaObjectWords result;
        result.status = Nv2aDmaObjectStatus::truncated;
        return result;
    }

    return {Nv2aDmaObjectStatus::ok, ramin[word_offset], ramin[word_offset + 1],
            ramin[word_offset + 2]};
}

enum class Nv2aDmaControlStatus : uint8_t {
    jump,
    unsupported,
    invalid,
    out_of_range
};

struct Nv2aDmaControlResult {
    Nv2aDmaControlStatus status = Nv2aDmaControlStatus::invalid;
    uint32_t next_get = 0;
};

inline Nv2aDmaControlResult process_nv2a_dma_control_word(
    uint32_t current_get, uint32_t word, uint32_t object_base, uint32_t object_limit) {
    Nv2aDmaControlResult result{Nv2aDmaControlStatus::invalid, current_get};
    if ((current_get & 3u) != 0) return result;

    const uint32_t command = word & 3u;
    if (command == 2u || word == 0x00020000u) {
        result.status = Nv2aDmaControlStatus::unsupported;
        return result;
    }
    if (command != 1u) return result;

    const uint32_t target = word & ~3u;
    const uint64_t effective_target = static_cast<uint64_t>(object_base) + target;
    if (effective_target > object_limit) {
        result.status = Nv2aDmaControlStatus::out_of_range;
        return result;
    }

    result.status = Nv2aDmaControlStatus::jump;
    result.next_get = target;
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

    bool sync_active_dma_registers(uint32_t channel_modes, uint32_t push1,
                                   volatile uint32_t* user_get,
                                   volatile uint32_t* user_put,
                                   volatile uint32_t* dma_get,
                                   volatile uint32_t* dma_put) {
        const auto channel = static_cast<uint8_t>(push1 & 0xf);
        if (!user_get || !user_put || !dma_get || !dma_put || channel >= 32 ||
            (channel_modes & (uint32_t{1} << channel)) == 0 ||
            (push1 & (uint32_t{1} << 8)) == 0) {
            return false;
        }

        const uint32_t guest_put = *user_put;
        if (guest_put != last_user_put_) {
            *dma_put = guest_put;
            last_user_put_ = guest_put;
        } else if (*dma_put != last_user_put_) {
            *user_put = *dma_put;
            last_user_put_ = *dma_put;
        }

        const uint32_t guest_get = *user_get;
        if (guest_get != last_user_get_) {
            *dma_get = guest_get;
            last_user_get_ = guest_get;
        } else if (*dma_get != last_user_get_) {
            *user_get = *dma_get;
            last_user_get_ = *dma_get;
        }
        return true;
    }

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
    uint32_t last_user_get_ = 0;
    uint32_t last_user_put_ = 0;
};

} // namespace xbe::kernel
