#include "apu.hpp"

#include <cstdint>

namespace xbe::kernel {
namespace {

constexpr uintptr_t apu_vp_base = 0xFE820000;
constexpr uintptr_t vp_pio_free_offset = 0x10;
constexpr uint32_t empty_pio_free_slots = 0x80;

} // namespace

bool apu_initialize() {
    // The VP PIO queue is empty at reset, so all command slots are available.
    *reinterpret_cast<volatile uint32_t*>(apu_vp_base + vp_pio_free_offset) =
        empty_pio_free_slots;
    return true;
}

} // namespace xbe::kernel
