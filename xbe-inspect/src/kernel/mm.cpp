#include "mm.hpp"
#include "../kernel.hpp"
#include "guest_dma_memory.hpp"
#include "physical_memory.hpp"
#include <cstdlib>
#include <sys/mman.h>
#include <cerrno>
#include <cstring>
#include <cstdio>

#include <unordered_map>
#include <unordered_set>
#include <mutex>
static std::unordered_map<void*, size_t> contiguous_allocs;
static std::unordered_set<void*> guest_dma_allocs;
static std::mutex contiguous_allocs_mutex;
static uint32_t next_guest_dma_address = xbe::kernel::guest_dma_memory_start;

static void track_alloc(void* ptr, size_t size) {
    if (!ptr) return;
    std::lock_guard<std::mutex> lock(contiguous_allocs_mutex);
    contiguous_allocs[ptr] = size;
}

static size_t get_tracked_alloc(void* ptr) {
    std::lock_guard<std::mutex> lock(contiguous_allocs_mutex);
    auto it = contiguous_allocs.find(ptr);
    if (it != contiguous_allocs.end()) return it->second;
    return 0;
}

static void untrack_alloc(void* ptr) {
    std::lock_guard<std::mutex> lock(contiguous_allocs_mutex);
    contiguous_allocs.erase(ptr);
}

static bool untrack_guest_dma_alloc(void* ptr) {
    std::lock_guard<std::mutex> lock(contiguous_allocs_mutex);
    return guest_dma_allocs.erase(ptr) != 0;
}

