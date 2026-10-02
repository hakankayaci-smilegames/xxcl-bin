#include "mm.hpp"
#include "../kernel.hpp"
#include <cstdlib>
#include <cstring>
#include <cstdio>

namespace xbe {
namespace kernel {

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
    int res = posix_memalign(&ptr, 4096, NumberOfBytes);
    if (res != 0 || !ptr) {
        if (AllocationType) *AllocationType = 0;
    printf("[KERNEL] MmClaimGpuInstanceMemory -> posix_memalign basarisiz! (Boyut: %zu)\n", NumberOfBytes);
        return nullptr;
    }

    memset(ptr, 0, NumberOfBytes);

    if (AllocationType) *AllocationType = 0;
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
    int res = posix_memalign(&ptr, align, NumberOfBytes);
    if (res != 0 || !ptr) {
        printf("[KERNEL] MmAllocateContiguousMemoryEx -> posix_memalign basarisiz oldu! (Hata: %d)\n", res);
        return nullptr;
    }

    memset(ptr, 0, NumberOfBytes);

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
    if (posix_memalign(&ptr, 4096, NumberOfBytes) != 0) {
        printf("[KERNEL] MmAllocateContiguousMemory basarisiz! (posix_memalign hatasi)\n");
        return nullptr;
    }

    memset(ptr, 0, NumberOfBytes);
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
