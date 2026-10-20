#include "../src/kernel/nv2a.hpp"
#include "../src/kernel/nv2a_pfifo.hpp"

#include <cstdlib>
#include <iostream>
#include <array>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

void test_fifo_register_addresses() {
    require(xbe::kernel::nv2a_fifo_dma_put_address == 0xFD003240,
            "PFIFO DMA PUT must use NV_PFIFO_CACHE1_DMA_PUT");
    require(xbe::kernel::nv2a_fifo_dma_get_address == 0xFD003244,
            "PFIFO DMA GET must use NV_PFIFO_CACHE1_DMA_GET");
}

void test_pfb_write_buffer_flush_completes_immediately() {
    volatile uint32_t register_value = 1u << 16;
    xbe::kernel::nv2a_complete_pfb_write_buffer_flush(&register_value);
    require(register_value == 0,
            "unmodeled PFB writeback cache must report no pending flush");
}

void test_pending_observation_is_counted_once_without_advancing_get() {
    xbe::kernel::Nv2aFifoObserver observer;
    require(!observer.observe(0, 0), "empty FIFO must not count as pending");
    require(observer.observe(0, 0x100), "new pending interval must be observed");
    require(!observer.observe(0, 0x100), "same pending interval must not count twice");
    require(observer.pending_updates() == 1, "one pending interval must be counted");
    require(observer.get() == 0 && observer.put() == 0x100,
            "observation must preserve hardware GET and PUT values");
}

void test_active_dma_user_registers_share_pfifo_state() {
    xbe::kernel::Nv2aPfifoState fifo;
    fifo.set_channel_dma_enabled(3, true);
    fifo.set_active_channel(3);
    fifo.write_dma_put(0x23456000);

    uint32_t value = 0;
    require(fifo.read_user_register(3, xbe::kernel::Nv2aUserRegister::dma_put, value),
            "active DMA channel USER PUT must be readable");
    require(value == 0x23456000,
            "USER PUT must read the value held by PFIFO DMA PUT");

    require(fifo.write_user_register(3, xbe::kernel::Nv2aUserRegister::dma_get, 0x40),
            "active DMA channel USER GET must be writable");
    require(fifo.dma_get() == 0x40,
            "USER GET writes must update PFIFO DMA GET");
}

void test_inactive_user_channel_does_not_change_active_fifo() {
    xbe::kernel::Nv2aPfifoState fifo;
    fifo.set_channel_dma_enabled(2, true);
    fifo.set_active_channel(2);
    fifo.write_dma_put(0x80);

    require(!fifo.write_user_register(1, xbe::kernel::Nv2aUserRegister::dma_put, 0x100),
            "USER writes for a non-active channel must be rejected");
    require(fifo.dma_put() == 0x80,
            "a rejected USER write must leave PFIFO DMA PUT unchanged");
}

void test_incrementing_method_packet_is_decoded_without_consuming_more_words() {
    const std::array<uint32_t, 3> words{0x00080060, 0x11223344, 0x55667788};
    const auto packet = xbe::kernel::decode_nv2a_method_packet(words);

    require(packet.status == xbe::kernel::Nv2aPacketStatus::ok,
            "valid incrementing method packet must decode");
    require(packet.method == 0x60 && packet.subchannel == 0,
            "packet header must produce the literal method and subchannel");
    require(packet.incrementing && packet.method_count == 2,
            "incrementing packet must preserve its count");
    require(packet.consumed_words == 3 && packet.payload[1] == 0x55667788,
            "decoder must expose only the declared payload words");
}

void test_truncated_method_packet_is_not_reported_as_consumed() {
    const std::array<uint32_t, 2> words{0x00080060, 0x11223344};
    const auto packet = xbe::kernel::decode_nv2a_method_packet(words);

    require(packet.status == xbe::kernel::Nv2aPacketStatus::truncated,
            "truncated payload must be rejected");
    require(packet.consumed_words == 0,
            "truncated packet must not advance the DMA GET position");
}

void test_reserved_method_header_is_rejected() {
    const std::array<uint32_t, 1> words{0x20000000};
    const auto packet = xbe::kernel::decode_nv2a_method_packet(words);

    require(packet.status == xbe::kernel::Nv2aPacketStatus::invalid_header,
            "reserved command header must not be accepted as a method packet");
    require(packet.consumed_words == 0,
            "invalid header must not advance the DMA GET position");
}

} // namespace

int main() {
    test_fifo_register_addresses();
    test_pfb_write_buffer_flush_completes_immediately();
    test_pending_observation_is_counted_once_without_advancing_get();
    test_active_dma_user_registers_share_pfifo_state();
    test_inactive_user_channel_does_not_change_active_fifo();
    test_incrementing_method_packet_is_decoded_without_consuming_more_words();
    test_truncated_method_packet_is_not_reported_as_consumed();
    test_reserved_method_header_is_rejected();
    return 0;
}
