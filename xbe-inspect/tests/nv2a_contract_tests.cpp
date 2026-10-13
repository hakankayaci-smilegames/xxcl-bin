#include "../src/kernel/nv2a.hpp"

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

void test_pending_observation_is_counted_once_without_advancing_get() {
    xbe::kernel::Nv2aFifoObserver observer;
    require(!observer.observe(0, 0), "empty FIFO must not count as pending");
    require(observer.observe(0, 0x100), "new pending interval must be observed");
    require(!observer.observe(0, 0x100), "same pending interval must not count twice");
    require(observer.pending_updates() == 1, "one pending interval must be counted");
    require(observer.get() == 0 && observer.put() == 0x100,
            "observation must preserve hardware GET and PUT values");
}

} // namespace

int main() {
    test_fifo_register_addresses();
    test_pending_observation_is_counted_once_without_advancing_get();
    return 0;
}
