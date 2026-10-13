#include "nv2a.hpp"
#include <sys/mman.h>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <pthread.h>
#include <unistd.h>
#include <atomic>

namespace xbe {
namespace kernel {

static pthread_t g_nv2a_worker_th;
static std::atomic<bool> g_nv2a_running{false};
static std::atomic<uint64_t> g_nv2a_fifo_pull_count{0};
static std::atomic<uint32_t> g_nv2a_last_pull_get{0};
static std::atomic<uint32_t> g_nv2a_last_pull_put{0};

void* nv2a_worker_loop(void* arg) {
    (void)arg;
    volatile uint32_t* pfifo_cache1_status = (volatile uint32_t*)0xFD100410;
    volatile uint32_t* ptimer_time_0 = (volatile uint32_t*)0xFD009400; // PTIMER TIME_0
    
    // DMA PushBuffer (NV03_CHANNEL_USER_PUT / GET)
    volatile uint32_t* channel_user_put = (volatile uint32_t*)0xFD800040;
    volatile uint32_t* channel_user_get = (volatile uint32_t*)0xFD800044;

    while (g_nv2a_running.load()) {
        // 1. PFIFO CACHE1 BUSY bitini (Bit 16) temizle
        if (*pfifo_cache1_status & 0x00010000) {
            *pfifo_cache1_status &= ~0x00010000;
        }

        // 2. Basit PTIMER artirimi
        if (ptimer_time_0) {
            *ptimer_time_0 += 1000;
        }

        // 3. DMA PushBuffer tuketimi (GET == PUT)
        if (*channel_user_get != *channel_user_put) {
            g_nv2a_last_pull_get.store(*channel_user_get, std::memory_order_relaxed);
            g_nv2a_last_pull_put.store(*channel_user_put, std::memory_order_relaxed);
            g_nv2a_fifo_pull_count.fetch_add(1, std::memory_order_relaxed);
            *channel_user_get = *channel_user_put;
        }

        usleep(500);
    }
    return nullptr;
}

void nv2a_start_worker() {
    if (g_nv2a_running.load()) return;
    g_nv2a_fifo_pull_count.store(0, std::memory_order_relaxed);
    g_nv2a_last_pull_get.store(0, std::memory_order_relaxed);
    g_nv2a_last_pull_put.store(0, std::memory_order_relaxed);
    g_nv2a_running.store(true);

    volatile uint32_t* pfifo_cache1_status = (volatile uint32_t*)0xFD100410;
    *pfifo_cache1_status = 0;

    pthread_create(&g_nv2a_worker_th, nullptr, nv2a_worker_loop, nullptr);
    printf("[NV2A] Asenkron GPU Worker Thread (FIFO Puller / PTIMER) baslatildi.\n");
}

void nv2a_stop_worker() {
    if (!g_nv2a_running.load()) return;
    g_nv2a_running.store(false);
    pthread_join(g_nv2a_worker_th, nullptr);
}

Nv2aDebugStats nv2a_debug_stats() {
    Nv2aDebugStats stats;
    stats.worker_running = g_nv2a_running.load();
    stats.fifo_pull_count = g_nv2a_fifo_pull_count.load(std::memory_order_relaxed);
    stats.last_pull_get = g_nv2a_last_pull_get.load(std::memory_order_relaxed);
    stats.last_pull_put = g_nv2a_last_pull_put.load(std::memory_order_relaxed);

    auto* channel_user_get = reinterpret_cast<volatile uint32_t*>(0xFD800044);
    auto* channel_user_put = reinterpret_cast<volatile uint32_t*>(0xFD800040);
    stats.fifo_get = *channel_user_get;
    stats.fifo_put = *channel_user_put;
    return stats;
}

void nv2a_init_mmio() {
    // 0xFD000000 - 16MB mapped memory
    void* mmio = mmap((void*)0xFD000000, 0x01000000, PROT_READ | PROT_WRITE, 
                      MAP_SHARED | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    if (mmio == MAP_FAILED) {
        printf("[NV2A] MMIO 0xFD000000 mmap BASARISIZ! (Hata: %s)\n", strerror(errno));
    } else {
        printf("[NV2A] NV2A MMIO sahte donanim alani (0xFD000000 - 16MB) basariyla eslendi.\n");
    }
}

} // namespace kernel
} // namespace xbe
