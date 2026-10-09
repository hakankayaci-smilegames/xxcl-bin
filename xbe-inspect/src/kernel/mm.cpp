#include "mm.hpp"
#include "../kernel.hpp"
#include <cstdlib>
#include <sys/mman.h>
#include <cerrno>
#include <cstring>
#include <cstdio>

#include <unordered_map>
#include <mutex>
static std::unordered_map<void*, size_t> contiguous_allocs;
static std::mutex contiguous_allocs_mutex;

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
        free(BaseAddress);
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

    void* ptr = nullptr;
    
    size_t alloc_size = (NumberOfBytes + 4095) & ~4095;
    int res = posix_memalign(&ptr, align, alloc_size);

    if (res != 0 || !ptr) {
        printf("[KERNEL] MmAllocateContiguousMemoryEx -> posix_memalign basarisiz oldu! (Hata: %d)\n", res);
        return nullptr;
    }

    memset(ptr, 0, alloc_size);

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
    uint32_t virt = (uint32_t)BaseAddress;
    // On Xbox, physical addresses are usually virt & 0x7FFFFFFF (for 0x80000000 range)
    // For now, since we run in userspace, let's just return the same address or mask it.
    // If the game passes this to DMA, we might need the original pointer. But let's return virt.
    // Actually Xbox DMA might expect physical address < 64MB or 128MB.
    // Let's return virt & 0x7FFFFFFF;
    uint32_t phys = virt & 0x03FFFFFF;
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
