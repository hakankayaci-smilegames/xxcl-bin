#pragma once
#include <cstddef>
#include <cstdint>

typedef int32_t NTSTATUS;

namespace xbe {
namespace kernel {

extern "C" void* __attribute__((__stdcall__)) xbox_MmAllocateContiguousMemory(uint32_t NumberOfBytes);
extern "C" void __attribute__((__stdcall__)) xbox_MmFreeContiguousMemory(void* BaseAddress);

extern "C" void* __attribute__((__stdcall__)) xbox_MmAllocateContiguousMemory(size_t NumberOfBytes);

    extern "C" void* __attribute__((__stdcall__)) xbox_MmAllocateContiguousMemoryEx(
        size_t NumberOfBytes,
        uint32_t LowestAcceptableAddress,
        uint32_t HighestAcceptableAddress,
        uint32_t Alignment,
        uint32_t ProtectionType
    );

    extern "C" NTSTATUS __attribute__((__stdcall__)) xbox_NtAllocateVirtualMemory(
    void** BaseAddress,
    uint32_t ZeroBits,
    size_t* RegionSize,
    uint32_t AllocationType,
    uint32_t Protect
);

extern "C" NTSTATUS __attribute__((__stdcall__)) xbox_NtFreeVirtualMemory(
    void** BaseAddress,
    size_t* RegionSize,
    uint32_t FreeType
);

extern "C" NTSTATUS __attribute__((__stdcall__)) xbox_MmPersistContiguousMemory(
    void* BaseAddress,
    uint32_t NumberOfBytes,
    uint8_t Persist
);

} // namespace kernel
} // namespace xbe
