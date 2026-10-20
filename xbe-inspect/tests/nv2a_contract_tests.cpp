#include "../src/kernel/nv2a.hpp"
#include "../src/kernel/nv2a_pfifo.hpp"

#include <cstdlib>
#include <iostream>

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
    fifo.write_dma_put(0x04AD8000);

    uint32_t value = 0;
    require(fifo.read_user_register(3, xbe::kernel::Nv2aUserRegister::dma_put, value),
            "active DMA channel USER PUT must be readable");
    require(value == 0x04AD8000,
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

} // namespace

int main() {
    test_fifo_register_addresses();
    test_pfb_write_buffer_flush_completes_immediately();
    test_pending_observation_is_counted_once_without_advancing_get();
    test_active_dma_user_registers_share_pfifo_state();
    test_inactive_user_channel_does_not_change_active_fifo();
    return 0;
}
