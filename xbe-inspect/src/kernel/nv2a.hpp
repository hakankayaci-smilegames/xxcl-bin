#pragma once

#include <cstdint>

namespace xbe {
namespace kernel {

struct Nv2aDebugStats {
    uint32_t fifo_get = 0;
    uint32_t fifo_put = 0;
    uint64_t fifo_pull_count = 0;
    uint32_t last_pull_get = 0;
    uint32_t last_pull_put = 0;
    bool worker_running = false;
};

void nv2a_init_mmio();
void nv2a_start_worker();
void nv2a_stop_worker();
Nv2aDebugStats nv2a_debug_stats();

} // namespace kernel
} // namespace xbe
