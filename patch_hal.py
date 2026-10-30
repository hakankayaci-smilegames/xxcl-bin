import re

with open('xbe-inspect/src/kernel/hal.cpp', 'r') as f:
    content = f.read()

apu_map_code = """
        void* apu_mmio = mmap((void*)0xFE800000, 0x00080000, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
        if (apu_mmio == MAP_FAILED) {
            printf("[HAL] APU MMIO 0xFE800000 mmap BASARISIZ! (Hata: %s)\\n", strerror(errno));
        } else {
            printf("[HAL] APU MMIO sahte donanim alani (0xFE800000 - 512KB) basariyla eslendi.\\n");
        }
"""

# Find where FED00000 is mapped
content = content.replace('printf("[HAL] USB MMIO sahte donanim alani (0xFED00000 - 64KB) basariyla eslendi.\\n");', 
                          'printf("[HAL] USB MMIO sahte donanim alani (0xFED00000 - 64KB) basariyla eslendi.\\n");\n' + apu_map_code)

with open('xbe-inspect/src/kernel/hal.cpp', 'w') as f:
    f.write(content)
