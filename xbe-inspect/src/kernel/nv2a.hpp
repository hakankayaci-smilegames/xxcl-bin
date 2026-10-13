#pragma once

#include <cstdint>

namespace xbe::kernel {

inline constexpr uintptr_t nv2a_fifo_dma_put_address = 0xFD003240;
inline constexpr uintptr_t nv2a_fifo_dma_get_address = 0xFD003244;
inline constexpr uintptr_t nv2a_fifo_status_address = 0xFD003214;

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
