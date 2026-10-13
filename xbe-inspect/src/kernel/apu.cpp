#include "apu.hpp"

#include "apu_dsp.hpp"
#include "physical_memory.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <vector>

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
constexpr uintptr_t papu_gpsaddr = 0x2040;
constexpr uintptr_t papu_gpsmax_sge = 0x20D4;
constexpr uintptr_t gp_region = 0x30000;
constexpr uintptr_t gp_reset_register = 0xFFFC;
constexpr uint32_t gp_reset_bit = 1;
constexpr uint32_t gp_dsp_reset_bit = 2;
constexpr uint32_t sge_address_mask = 0xFFFFF000;
constexpr size_t queue_capacity = 1024;
constexpr size_t queue_mask = queue_capacity - 1;
constexpr uint32_t empty_pio_free_slots = 0x80;
constexpr uint32_t guest_page_size = 0x1000;
constexpr auto gp_dsp_frame_period = std::chrono::microseconds(667);

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
uint32_t gp_sge_base = 0;
uint32_t gp_max_sge = 0;
uint32_t gp_reset_value = 0;
ApuDspCore gp_dsp;
std::atomic<bool> apu_worker_running{false};
std::atomic<uint64_t> gp_dsp_instructions_executed{0};
std::thread apu_worker;
uint32_t last_reported_dsp_fault_pc = UINT32_MAX;
uint32_t last_reported_dsp_fault_instruction = UINT32_MAX;

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
    if (write.address == apu_base + papu_gpsaddr) {
        gp_sge_base = write.value;
        return;
    }
    if (write.address == apu_base + papu_gpsmax_sge) {
        gp_max_sge = write.value;
        return;
    }
    if (write.address == apu_base + gp_region + gp_reset_register) {
        const uint32_t previous = gp_reset_value;
        gp_reset_value = write.value;
        const uint32_t running_mask = gp_reset_bit | gp_dsp_reset_bit;
        if ((previous & running_mask) != running_mask &&
            (write.value & running_mask) == running_mask) {
            last_reported_dsp_fault_pc = UINT32_MAX;
            last_reported_dsp_fault_instruction = UINT32_MAX;
            const bool loaded = gp_dsp.bootstrap(gp_sge_base, gp_max_sge);
            if (std::getenv("XXCL_TRACE_APU")) {
                if (loaded) {
                    std::fprintf(stderr,
                                 "[APU DSP GP] bootstrap SGE=0x%08X max=%u P[0]=0x%06X P[0x7FF]=0x%06X\n",
                                 gp_sge_base, gp_max_sge, gp_dsp.program_word(0),
                                 gp_dsp.program_word(ApuDspCore::bootstrap_word_count - 1));
                    for (uint32_t address = 0; address < 16; ++address) {
                        std::fprintf(stderr, "[APU DSP GP] P[%03X]=%06X\n",
                                     address, gp_dsp.program_word(address));
                    }
                    for (uint32_t address = 0x100; address < 0x116; ++address) {
                        std::fprintf(stderr, "[APU DSP GP] P[%03X]=%06X\n",
                                     address, gp_dsp.program_word(address));
                    }
                } else {
                    std::fprintf(stderr,
                                 "[APU DSP GP] bootstrap failed SGE=0x%08X max=%u\n",
                                 gp_sge_base, gp_max_sge);
                }
            }
        }
        return;
    }

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
    apu_stop_worker();
    for (uint32_t index = 0; index < queue_capacity; ++index) {
        write_queue[index].sequence.store(index, std::memory_order_relaxed);
        write_queue[index].write = {};
    }
    enqueue_position.store(0, std::memory_order_relaxed);
    dequeue_position.store(0, std::memory_order_relaxed);
    vp_sge_base = 0;
    current_inbuf_sge = 0;
    current_outbuf_sge = 0;
    gp_sge_base = 0;
    gp_max_sge = 0;
    gp_reset_value = 0;
    gp_dsp = ApuDspCore{};

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

size_t apu_process_mmio_writes(size_t max_writes) {
    size_t processed = 0;
    MmioWrite write{};
    while (processed < max_writes && dequeue_mmio_write(write)) {
        process_mmio_write(write);
        ++processed;
    }
    return processed;
}

size_t apu_run_gp_dsp(uint32_t instruction_budget) {
    static bool reported_run_state = false;
    const bool trace = std::getenv("XXCL_TRACE_APU") != nullptr;
    if (trace && !reported_run_state &&
        (gp_reset_value & (gp_reset_bit | gp_dsp_reset_bit)) ==
            (gp_reset_bit | gp_dsp_reset_bit)) {
        std::fprintf(stderr, "[APU DSP GP] run enabled reset=0x%X PC=0x%03X\n",
                     gp_reset_value, gp_dsp.program_counter());
        reported_run_state = true;
    }
    if ((gp_reset_value & (gp_reset_bit | gp_dsp_reset_bit)) !=
        (gp_reset_bit | gp_dsp_reset_bit)) return 0;

    uint32_t executed = 0;
    while (executed < instruction_budget) {
        const uint32_t pc = gp_dsp.program_counter();
        const ApuDspCore::StepResult result = gp_dsp.step();
        if (result != ApuDspCore::StepResult::executed) {
            const uint32_t instruction = gp_dsp.program_word(pc);
            if (std::getenv("XXCL_TRACE_APU") &&
                (pc != last_reported_dsp_fault_pc ||
                 instruction != last_reported_dsp_fault_instruction)) {
                std::fprintf(stderr,
                             "[APU DSP GP] stopped result=%u PC=0x%03X P=0x%06X\n",
                             static_cast<unsigned>(result), pc, instruction);
                last_reported_dsp_fault_pc = pc;
                last_reported_dsp_fault_instruction = instruction;
            }
            break;
        }
        ++executed;
    }
    gp_dsp_instructions_executed.fetch_add(executed, std::memory_order_relaxed);
    return executed;
}

