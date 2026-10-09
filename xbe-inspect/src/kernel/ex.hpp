#pragma once
#include <cstdint>
#include <cstddef>

typedef int32_t NTSTATUS;

extern "C" {
    NTSTATUS __attribute__((__stdcall__)) xbox_ExQueryNonVolatileSetting(
        uint32_t ValueIndex,
        uint32_t* Type,
        uint8_t* Value,
        uint32_t ValueLength,
        uint32_t* ResultLength
    );
}

extern "C" {
    void* __attribute__((__stdcall__)) xbox_ExAllocatePool(uint32_t NumberOfBytes);
    void* __attribute__((__stdcall__)) xbox_ExAllocatePoolWithTag(uint32_t NumberOfBytes, uint32_t Tag);
    void __attribute__((__stdcall__)) xbox_ExFreePool(void* P);
}


extern "C" uint32_t __attribute__((__stdcall__)) xbox_ExQueryPoolBlockSize(void* PoolBlock);
