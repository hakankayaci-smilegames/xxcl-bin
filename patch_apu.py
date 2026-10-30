import re

with open('xbe-inspect/src/kernel/nv2a.cpp', 'r') as f:
    content = f.read()

apu_code = """
        // 4. APU FIFO Space (0xFE820010)
        volatile uint32_t* apu_fifo_space = (volatile uint32_t*)0xFE820010;
        // Check if mapped (we map 0xFE800000 to 0xFE87FFFF, so it's safe to write)
        *apu_fifo_space = 0x200; // Always enough space in Audio FIFO
"""

content = content.replace(
    '// CPU\'yu %100\'e bogmamak icin 500 mikrosaniye uyu',
    apu_code + '\n        // CPU\'yu %100\'e bogmamak icin 500 mikrosaniye uyu'
)

with open('xbe-inspect/src/kernel/nv2a.cpp', 'w') as f:
    f.write(content)
