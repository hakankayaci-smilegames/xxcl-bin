#include "physical_memory.hpp"

#include <map>
#include <mutex>

namespace xbe::kernel {
namespace {

constexpr uint32_t physical_floor = 0x00010000;
constexpr uint32_t physical_ceiling = 0x03ffffff;
constexpr size_t guest_page_size = 0x1000;

struct Region {
    uintptr_t host_start;
    size_t size;
    uint32_t physical_start;
};

std::mutex regions_mutex;
std::map<uintptr_t, Region> regions_by_host;
std::map<uint32_t, Region> regions_by_physical;

uint64_t align_up(uint64_t value, size_t alignment) {
    const auto mask = static_cast<uint64_t>(alignment - 1);
    return (value + mask) & ~mask;
}

bool overlaps(uint64_t start, uint64_t size, uint64_t other_start, uint64_t other_size) {
    return start < other_start + other_size && other_start < start + size;
}

uint32_t allocate_physical_locked(uintptr_t host_start, size_t size, size_t alignment,
                                  uint32_t lowest_address, uint32_t highest_address) {
    if (host_start == 0 || size == 0 || alignment == 0 || (alignment & (alignment - 1)) != 0) return 0;
    if (lowest_address == 0) lowest_address = physical_floor;
    if (highest_address == 0 || highest_address > physical_ceiling) highest_address = physical_ceiling;
    if (lowest_address < physical_floor) lowest_address = physical_floor;
    if (lowest_address > highest_address) return 0;

    auto host_after = regions_by_host.upper_bound(host_start);
    if (host_after != regions_by_host.begin()) {
        const auto& previous = std::prev(host_after)->second;
        if (host_start >= previous.host_start && host_start < previous.host_start + previous.size) return 0;
    }
    if (host_after != regions_by_host.end() && host_start + size > host_after->second.host_start) return 0;

    uint64_t candidate = align_up(lowest_address, alignment);
    for (const auto& [physical_start, region] : regions_by_physical) {
        if (candidate + size <= physical_start) break;
        if (overlaps(candidate, size, physical_start, region.size)) {
            candidate = align_up(static_cast<uint64_t>(physical_start) + region.size, alignment);
        }
    }
    if (candidate > highest_address || size - 1 > highest_address - candidate) return 0;

    Region region{host_start, size, static_cast<uint32_t>(candidate)};
    regions_by_host.emplace(host_start, region);
    regions_by_physical.emplace(region.physical_start, region);
    return region.physical_start;
}

uint32_t host_to_physical_locked(uintptr_t host_address) {
    auto after = regions_by_host.upper_bound(host_address);
    if (after == regions_by_host.begin()) return 0;
    const auto& region = std::prev(after)->second;
    if (host_address < region.host_start || host_address - region.host_start >= region.size) return 0;
    return region.physical_start + static_cast<uint32_t>(host_address - region.host_start);
}

} // namespace

uint32_t physical_memory_register(void* host_address, size_t size, size_t alignment,
                                  uint32_t lowest_address, uint32_t highest_address) {
    if (!host_address || !size) return 0;
    std::lock_guard<std::mutex> lock(regions_mutex);
    const uintptr_t start = reinterpret_cast<uintptr_t>(host_address);
    auto existing = regions_by_host.find(start);
    if (existing != regions_by_host.end()) {
        const auto& region = existing->second;
        if (region.size == size && region.physical_start >= lowest_address &&
            (highest_address == 0 || region.physical_start + size - 1 <= highest_address)) {
            return region.physical_start;
        }
        return 0;
    }
    return allocate_physical_locked(start, size, alignment, lowest_address, highest_address);
}

uint32_t physical_memory_map_page(void* host_address) {
    if (!host_address) return 0;
    const uintptr_t page_start = reinterpret_cast<uintptr_t>(host_address) & ~(guest_page_size - 1);
    std::lock_guard<std::mutex> lock(regions_mutex);
    const uint32_t existing = host_to_physical_locked(page_start);
    if (existing != 0) return existing;
    return allocate_physical_locked(page_start, guest_page_size, guest_page_size,
                                    physical_floor, physical_ceiling);
}

uint32_t physical_memory_from_host(const void* host_address) {
    if (!host_address) return 0;
    std::lock_guard<std::mutex> lock(regions_mutex);
    return host_to_physical_locked(reinterpret_cast<uintptr_t>(host_address));
}

void* physical_memory_to_host(uint32_t physical_address, size_t size) {
    if (size == 0) return nullptr;
    std::lock_guard<std::mutex> lock(regions_mutex);
    auto after = regions_by_physical.upper_bound(physical_address);
    if (after == regions_by_physical.begin()) return nullptr;
    const auto& region = std::prev(after)->second;
    if (physical_address < region.physical_start) return nullptr;
    const size_t offset = physical_address - region.physical_start;
    if (offset >= region.size || size > region.size - offset) return nullptr;
    return reinterpret_cast<void*>(region.host_start + offset);
}

void* physical_memory_to_host_dma_alias(uint32_t dma_address, size_t size) {
    constexpr uint32_t dma_address_mask = 0x0fffffffu;
    if (size == 0 || dma_address > dma_address_mask || size > 0x10000000u) return nullptr;

    std::lock_guard<std::mutex> lock(regions_mutex);
    void* match = nullptr;
    for (const auto& [host_start, region] : regions_by_host) {
        const auto alias_start = static_cast<uint32_t>(host_start & dma_address_mask);
        const uint32_t offset = (dma_address - alias_start) & dma_address_mask;
        if (offset >= region.size || size > region.size - offset) continue;
        if (match) return nullptr;
        match = reinterpret_cast<void*>(host_start + offset);
    }
    return match;
}

bool physical_memory_unregister(void* host_address) {
    if (!host_address) return false;
    std::lock_guard<std::mutex> lock(regions_mutex);
    auto it = regions_by_host.find(reinterpret_cast<uintptr_t>(host_address));
    if (it == regions_by_host.end()) return false;
    regions_by_physical.erase(it->second.physical_start);
    regions_by_host.erase(it);
    return true;
}

} // namespace xbe::kernel