static void* allocate_guest_dma_memory(size_t size, size_t alignment) {
    if (size > UINT32_MAX || alignment > UINT32_MAX) return nullptr;
    const uint32_t safe_alignment = static_cast<uint32_t>(alignment);
    std::lock_guard<std::mutex> lock(contiguous_allocs_mutex);
    uint32_t candidate = next_guest_dma_address;
    uint32_t aligned_candidate = 0;
    while (xbe::kernel::find_guest_dma_memory_address(
        static_cast<uint32_t>(size), safe_alignment, aligned_candidate, candidate)) {
        candidate = aligned_candidate;
        void* memory = mmap(reinterpret_cast<void*>(static_cast<uintptr_t>(candidate)), size,
                            PROT_READ | PROT_WRITE,
                            MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        if (memory != MAP_FAILED) {
            next_guest_dma_address = candidate + static_cast<uint32_t>(size);
            contiguous_allocs[memory] = size;
            guest_dma_allocs.insert(memory);
            return memory;
        }
        if (errno != EEXIST) return nullptr;
        if (candidate > UINT32_MAX - safe_alignment) return nullptr;
        candidate += safe_alignment;
    }
    return nullptr;
}


namespace xbe {
namespace kernel {

void mm_init_physical_memory_mirrors() {
    void* kseg0 = mmap((void*)0x80000000, 0x04000000, 
                       PROT_READ | PROT_WRITE, 
                       MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    if (kseg0 == MAP_FAILED) {
        printf("[MM] KSEG0 (0x80000000 - 64MB) mmap BASARISIZ! (Hata: %s)\n", strerror(errno));
    } else {
        printf("[MM] KSEG0 (0x80000000 - 64MB) fiziksel bellek aynasi basariyla eslendi.\n");
    }

    void* kseg1 = mmap((void*)0xA0000000, 0x04000000, 
                       PROT_READ | PROT_WRITE, 
                       MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    if (kseg1 == MAP_FAILED) {
        printf("[MM] KSEG1 (0xA0000000 - 64MB) mmap BASARISIZ! (Hata: %s)\n", strerror(errno));
    } else {
        printf("[MM] KSEG1 (0xA0000000 - 64MB) fiziksel bellek aynasi basariyla eslendi.\n");
    }
}


static uint8_t emergency_buffer[4096] __attribute__((aligned(4096)));
extern "C" void __attribute__((__stdcall__)) xbox_MmFreeContiguousMemory(void* BaseAddress) {
    xbe::record_kernel_call(167); // Usually 166 or 167 depending on Ex vs normal.
    printf("[KERNEL] MmFreeContiguousMemory(BaseAddress: %p) cagrildi.\n", BaseAddress);
    if (BaseAddress && BaseAddress != emergency_buffer) {
        xbe::kernel::physical_memory_unregister(BaseAddress);
        const size_t alloc_size = get_tracked_alloc(BaseAddress);
        const bool mapped_guest_dma = untrack_guest_dma_alloc(BaseAddress);
        untrack_alloc(BaseAddress);
        if (mapped_guest_dma) munmap(BaseAddress, alloc_size);
        else free(BaseAddress);
    }
}




extern "C" void* __attribute__((__stdcall__)) xbox_MmClaimGpuInstanceMemory(
    size_t NumberOfBytes,
    uint32_t* AllocationType
) {
    xbe::record_kernel_call(168);
    if (NumberOfBytes == 0) return nullptr;

    void* ptr = nullptr;
    
    size_t alloc_size = (NumberOfBytes + 4095) & ~4095;
    int res = posix_memalign(&ptr, 4096, alloc_size);

    if (res != 0 || !ptr) {
        if (AllocationType) *AllocationType = 0;
    printf("[KERNEL] MmClaimGpuInstanceMemory -> posix_memalign basarisiz! (Boyut: %zu)\n", NumberOfBytes);
        return nullptr;
    }

    memset(ptr, 0, alloc_size);
    if (!physical_memory_register(ptr, alloc_size, 4096, 0, 0)) {
        free(ptr);
        if (AllocationType) *AllocationType = 0;
        return nullptr;
    }

    if (AllocationType) *AllocationType = 0;
    track_alloc(ptr, alloc_size);
    printf("[KERNEL] MmClaimGpuInstanceMemory -> Boyut: %zu bayt, AllocType: %p => Donen Ptr: %p\n",
           NumberOfBytes, AllocationType, ptr);

    return ptr;
}

extern "C" void* __attribute__((__stdcall__)) xbox_MmAllocateContiguousMemoryEx(
    size_t NumberOfBytes,
    uint32_t LowestAcceptableAddress,
    uint32_t HighestAcceptableAddress,
    uint32_t Alignment,
    uint32_t ProtectionType
) {
    xbe::record_kernel_call(166);
    if (NumberOfBytes == 0) return nullptr;

    size_t align = (Alignment > 4096) ? Alignment : 4096;
    if ((align & (align - 1)) != 0) {
        align = 4096;
    }

    size_t alloc_size = (NumberOfBytes + 4095) & ~4095;
    void* ptr = allocate_guest_dma_memory(alloc_size, align);

    if (!ptr) {
        printf("[KERNEL] MmAllocateContiguousMemoryEx -> Xbox DMA adres araliginda tahsis basarisiz oldu.\n");
        return nullptr;
    }

    memset(ptr, 0, alloc_size);
    if (!physical_memory_register(ptr, alloc_size, align,
                                  LowestAcceptableAddress, HighestAcceptableAddress)) {
        untrack_guest_dma_alloc(ptr);
        untrack_alloc(ptr);
        munmap(ptr, alloc_size);
        return nullptr;
    }

    track_alloc(ptr, alloc_size);
    printf("[KERNEL] MmAllocateContiguousMemoryEx -> Boyut: %zu (0x%zX), Align: 0x%X, Protect: 0x%X => Ptr: %p\n",
           NumberOfBytes, NumberOfBytes, Alignment, ProtectionType, ptr);

    return ptr;
}

extern "C" void* __attribute__((__stdcall__)) xbox_MmAllocateContiguousMemory(size_t NumberOfBytes) {
    xbe::record_kernel_call(165);
    printf("[KERNEL] MmAllocateContiguousMemory cagrildi -> Boyut: %zu (0x%zX) bayt\n", NumberOfBytes, NumberOfBytes);

    if (NumberOfBytes == 0 || NumberOfBytes > 0x10000000) {
        return nullptr;
    }

    void* ptr = nullptr;
    
    size_t alloc_size = (NumberOfBytes + 4095) & ~4095;
    if (posix_memalign(&ptr, 4096, alloc_size) != 0) {

        printf("[KERNEL] MmAllocateContiguousMemory basarisiz! (posix_memalign hatasi)\n");
        return nullptr;
    }

    memset(ptr, 0, alloc_size);
    if (!physical_memory_register(ptr, alloc_size, 4096, 0, 0)) {
        free(ptr);
        return nullptr;
    }
    track_alloc(ptr, alloc_size);
    return ptr;
}

extern "C" NTSTATUS __attribute__((__stdcall__)) xbox_NtAllocateVirtualMemory(
    void** BaseAddress,
    uint32_t ZeroBits,
    size_t* RegionSize,
    uint32_t AllocationType,
    uint32_t Protect
) {
    xbe::record_kernel_call(184);
    
    size_t size = RegionSize ? *RegionSize : 0;
    void* requested_base = BaseAddress ? *BaseAddress : nullptr;
    
    printf("[KERNEL] NtAllocateVirtualMemory(Ordinal 184) cagrildi -> Base: %p, Boyut: %zu bayt, AllocType: 0x%X\n", requested_base, size, AllocationType);

    if (size == 0) {
        printf("[KERNEL] NtAllocateVirtualMemory -> Sifir bayt istendi, STATUS_SUCCESS donuluyor.\n");
        return 0; // STATUS_SUCCESS
    }

    
    if (requested_base != nullptr) {
        printf("[KERNEL] NtAllocateVirtualMemory -> Ozel adres istendi: %p, MEM_COMMIT simule ediliyor.\n", requested_base);
        if (BaseAddress) *BaseAddress = requested_base;
        if (RegionSize) *RegionSize = size;
        return 0; // STATUS_SUCCESS
    }

    void* ptr = nullptr;
    if (posix_memalign(&ptr, 4096, size) == 0) {
        std::memset(ptr, 0, size);
    } else {
        ptr = malloc(size);
        if (ptr) {
            std::memset(ptr, 0, size);
        } else {
            ptr = emergency_buffer;
        }
    }

    if (BaseAddress) {
        *BaseAddress = ptr; // PVOID* üzerinden pointer dondur!
    }
    if (RegionSize) {
        *RegionSize = size;
    }

    printf("[KERNEL] NtAllocateVirtualMemory -> Ayrilan Ptr: %p\n", ptr);
    return 0; // STATUS_SUCCESS
}

extern "C" NTSTATUS __attribute__((__stdcall__)) xbox_NtFreeVirtualMemory(
    void** BaseAddress,
    size_t* RegionSize,
    uint32_t FreeType
) {
    xbe::record_kernel_call(199);
    printf("[KERNEL] NtFreeVirtualMemory cagrildi -> BaseAddressPtr: %p, FreeType: 0x%X\n", BaseAddress, FreeType);
    if (BaseAddress && *BaseAddress && *BaseAddress != emergency_buffer) {
        free(*BaseAddress);
        *BaseAddress = nullptr;
    }
    return 0; // STATUS_SUCCESS
}

} // namespace kernel
} // namespace xbe

namespace xbe {
namespace kernel {

extern "C" NTSTATUS __attribute__((__stdcall__)) xbox_MmPersistContiguousMemory(
    void* BaseAddress,
    uint32_t NumberOfBytes,
    uint8_t Persist
) {
    xbe::record_kernel_call(178);
    printf("[KERNEL] MmPersistContiguousMemory(BaseAddress: %p, Size: %u, Persist: %u) cagrildi.\n", BaseAddress, NumberOfBytes, Persist);
    
    // Nothing to do for persistence on our high-level Linux wrapper
    return 0; // STATUS_SUCCESS
}

} // namespace kernel
} // namespace xbe

extern "C" void __attribute__((__stdcall__)) xbox_MmLockUnlockBufferPages(
    void* BaseAddress,
    uint32_t NumberOfBytes,
    uint8_t UnlockPages
) {
    xbe::record_kernel_call(175);
    // Since our emulator uses virtual memory mapped directly, we don't strictly need to lock pages in physical RAM.
    // We can just ignore this or implement mlock/munlock if necessary.
    printf("[KERNEL] MmLockUnlockBufferPages(Addr: %p, Size: %u, Unlock: %u) cagrildi.\n", BaseAddress, NumberOfBytes, UnlockPages);
}

extern "C" uint32_t __attribute__((__stdcall__)) xbox_MmGetPhysicalAddress(void* BaseAddress) {
    xbe::record_kernel_call(173);
    const uintptr_t virt = reinterpret_cast<uintptr_t>(BaseAddress);
    uint32_t phys = xbe::kernel::physical_memory_from_host(BaseAddress);
    if (phys == 0) {
        constexpr uintptr_t kseg0_end = 0x84000000;
        constexpr uintptr_t kseg1_end = 0xA4000000;
        if ((virt >= 0x80000000 && virt < kseg0_end) ||
            (virt >= 0xA0000000 && virt < kseg1_end)) {
            phys = static_cast<uint32_t>(virt & 0x1fffffff);
        } else {
            phys = xbe::kernel::physical_memory_map_page(BaseAddress);
        }
    }
    printf("[KERNEL] MmGetPhysicalAddress(Addr: %p) -> Phys: 0x%08X\n", BaseAddress, phys);
    return phys;
}

#include <malloc.h>

extern "C" uint32_t __attribute__((__stdcall__)) xbox_MmQueryAllocationSize(void* BaseAddress) {
    xbe::record_kernel_call(180);
    size_t tracked = get_tracked_alloc(BaseAddress);
    if (tracked > 0) {
        printf("[KERNEL] MmQueryAllocationSize(Addr: %p) -> Size: %zu (Tracked)\n", BaseAddress, tracked);
        return (uint32_t)tracked;
    }
    uint32_t size = (uint32_t)malloc_usable_size(BaseAddress);
    if (size % 4096 != 0) size = (size + 4095) & ~4095;
    printf("[KERNEL] MmQueryAllocationSize(Addr: %p) -> Size: %u (Fallback)\n", BaseAddress, size);
    return size;
}
