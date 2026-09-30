#pragma once
#include <cstddef>
#include <cstdint>

namespace xbe {
namespace kernel {

extern "C" void* __attribute__((__stdcall__)) xbox_MmAllocateContiguousMemory(size_t NumberOfBytes);
extern "C" void __attribute__((__stdcall__)) xbox_MmFreeContiguousMemory(void* BaseAddress);

} // namespace kernel
} // namespace xbe
