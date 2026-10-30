import re

with open('xbe-inspect/src/kernel/mm.cpp', 'r') as f:
    content = f.read()

# Replace MmAllocateContiguousMemory...
tracking_code = """
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
"""

# Insert tracking_code after #include <cstdio>
content = content.replace("#include <cstdio>", "#include <cstdio>\n" + tracking_code)

# Add track_alloc to MmClaimGpuInstanceMemory
content = content.replace(
    'printf("[KERNEL] MmClaimGpuInstanceMemory -> Boyut: %zu bayt, AllocType: %p => Donen Ptr: %p\\n",',
    'track_alloc(ptr, alloc_size);\n    printf("[KERNEL] MmClaimGpuInstanceMemory -> Boyut: %zu bayt, AllocType: %p => Donen Ptr: %p\\n",'
)

# Add track_alloc to MmAllocateContiguousMemoryEx
content = content.replace(
    'printf("[KERNEL] MmAllocateContiguousMemoryEx -> Boyut: %zu (0x%zX), Align: 0x%X, Protect: 0x%X => Ptr: %p\\n",',
    'track_alloc(ptr, alloc_size);\n    printf("[KERNEL] MmAllocateContiguousMemoryEx -> Boyut: %zu (0x%zX), Align: 0x%X, Protect: 0x%X => Ptr: %p\\n",'
)

# Add track_alloc to MmAllocateContiguousMemory
content = content.replace(
    'memset(ptr, 0, alloc_size);\n    return ptr;',
    'memset(ptr, 0, alloc_size);\n    track_alloc(ptr, alloc_size);\n    return ptr;'
)

# Fix MmQueryAllocationSize
query_pattern = r"(extern \"C\" uint32_t __attribute__\(\(__stdcall__\)\) xbox_MmQueryAllocationSize\(void\* BaseAddress\) \{.*?return size;\n\})"
new_query = """extern "C" uint32_t __attribute__((__stdcall__)) xbox_MmQueryAllocationSize(void* BaseAddress) {
    xbe::record_kernel_call(180);
    size_t tracked = get_tracked_alloc(BaseAddress);
    if (tracked > 0) {
        printf("[KERNEL] MmQueryAllocationSize(Addr: %p) -> Size: %zu (Tracked)\\n", BaseAddress, tracked);
        return (uint32_t)tracked;
    }
    uint32_t size = (uint32_t)malloc_usable_size(BaseAddress);
    if (size % 4096 != 0) size = (size + 4095) & ~4095;
    printf("[KERNEL] MmQueryAllocationSize(Addr: %p) -> Size: %u (Fallback)\\n", BaseAddress, size);
    return size;
}"""
content = re.sub(query_pattern, new_query, content, flags=re.DOTALL)

with open('xbe-inspect/src/kernel/mm.cpp', 'w') as f:
    f.write(content)
