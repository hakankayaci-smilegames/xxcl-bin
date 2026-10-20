#include "../src/kernel/nv2a.hpp"
#include "../src/kernel/nv2a_pfifo.hpp"
#include "../src/kernel/guest_dma_memory.hpp"

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

void test_active_dma_register_sync_copies_user_submission_to_pfifo() {
    xbe::kernel::Nv2aPfifoState fifo;
    volatile uint32_t user_get = 0;
    volatile uint32_t user_put = 0x123400;
    volatile uint32_t dma_get = 0;
    volatile uint32_t dma_put = 0;

    require(fifo.sync_active_dma_registers(1, 0x100, &user_get, &user_put,
                                           &dma_get, &dma_put),
            "active DMA channel must be synchronized");
    require(dma_put == 0x123400,
            "guest USER PUT must reach PFIFO DMA PUT");
}

void test_active_dma_register_sync_mirrors_completed_get_to_user() {
    xbe::kernel::Nv2aPfifoState fifo;
    volatile uint32_t user_get = 0;
    volatile uint32_t user_put = 0x400;
    volatile uint32_t dma_get = 0;
    volatile uint32_t dma_put = 0;

    require(fifo.sync_active_dma_registers(1, 0x100, &user_get, &user_put,
                                           &dma_get, &dma_put),
            "active DMA channel must be synchronized");
    dma_get = 0x80;
    require(fifo.sync_active_dma_registers(1, 0x100, &user_get, &user_put,
                                           &dma_get, &dma_put),
            "active DMA channel must remain synchronized");
    require(user_get == 0x80,
            "PFIFO GET progress must be visible through USER DMA GET");
}

void test_pio_or_disabled_channel_is_not_dma_synchronized() {
    xbe::kernel::Nv2aPfifoState fifo;
    volatile uint32_t user_get = 0;
    volatile uint32_t user_put = 0x400;
    volatile uint32_t dma_get = 0;
    volatile uint32_t dma_put = 0;

    require(!fifo.sync_active_dma_registers(0, 0x100, &user_get, &user_put,
                                            &dma_get, &dma_put),
            "PIO channel must not be treated as a DMA channel");
    require(dma_put == 0,
            "inactive channel must not copy USER PUT into PFIFO");
}

void test_dma_object_words_are_read_from_the_requested_ramin_offset() {
    const std::array<uint32_t, 6> ramin{
        0, 0, 0, 0x0000B002, 0x07FFAFFf, 0x00000003
    };
    const auto object = xbe::kernel::decode_nv2a_dma_object(ramin, 12);

    require(object.status == xbe::kernel::Nv2aDmaObjectStatus::ok,
            "complete aligned DMA object must decode");
    require(object.flags == 0x0000B002 && object.limit == 0x07FFAFFf &&
                object.frame == 3,
            "DMA object must preserve its three source words");
    require(xbe::kernel::nv2a_dma_object_base_address(object) == 0,
            "DMA object base must combine the address fields from flags and frame");
}

void test_dma_object_decoder_rejects_misaligned_and_truncated_inputs() {
    const std::array<uint32_t, 6> ramin{};
    const auto misaligned = xbe::kernel::decode_nv2a_dma_object(ramin, 2);
    const auto truncated = xbe::kernel::decode_nv2a_dma_object(ramin, 20);

    require(misaligned.status == xbe::kernel::Nv2aDmaObjectStatus::invalid_offset,
            "unaligned DMA object address must be rejected");
    require(truncated.status == xbe::kernel::Nv2aDmaObjectStatus::truncated,
            "DMA object extending beyond RAMIN must be rejected");
}

void test_dma_jump_advances_get_to_the_decoded_target() {
    const auto result = xbe::kernel::process_nv2a_dma_control_word(
        0, 0x04ad8001, 0, 0x07ffafff);

    require(result.status == xbe::kernel::Nv2aDmaControlStatus::jump,
            "aligned DMA jump within the object limit must be processed");
    require(result.next_get == 0x04ad8000,
            "processing a DMA jump must move GET to its target address");
}

void test_dma_jump_outside_the_object_limit_does_not_advance_get() {
    const auto result = xbe::kernel::process_nv2a_dma_control_word(
        0, 0x08000001, 0, 0x07ffafff);

    require(result.status == xbe::kernel::Nv2aDmaControlStatus::out_of_range,
            "DMA jump beyond the object limit must be rejected");
    require(result.next_get == 0,
            "rejected DMA jump must leave GET unchanged");
}

void test_dma_jump_limit_includes_the_dma_object_base() {
    const auto result = xbe::kernel::process_nv2a_dma_control_word(
        0x100, 0x00002001, 0x00001000, 0x00002fff);

    require(result.status == xbe::kernel::Nv2aDmaControlStatus::out_of_range,
            "DMA object base plus jump target must stay below the object limit");
    require(result.next_get == 0x100,
            "base-adjusted out-of-range jump must not advance GET");
}

void test_dma_jump_at_the_inclusive_limit_is_accepted() {
    const auto result = xbe::kernel::process_nv2a_dma_control_word(
        0, 0x00001001, 0, 0x00001000);

    require(result.status == xbe::kernel::Nv2aDmaControlStatus::jump,
            "DMA jump exactly at the inclusive object limit must be accepted");
    require(result.next_get == 0x1000,
            "inclusive-limit jump must move GET to its target");
}

void test_unsupported_dma_call_does_not_advance_get() {
    const auto result = xbe::kernel::process_nv2a_dma_control_word(
        0x100, 0x02000002, 0, 0x07ffafff);

    require(result.status == xbe::kernel::Nv2aDmaControlStatus::unsupported,
            "valid but unsupported DMA call must be reported as unsupported");
    require(result.next_get == 0x100,
            "unsupported DMA control word must leave GET unchanged");
}

} // namespace

int main() {
    uint32_t guest_address = 0;
    if (!xbe::kernel::find_guest_dma_memory_address(0x200000, 0x4000, guest_address) ||
        guest_address != 0x04000000) return 40;
    if (!xbe::kernel::find_guest_dma_memory_address(0x200000, 0x4000, guest_address,
                                       0x04001000) ||
        guest_address != 0x04004000) return 41;
    if (xbe::kernel::find_guest_dma_memory_address(0x04000000, 0x1000, guest_address)) return 42;

    test_fifo_register_addresses();
    test_pfb_write_buffer_flush_completes_immediately();
    test_pending_observation_is_counted_once_without_advancing_get();
    test_active_dma_user_registers_share_pfifo_state();
    test_inactive_user_channel_does_not_change_active_fifo();
    test_incrementing_method_packet_is_decoded_without_consuming_more_words();
    test_truncated_method_packet_is_not_reported_as_consumed();
    test_reserved_method_header_is_rejected();
    test_active_dma_register_sync_copies_user_submission_to_pfifo();
    test_active_dma_register_sync_mirrors_completed_get_to_user();
    test_pio_or_disabled_channel_is_not_dma_synchronized();
    test_dma_object_words_are_read_from_the_requested_ramin_offset();
    test_dma_object_decoder_rejects_misaligned_and_truncated_inputs();
    test_dma_jump_advances_get_to_the_decoded_target();
    test_dma_jump_outside_the_object_limit_does_not_advance_get();
    test_dma_jump_limit_includes_the_dma_object_base();
    test_dma_jump_at_the_inclusive_limit_is_accepted();
    test_unsupported_dma_call_does_not_advance_get();
    return 0;
}
