#pragma once
#include <cstdint>
#include <cstddef>

extern "C" {
    void* __attribute__((__stdcall__)) xbox_ExAllocatePoolWithTag(uint32_t PoolType, size_t NumberOfBytes, uint32_t Tag);
}
