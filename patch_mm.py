import re

with open('xbe-inspect/src/kernel/mm.cpp', 'r') as f:
    content = f.read()

# Fix MmAllocateContiguousMemoryEx
pattern1 = r"(int res = posix_memalign\(&ptr, align, NumberOfBytes\);)"
replacement1 = r"""
    size_t alloc_size = (NumberOfBytes + 4095) & ~4095;
    int res = posix_memalign(&ptr, align, alloc_size);
"""
content = re.sub(pattern1, replacement1, content)

# Fix MmAllocateContiguousMemory
pattern2 = r"(if \(posix_memalign\(&ptr, 4096, NumberOfBytes\) != 0\) \{)"
replacement2 = r"""
    size_t alloc_size = (NumberOfBytes + 4095) & ~4095;
    if (posix_memalign(&ptr, 4096, alloc_size) != 0) {
"""
content = re.sub(pattern2, replacement2, content)

# Fix MmClaimGpuInstanceMemory
pattern3 = r"(int res = posix_memalign\(&ptr, 4096, NumberOfBytes\);)"
replacement3 = r"""
    size_t alloc_size = (NumberOfBytes + 4095) & ~4095;
    int res = posix_memalign(&ptr, 4096, alloc_size);
"""
content = re.sub(pattern3, replacement3, content)

# Fix the memset using NumberOfBytes in all cases
content = content.replace("memset(ptr, 0, NumberOfBytes);", "memset(ptr, 0, alloc_size);")

with open('xbe-inspect/src/kernel/mm.cpp', 'w') as f:
    f.write(content)
