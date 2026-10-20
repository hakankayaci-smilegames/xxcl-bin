#pragma once

#include <cstdint>

namespace xbe::kernel {

inline constexpr uintptr_t nv2a_fifo_dma_put_address = 0xFD003240;
inline constexpr uintptr_t nv2a_fifo_dma_get_address = 0xFD003244;
inline constexpr uintptr_t nv2a_fifo_mode_address = 0xFD002504;
inline constexpr uintptr_t nv2a_fifo_push1_address = 0xFD003204;
inline constexpr uintptr_t nv2a_fifo_dma_instance_address = 0xFD00322C;
inline constexpr uintptr_t nv2a_user_base_address = 0xFD800000;
inline constexpr uintptr_t nv2a_ramin_base_address = 0xFD700000;
inline constexpr uint32_t nv2a_ramin_size = 0x00100000;
inline constexpr uint32_t nv2a_user_dma_put_offset = 0x40;
inline constexpr uint32_t nv2a_user_dma_get_offset = 0x44;
inline constexpr uintptr_t nv2a_fifo_status_address = 0xFD003214;
inline constexpr uintptr_t nv2a_pfb_write_buffer_cache_address = 0xFD100410;

inline void nv2a_complete_pfb_write_buffer_flush(volatile uint32_t* register_value) {
    // No writeback cache is modeled, so the flush has completed immediately.
    *register_value = 0;
}

class Nv2aFifoObserver {
public:
    bool observe(uint32_t get, uint32_t put) {
        const bool changed = !has_previous_ || get != get_ || put != put_;
        has_previous_ = true;
        get_ = get;
        put_ = put;
        if (!changed || get == put) return false;
        ++pending_updates_;
        return true;
    }

    uint64_t pending_updates() const { return pending_updates_; }
    uint32_t get() const { return get_; }
    uint32_t put() const { return put_; }

private:
    uint32_t get_ = 0;
    uint32_t put_ = 0;
    uint64_t pending_updates_ = 0;
    bool has_previous_ = false;
};

struct Nv2aDebugStats {
    uint32_t fifo_get = 0;
    uint32_t fifo_put = 0;
    uint32_t fifo_status = 0;
    uint64_t pending_updates = 0;
    uint32_t last_pending_get = 0;
    uint32_t last_pending_put = 0;
    bool worker_running = false;
};

void nv2a_init_mmio();
void nv2a_start_worker();
void nv2a_stop_worker();
Nv2aDebugStats nv2a_debug_stats();

} // namespace xbe::kernel
