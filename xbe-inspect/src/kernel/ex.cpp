#include "ex.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" {
    void* __attribute__((__stdcall__)) xbox_ExAllocatePoolWithTag(uint32_t PoolType, size_t NumberOfBytes, uint32_t Tag) {
        void* ptr = nullptr;
        
        // 16-byte aligned allocation
        if (posix_memalign(&ptr, 16, NumberOfBytes) != 0) {
            ptr = nullptr;
        }

        if (ptr) {
            memset(ptr, 0, NumberOfBytes);
        }

        printf("[KERNEL] ExAllocatePoolWithTag(Ordinal 24?) cagrildi -> PoolType: %u, Boyut: %zu bayt, Tag: 0x%08X ('%.4s') -> Ptr: %p\n", 
               PoolType, NumberOfBytes, Tag, (const char*)&Tag, ptr);

        return ptr;
    }
}
