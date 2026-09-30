#include "mm.hpp"
#include "../kernel.hpp"
#include <cstdlib>
#include <cstring>
#include <iostream>

namespace xbe {
namespace kernel {

extern "C" void* __attribute__((__stdcall__)) xbox_MmAllocateContiguousMemory(size_t NumberOfBytes) {
    xbe::record_kernel_call(184);
    if (NumberOfBytes == 0) return nullptr;

    void* ptr = nullptr;
    // 32-bit x86 mimarisinde sayfa hizalamasi (4096 bayt)
    if (posix_memalign(&ptr, 4096, NumberOfBytes) == 0) {
        std::memset(ptr, 0, NumberOfBytes);
        return ptr;
    }
    return nullptr;
}

extern "C" void __attribute__((__stdcall__)) xbox_MmFreeContiguousMemory(void* BaseAddress) {
    xbe::record_kernel_call(186);
    if (BaseAddress) {
        free(BaseAddress);
    }
}

} // namespace kernel
} // namespace xbe
