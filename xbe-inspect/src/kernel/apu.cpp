#include "apu.hpp"

#include "physical_memory.hpp"

#include <array>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace xbe::kernel {
namespace {

constexpr uintptr_t apu_base = 0xFE800000;
constexpr uintptr_t vp_base = 0xFE820000;
constexpr uintptr_t vp_pio_free_offset = 0x10;
constexpr uintptr_t vp_current_inbuf_sge = 0x804;
constexpr uintptr_t vp_current_inbuf_sge_offset = 0x808;
constexpr uintptr_t vp_current_outbuf_sge = 0x1800;
constexpr uintptr_t vp_current_outbuf_sge_offset = 0x1808;
constexpr uintptr_t papu_vpsgeaddr = 0x2030;
constexpr uint32_t sge_address_mask = 0xFFFFF000;
constexpr size_t queue_capacity = 1024;
constexpr size_t queue_mask = queue_capacity - 1;
constexpr uint32_t empty_pio_free_slots = 0x80;

static_assert((queue_capacity & queue_mask) == 0);
static_assert(std::atomic<uint32_t>::is_always_lock_free);

struct MmioWrite {
    uintptr_t address;
    uint32_t value;
};

struct QueueSlot {
    std::atomic<uint32_t> sequence{0};
    MmioWrite write{};
};

std::array<QueueSlot, queue_capacity> write_queue;
std::atomic<uint32_t> enqueue_position{0};
std::atomic<uint32_t> dequeue_position{0};
uint32_t vp_sge_base = 0;
uint32_t current_inbuf_sge = 0;
uint32_t current_outbuf_sge = 0;

bool dequeue_mmio_write(MmioWrite& write) {
    uint32_t position = dequeue_position.load(std::memory_order_relaxed);
    for (;;) {
        QueueSlot& slot = write_queue[position & queue_mask];
        const uint32_t sequence = slot.sequence.load(std::memory_order_acquire);
        const int32_t difference = static_cast<int32_t>(sequence - (position + 1));
        if (difference == 0) {
            if (dequeue_position.compare_exchange_weak(position, position + 1,
                                                       std::memory_order_relaxed)) {
                write = slot.write;
                slot.sequence.store(position + queue_capacity, std::memory_order_release);
                return true;
            }
        } else if (difference < 0) {
            return false;
        } else {
            position = dequeue_position.load(std::memory_order_relaxed);
        }
    }
}

bool store_sge_entry(uint32_t handle, uint32_t address) {
    if (vp_sge_base == 0 || handle > (UINT32_MAX - vp_sge_base) / 8) return false;
    const uint32_t entry_physical = vp_sge_base + handle * 8;
    auto* entry = static_cast<uint32_t*>(physical_memory_to_host(entry_physical, sizeof(uint32_t)));
    if (!entry) {
        if (std::getenv("XXCL_TRACE_APU")) {
            std::fprintf(stderr, "[APU SGE] unmapped table entry at 0x%08X (base 0x%08X handle 0x%X)\n",
                         entry_physical, vp_sge_base, handle);
        }
        return false;
    }
    *entry = address & sge_address_mask;
    if (std::getenv("XXCL_TRACE_APU")) {
        std::fprintf(stderr, "[APU SGE] table 0x%08X[%u] = 0x%08X\n",
                     vp_sge_base, handle, *entry);
    }
    return true;
}

void process_mmio_write(const MmioWrite& write) {
    if (write.address == apu_base + papu_vpsgeaddr) {
        vp_sge_base = write.value;
        return;
    }

    if (write.address == vp_base + vp_current_inbuf_sge) {
        current_inbuf_sge = write.value;
    } else if (write.address == vp_base + vp_current_inbuf_sge_offset) {
        store_sge_entry(current_inbuf_sge, write.value);
    } else if (write.address == vp_base + vp_current_outbuf_sge) {
        current_outbuf_sge = write.value;
    } else if (write.address == vp_base + vp_current_outbuf_sge_offset) {
        store_sge_entry(current_outbuf_sge, write.value);
    }
}

} // namespace

bool apu_initialize() {
    for (uint32_t index = 0; index < queue_capacity; ++index) {
        write_queue[index].sequence.store(index, std::memory_order_relaxed);
        write_queue[index].write = {};
    }
    enqueue_position.store(0, std::memory_order_relaxed);
    dequeue_position.store(0, std::memory_order_relaxed);
    vp_sge_base = 0;
    current_inbuf_sge = 0;
    current_outbuf_sge = 0;

    // The VP PIO queue is empty at reset, so all command slots are available.
    *reinterpret_cast<volatile uint32_t*>(vp_base + vp_pio_free_offset) =
        empty_pio_free_slots;
    return true;
}

bool apu_queue_mmio_write(uintptr_t address, uint32_t value) {
    if (address < apu_base || address >= apu_base + 0x80000) return false;

    uint32_t position = enqueue_position.load(std::memory_order_relaxed);
    for (;;) {
        QueueSlot& slot = write_queue[position & queue_mask];
        const uint32_t sequence = slot.sequence.load(std::memory_order_acquire);
        const int32_t difference = static_cast<int32_t>(sequence - position);
        if (difference == 0) {
            if (enqueue_position.compare_exchange_weak(position, position + 1,
                                                       std::memory_order_relaxed)) {
                slot.write = {address, value};
                slot.sequence.store(position + 1, std::memory_order_release);
                return true;
            }
        } else if (difference < 0) {
            return false;
        } else {
            position = enqueue_position.load(std::memory_order_relaxed);
        }
    }
}

size_t apu_process_mmio_writes() {
    size_t processed = 0;
    MmioWrite write{};
    while (dequeue_mmio_write(write)) {
        process_mmio_write(write);
        ++processed;
    }
    return processed;
}

} // namespace xbe::kernel
