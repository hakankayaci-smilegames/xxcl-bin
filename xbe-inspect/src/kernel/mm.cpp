#include "mm.hpp"
#include "../kernel.hpp"
#include <cstdlib>
#include <cstring>
#include <cstdio>

namespace xbe {
namespace kernel {

static uint8_t emergency_buffer[4096] __attribute__((aligned(4096)));

extern "C" void* __attribute__((__stdcall__)) xbox_MmAllocateContiguousMemory(uint32_t NumberOfBytes) {
    xbe::record_kernel_call(165);
    
    printf("[KERNEL] MmAllocateContiguousMemory cagrildi -> Boyut: %u bayt\n", NumberOfBytes);

    if (NumberOfBytes == 0) {
        return emergency_buffer;
    }

    void* ptr = nullptr;
    if (posix_memalign(&ptr, 4096, NumberOfBytes) == 0) {
        std::memset(ptr, 0, NumberOfBytes);
    } else {
        ptr = malloc(NumberOfBytes);
        if (ptr) std::memset(ptr, 0, NumberOfBytes);
        else ptr = emergency_buffer;
    }
    printf("[KERNEL] MmAllocateContiguousMemory -> Donen Ptr: %p\n", ptr);
    return ptr;
}

extern "C" void __attribute__((__stdcall__)) xbox_MmFreeContiguousMemory(void* BaseAddress) {
    xbe::record_kernel_call(167); // Usually 166 or 167 depending on Ex vs normal.
    printf("[KERNEL] MmFreeContiguousMemory(BaseAddress: %p) cagrildi.\n", BaseAddress);
    if (BaseAddress && BaseAddress != emergency_buffer) {
        free(BaseAddress);
    }
}

extern "C" NTSTATUS __attribute__((__stdcall__)) xbox_NtAllocateVirtualMemory(
    void** BaseAddress,
    uint32_t ZeroBits,
    size_t* RegionSize,
    uint32_t AllocationType,
    uint32_t Protect
) {
    xbe::record_kernel_call(184); // Update stat
    
    size_t size = RegionSize ? *RegionSize : 0;
    void* requested_base = BaseAddress ? *BaseAddress : nullptr;
    
    printf("[KERNEL] NtAllocateVirtualMemory(Ordinal 184) cagrildi -> Base: %p, Boyut: %zu bayt, AllocType: 0x%X\n", requested_base, size, AllocationType);

    if (size == 0) {
        printf("[KERNEL] NtAllocateVirtualMemory -> Sifir bayt istendi, STATUS_SUCCESS donuluyor.\n");
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
