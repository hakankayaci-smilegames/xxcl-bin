#include "../kernel.hpp"
#include "ex.hpp"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <malloc.h>
#include <unordered_map>
#include <mutex>

static std::unordered_map<void*, size_t> pool_allocs;
static std::mutex pool_allocs_mutex;

static void track_pool(void* ptr, size_t size) {
    if (!ptr) return;
    std::lock_guard<std::mutex> lock(pool_allocs_mutex);
    pool_allocs[ptr] = size;
}

static size_t get_pool_alloc(void* ptr) {
    std::lock_guard<std::mutex> lock(pool_allocs_mutex);
    auto it = pool_allocs.find(ptr);
    if (it != pool_allocs.end()) return it->second;
    return 0;
}

extern "C" {
    NTSTATUS __attribute__((__stdcall__)) xbox_ExQueryNonVolatileSetting(
        uint32_t ValueIndex,
        uint32_t* Type,
        uint8_t* Value,
        uint32_t ValueLength,
        uint32_t* ResultLength
    ) {
        printf("[KERNEL] ExQueryNonVolatileSetting(Index: %u, Length: %u) called.\n", ValueIndex, ValueLength);
        
        if (ResultLength) *ResultLength = 4;
        if (Type) *Type = 4; // REG_DWORD by default
        
        uint32_t dummy_val = 0;
        
        switch(ValueIndex) {
            case 7: // XC_LANGUAGE
                dummy_val = 1; // English
                break;
            case 8: // XC_VIDEO
                dummy_val = 0; // standard
                break;
            case 9: // XC_AUDIO
                dummy_val = 0; // Stereo
                break;
            case 10: // XC_P_CONTROL_GAMES
                dummy_val = 0; // All ages
                break;
            case 17: // XC_MISC (0x11)
                dummy_val = 0;
                break;
            case 259: // XC_FACTORY_AV_REGION (0x103)
                dummy_val = 2; // PAL, or 1 for NTSC
                break;
            case 260: // XC_FACTORY_GAME_REGION (0x104)
                dummy_val = 4; // Europe/Rest of World. 1=NA, 2=Japan, 4=RoW
                break;
            default:
                dummy_val = 0;
                break;
        }
        
        if (Value && ValueLength >= 4) {
            *(uint32_t*)Value = dummy_val;
        } else if (Value && ValueLength > 0) {
            memset(Value, 0, ValueLength);
        }
        return 0;
    }


    void* __attribute__((__stdcall__)) xbox_ExAllocatePool(uint32_t NumberOfBytes) {
        xbe::record_kernel_call(14);
        void* ptr = malloc(NumberOfBytes + 4096);
        if(ptr) memset(ptr, 0, NumberOfBytes + 4096);
        track_pool(ptr, NumberOfBytes);
        printf("[KERNEL] ExAllocatePool(Size: %u) -> Ptr: %p\\n", NumberOfBytes, ptr);
        return ptr;
    }

    void* __attribute__((__stdcall__)) xbox_ExAllocatePoolWithTag(uint32_t NumberOfBytes, uint32_t Tag) {
        xbe::record_kernel_call(15);
        void* ptr = malloc(NumberOfBytes + 4096);
        if(ptr) memset(ptr, 0, NumberOfBytes + 4096);
        track_pool(ptr, NumberOfBytes);
        printf("[KERNEL] ExAllocatePoolWithTag(Size: %u, Tag: 0x%08X) -> Ptr: %p\\n", NumberOfBytes, Tag, ptr);
        return ptr;
    }

    void __attribute__((__stdcall__)) xbox_ExFreePool(void* P) {
        xbe::record_kernel_call(17);
        printf("[KERNEL] ExFreePool(Ptr: %p)\\n", P);
        free(P);
    }

    uint32_t __attribute__((__stdcall__)) xbox_ExQueryPoolBlockSize(void* PoolBlock) {
        xbe::record_kernel_call(23);
        size_t tracked = get_pool_alloc(PoolBlock);
        if (tracked > 0) {
            printf("[KERNEL] ExQueryPoolBlockSize(Ptr: %p) -> Size: %zu (Tracked)\\n", PoolBlock, tracked);
            return (uint32_t)tracked;
        }
        uint32_t size = (uint32_t)malloc_usable_size(PoolBlock);
        printf("[KERNEL] ExQueryPoolBlockSize(Ptr: %p) -> Size: %u (Fallback)\\n", PoolBlock, size);
        return size;
    }
}
