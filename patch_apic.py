import re

with open('xbe-inspect/src/kernel/hal.cpp', 'r') as f:
    content = f.read()

apic_code = """
        void* apic_mmio = mmap((void*)0xFEC00000, 0x00010000, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
        if (apic_mmio == MAP_FAILED) {
            printf("[HAL] APIC MMIO 0xFEC00000 mmap BASARISIZ! (Hata: %s)\\n", strerror(errno));
        } else {
            printf("[HAL] APIC MMIO sahte donanim alani (0xFEC00000 - 64KB) basariyla eslendi.\\n");
        }
"""

content = content.replace(
    'printf("[HAL] USB MMIO sahte donanim alani (0xFED00000 - 64KB) basariyla eslendi.\\n");', 
    'printf("[HAL] USB MMIO sahte donanim alani (0xFED00000 - 64KB) basariyla eslendi.\\n");\n' + apic_code
)

with open('xbe-inspect/src/kernel/hal.cpp', 'w') as f:
    f.write(content)
