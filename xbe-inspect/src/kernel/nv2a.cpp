#include "nv2a.hpp"
#include "nv2a_pfifo.hpp"
#include "physical_memory.hpp"
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
static std::atomic<uint64_t> g_nv2a_pending_updates{0};
static std::atomic<uint32_t> g_nv2a_last_pending_get{0};
static std::atomic<uint32_t> g_nv2a_last_pending_put{0};

void* nv2a_worker_loop(void* arg) {
    (void)arg;
    volatile uint32_t* ptimer_time_0 = (volatile uint32_t*)0xFD009400; // PTIMER TIME_0
    Nv2aFifoObserver fifo_observer;
    Nv2aPfifoState pfifo_state;
    auto* fifo_dma_put = reinterpret_cast<volatile uint32_t*>(nv2a_fifo_dma_put_address);
    auto* fifo_dma_get = reinterpret_cast<volatile uint32_t*>(nv2a_fifo_dma_get_address);
    auto* fifo_mode = reinterpret_cast<volatile uint32_t*>(nv2a_fifo_mode_address);
    auto* fifo_push1 = reinterpret_cast<volatile uint32_t*>(nv2a_fifo_push1_address);
    auto* fifo_dma_instance = reinterpret_cast<volatile uint32_t*>(nv2a_fifo_dma_instance_address);
    auto* ramin = reinterpret_cast<const uint32_t*>(nv2a_ramin_base_address);
    uint32_t last_reported_dma_instance = UINT32_MAX;
    uint32_t last_reported_dma_get = UINT32_MAX;
    auto* pfb_write_buffer_cache = reinterpret_cast<volatile uint32_t*>(nv2a_pfb_write_buffer_cache_address);

    while (g_nv2a_running.load()) {
        nv2a_complete_pfb_write_buffer_flush(pfb_write_buffer_cache);

        const uint32_t dma_instance = (*fifo_dma_instance & 0xffff) << 4;
        const auto dma_object = decode_nv2a_dma_object(
            std::span<const uint32_t>(ramin, nv2a_ramin_size / sizeof(uint32_t)),
            dma_instance);
        if (dma_object.status == Nv2aDmaObjectStatus::ok &&
            dma_instance != last_reported_dma_instance) {
            printf("[NV2A] DMA object RAMIN+0x%05X: flags=%08X limit=%08X frame=%08X\n",
                   dma_instance, dma_object.flags, dma_object.limit, dma_object.frame);
            last_reported_dma_instance = dma_instance;
        }

        // PTIMER ilerletmesi mevcut saat davranışını korur.
        if (ptimer_time_0) {
            *ptimer_time_0 += 1000;
        }

        const uint32_t active_channel = *fifo_push1 & 0xf;
        auto* user_dma_base = reinterpret_cast<volatile uint32_t*>(
            nv2a_user_base_address + active_channel * 0x10000);
        pfifo_state.sync_active_dma_registers(
            *fifo_mode, *fifo_push1,
            user_dma_base + nv2a_user_dma_get_offset / sizeof(uint32_t),
            user_dma_base + nv2a_user_dma_put_offset / sizeof(uint32_t),
            fifo_dma_get, fifo_dma_put);

        if (dma_object.status == Nv2aDmaObjectStatus::ok &&
            *fifo_dma_get != *fifo_dma_put && (*fifo_dma_get & 3u) == 0) {
            constexpr uint64_t guest_physical_memory_size = 0x04000000;
            const uint32_t object_base = nv2a_dma_object_base_address(dma_object);
            const uint64_t command_address =
                static_cast<uint64_t>(object_base) + *fifo_dma_get;
            const volatile uint32_t* command_word = nullptr;

            if (command_address <= dma_object.limit &&
                sizeof(uint32_t) - 1 <= dma_object.limit - command_address &&
                command_address <= UINT32_MAX) {
                command_word = static_cast<const volatile uint32_t*>(
                    physical_memory_to_host_dma_alias(
                        static_cast<uint32_t>(command_address), sizeof(uint32_t)));
                if (!command_word && command_address < guest_physical_memory_size) {
                    command_word = reinterpret_cast<const volatile uint32_t*>(
                        0x80000000u + static_cast<uint32_t>(command_address));
                } else if (!command_word) {
                    command_word = static_cast<const volatile uint32_t*>(
                        physical_memory_to_host(static_cast<uint32_t>(command_address),
                                                sizeof(uint32_t)));
                }
            }

            if (!command_word) {
                if (*fifo_dma_get != last_reported_dma_get) {
                    printf("[NV2A] DMA GET 0x%08X okunamiyor; isaretci ilerletilmedi.\n",
                           *fifo_dma_get);
                    last_reported_dma_get = *fifo_dma_get;
                }
            } else {
                const uint32_t word = *command_word;
                if ((word & 3u) != 0) {
                    const auto control = process_nv2a_dma_control_word(
                        *fifo_dma_get, word, object_base, dma_object.limit);
                    if (control.status == Nv2aDmaControlStatus::jump) {
                        const uint32_t previous_get = *fifo_dma_get;
                        *fifo_dma_get = control.next_get;
                        pfifo_state.sync_active_dma_registers(
                            *fifo_mode, *fifo_push1,
                            user_dma_base + nv2a_user_dma_get_offset / sizeof(uint32_t),
                            user_dma_base + nv2a_user_dma_put_offset / sizeof(uint32_t),
                            fifo_dma_get, fifo_dma_put);
                        printf("[NV2A] DMA JUMP 0x%08X -> 0x%08X; GET ilerletildi.\n",
                               previous_get, control.next_get);
                        last_reported_dma_get = UINT32_MAX;
                    } else if (control.status != Nv2aDmaControlStatus::invalid &&
                               *fifo_dma_get != last_reported_dma_get) {
                        printf("[NV2A] DMA kontrol kelimesi 0x%08X desteklenmiyor veya nesne sinirini asiyor; GET sabit.\n",
                               word);
                        last_reported_dma_get = *fifo_dma_get;
                    }
                }
            }
        }

        // USER DMA işaretçilerini seçili kanalın PFIFO durumuyla eşleştir.
        if (fifo_observer.observe(*fifo_dma_get, *fifo_dma_put)) {
            g_nv2a_last_pending_get.store(fifo_observer.get(), std::memory_order_relaxed);
            g_nv2a_last_pending_put.store(fifo_observer.put(), std::memory_order_relaxed);
            g_nv2a_pending_updates.store(fifo_observer.pending_updates(), std::memory_order_relaxed);
        }

        usleep(500);
    }
    return nullptr;
}

void nv2a_start_worker() {
    if (g_nv2a_running.load()) return;
    g_nv2a_pending_updates.store(0, std::memory_order_relaxed);
    g_nv2a_last_pending_get.store(0, std::memory_order_relaxed);
    g_nv2a_last_pending_put.store(0, std::memory_order_relaxed);
    g_nv2a_running.store(true);

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
    stats.pending_updates = g_nv2a_pending_updates.load(std::memory_order_relaxed);
    stats.last_pending_get = g_nv2a_last_pending_get.load(std::memory_order_relaxed);
    stats.last_pending_put = g_nv2a_last_pending_put.load(std::memory_order_relaxed);

    auto* fifo_dma_get = reinterpret_cast<volatile uint32_t*>(nv2a_fifo_dma_get_address);
    auto* fifo_dma_put = reinterpret_cast<volatile uint32_t*>(nv2a_fifo_dma_put_address);
    auto* fifo_status = reinterpret_cast<volatile uint32_t*>(nv2a_fifo_status_address);
    stats.fifo_get = *fifo_dma_get;
    stats.fifo_put = *fifo_dma_put;
    stats.fifo_status = *fifo_status;
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
