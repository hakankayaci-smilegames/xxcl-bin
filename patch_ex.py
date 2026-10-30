import re

with open('xbe-inspect/src/kernel/ex.cpp', 'r') as f:
    content = f.read()

tracking_code = """
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
"""

content = content.replace("#include <cstdlib>", tracking_code + "\n#include <cstdlib>")

content = content.replace(
    'void* ptr = malloc(NumberOfBytes);',
    'void* ptr = malloc(NumberOfBytes);\n        track_pool(ptr, NumberOfBytes);'
)

query_pattern = r"(extern \"C\" \{.*?xbox_ExQueryPoolBlockSize.*?xbe::record_kernel_call\(23\);.*?return size;.*?})"
new_query = """extern "C" {
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
}"""
content = re.sub(query_pattern, new_query, content, flags=re.DOTALL)

with open('xbe-inspect/src/kernel/ex.cpp', 'w') as f:
    f.write(content)