ApuDspDebugStats apu_gp_dsp_debug_stats() {
    ApuDspDebugStats stats;
    stats.program_counter = gp_dsp.program_counter();
    stats.program_words[0] = gp_dsp.program_word(stats.program_counter);
    stats.program_words[1] = gp_dsp.program_word(stats.program_counter + 1);
    stats.program_words[2] = gp_dsp.program_word(stats.program_counter + 2);
    stats.instructions_executed =
        gp_dsp_instructions_executed.load(std::memory_order_relaxed);
    return stats;
}

bool apu_start_worker() {
    bool expected = false;
    if (!apu_worker_running.compare_exchange_strong(expected, true,
                                                    std::memory_order_acq_rel)) {
        return true;
    }
    try {
        apu_worker = std::thread([] {
            const bool trace = std::getenv("XXCL_TRACE_APU") != nullptr;
            if (trace) std::fprintf(stderr, "[APU] device worker started\n");
            bool reported_execution = false;
            bool reported_queue = false;
            uint32_t worker_iterations = 0;
            auto next_frame = std::chrono::steady_clock::now();
            while (apu_worker_running.load(std::memory_order_acquire)) {
                const size_t processed = apu_process_mmio_writes(256);
                if (trace && !reported_queue && processed != 0) {
                    std::fprintf(stderr, "[APU] worker processed %zu MMIO writes\n", processed);
                    reported_queue = true;
                }
                gp_dsp.start_frame();
                const size_t executed = apu_run_gp_dsp(27000);
                if (trace && !reported_execution && executed != 0) {
                    std::fprintf(stderr,
                                 "[APU DSP GP] worker executed %zu instructions PC=0x%03X\n",
                                 executed, gp_dsp.program_counter());
                    reported_execution = true;
                }
                if (trace && ++worker_iterations % 1000 == 0) {
                    std::fprintf(stderr,
                                 "[APU DSP GP] worker checkpoint PC=0x%03X P=%06X/%06X/%06X executed=%zu\n",
                                 gp_dsp.program_counter(),
                                 gp_dsp.program_word(gp_dsp.program_counter()),
                                 gp_dsp.program_word(gp_dsp.program_counter() + 1),
                                 gp_dsp.program_word(gp_dsp.program_counter() + 2), executed);
                }
                next_frame += gp_dsp_frame_period;
                std::this_thread::sleep_until(next_frame);
                if (next_frame < std::chrono::steady_clock::now()) {
                    next_frame = std::chrono::steady_clock::now();
                }
            }
        });
    } catch (...) {
        apu_worker_running.store(false, std::memory_order_release);
        return false;
    }
    return true;
}

void apu_stop_worker() {
    if (!apu_worker_running.exchange(false, std::memory_order_acq_rel)) return;
    if (apu_worker.joinable()) apu_worker.join();
}

bool apu_gp_program_read(uint32_t address, uint32_t& value) {
    if (address >= ApuDspCore::program_word_count) return false;
    value = gp_dsp.program_word(address);
    return true;
}

bool apu_sge_transfer(uint32_t sge_table_physical, uint32_t max_sge,
                      uint32_t scratch_offset, std::span<std::byte> buffer,
                      bool write_to_guest) {
    if (buffer.empty()) return true;

    struct Segment {
        std::byte* guest_address;
        size_t buffer_offset;
        size_t size;
    };
    std::vector<Segment> segments;
    size_t remaining = buffer.size();
    size_t buffer_offset = 0;
    uint64_t scratch_address = scratch_offset;

    while (remaining != 0) {
        const uint64_t page_index = scratch_address / guest_page_size;
        const size_t page_offset = static_cast<size_t>(scratch_address % guest_page_size);
        if (page_index > max_sge || page_index > (UINT32_MAX - sge_table_physical) / 8) {
            return false;
        }

        const uint64_t entry_physical = static_cast<uint64_t>(sge_table_physical) + page_index * 8;
        if (entry_physical > UINT32_MAX - sizeof(uint32_t)) return false;
        const auto* entry = static_cast<const uint32_t*>(
            physical_memory_to_host(static_cast<uint32_t>(entry_physical), sizeof(uint32_t)));
        if (!entry) return false;

        uint32_t guest_page = 0;
        std::memcpy(&guest_page, entry, sizeof(guest_page));
        const size_t segment_size = std::min(remaining, guest_page_size - page_offset);
        const uint64_t guest_physical = static_cast<uint64_t>(guest_page) + page_offset;
        if (guest_physical > UINT32_MAX || segment_size - 1 > UINT32_MAX - guest_physical) {
            return false;
        }
        auto* guest = static_cast<std::byte*>(physical_memory_to_host(
            static_cast<uint32_t>(guest_physical), segment_size));
        if (!guest) return false;

        segments.push_back({guest, buffer_offset, segment_size});
        scratch_address += segment_size;
        buffer_offset += segment_size;
        remaining -= segment_size;
    }

    for (const Segment& segment : segments) {
        if (write_to_guest) {
            std::memcpy(segment.guest_address, buffer.data() + segment.buffer_offset, segment.size);
        } else {
            std::memcpy(buffer.data() + segment.buffer_offset, segment.guest_address, segment.size);
        }
    }
    return true;
}

} // namespace xbe::kernel
