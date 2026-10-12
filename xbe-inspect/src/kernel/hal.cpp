#include <sys/mman.h>
#include <cerrno>
#include "hal.hpp"
#include "apu.hpp"
#include "mmio.hpp"
#include "../kernel.hpp"
#include <cstdio>
#include <cstring>
#include <cstdlib>

namespace xbe {
namespace kernel {


extern "C" uint32_t __attribute__((__stdcall__)) xbox_HalGetInterruptVector(
    uint32_t BusInterruptLevel,
    uint8_t* Irql
) {
    xbe::record_kernel_call(44);
    uint32_t vector = 0x30 + BusInterruptLevel;

    if (Irql) {
        *Irql = (uint8_t)(0x18);
    }

    printf("[KERNEL] HalGetInterruptVector -> IRQ: %u => Vector: 0x%02X, Irql: %u\n",
           BusInterruptLevel, vector, Irql ? *Irql : 0);

    return vector;
}


extern "C" void __attribute__((__stdcall__)) xbox_HalRegisterShutdownNotification(
    void* ShutdownRegistration,
    uint8_t Register
) {
    xbe::record_kernel_call(47);
    printf("[KERNEL] HalRegisterShutdownNotification -> RegPtr: %p, State: %u\n",
           ShutdownRegistration, Register);
}



// Bus 1 (NV2A GPU) için 256 baytlık PCI Configuration Space
static uint8_t g_nv2a_pci_config[256] = {0};
static bool g_pci_initialized = false;

static void init_nv2a_pci_space() {
    if (g_pci_initialized) return;
    memset(g_nv2a_pci_config, 0, sizeof(g_nv2a_pci_config));

    // 0x00: Vendor ID (0x10DE - NVIDIA) & Device ID (0x02A0 - NV2A)
    *(uint16_t*)&g_nv2a_pci_config[0x00] = 0x10DE;
    *(uint16_t*)&g_nv2a_pci_config[0x02] = 0x02A0;

    // 0x04: Command Register (Bus Master | Memory Space | I/O Space)
    *(uint16_t*)&g_nv2a_pci_config[0x04] = 0x0007;

    // 0x08: Revision ID (0xB2) & Class Code (0x030000 - VGA Compatible Controller)
    *(uint32_t*)&g_nv2a_pci_config[0x08] = 0x030000B2;

    // 0x10: BAR0 (MMIO Base: 0xFD000000)
    *(uint32_t*)&g_nv2a_pci_config[0x10] = 0xFD000000;

    // 0x40 - 0x44: AGP Capabilities & Status
    *(uint32_t*)&g_nv2a_pci_config[0x40] = 0x00200002;
    *(uint32_t*)&g_nv2a_pci_config[0x44] = 0x1F000217;

    // 0x4C: NV_PBUS_PCI_NV_4C
    *(uint32_t*)&g_nv2a_pci_config[0x4C] = 0x0000007F;

    // 0x50 - 0x54: AGP Command & Control
    *(uint32_t*)&g_nv2a_pci_config[0x50] = 0x00000100;

    g_pci_initialized = true;
    printf("[PCI] NV2A PCI Configuration Space basariyla ilklendirildi.\n");
}

extern "C" uint32_t __attribute__((__stdcall__)) xbox_HalReadWritePCISpace(
    uint32_t BusNumber,
    uint32_t SlotNumber,
    uint32_t RegisterNumber,
    void* Buffer,
    uint32_t Length,
    uint8_t WritePCISpace
) {
    xbe::record_kernel_call(46);
    if (!Buffer || Length == 0) return 0;
    init_nv2a_pci_space();

    if (RegisterNumber + Length > 256) {
        printf("[PCI] Uyari: Register sinir asimi (Reg: 0x%X, Len: %u)\n", RegisterNumber, Length);
        return 0;
    }

    if (WritePCISpace) {
        memcpy(&g_nv2a_pci_config[RegisterNumber], Buffer, Length);
        uint32_t val = 0;
        memcpy(&val, Buffer, (Length <= 4) ? Length : 4);
        printf("[PCI] WRITE -> Bus: %u, Slot: 0x%04X, Reg: 0x%02X, Len: %u, Val: 0x%08X\n",
               BusNumber, SlotNumber, RegisterNumber, Length, val);
    } else {
        memcpy(Buffer, &g_nv2a_pci_config[RegisterNumber], Length);
        uint32_t val = 0;
        memcpy(&val, Buffer, (Length <= 4) ? Length : 4);
        printf("[PCI] READ  -> Bus: %u, Slot: 0x%04X, Reg: 0x%02X, Len: %u => Val: 0x%08X\n",
               BusNumber, SlotNumber, RegisterNumber, Length, val);
    }

    return Length;
}


extern "C" void __attribute__((__stdcall__)) xbox_HalReturnToFirmware(uint32_t Routine) {
    xbe::record_kernel_call(49);
    printf("[KERNEL] HalReturnToFirmware(Routine: %u) cagrildi. Oyun kapaniyor/reboot ediliyor!\n", Routine);
    uint32_t ret_addr;
    asm volatile("mov 4(%%ebp), %0" : "=r"(ret_addr));
    printf("[DEBUG] HalReturnToFirmware called from 0x%08X\n", ret_addr);
    uint32_t* ebp_ptr;
    asm volatile("mov %%ebp, %0" : "=r"(ebp_ptr));
    printf("--- FATAL ERROR STACK TRACE ---\n");
    for(int i = 0; i < 5; i++) {
        if (!ebp_ptr) break;
        printf("Frame %d: EBP=%p, RET=0x%08X\n", i, ebp_ptr, ebp_ptr[1]);
        ebp_ptr = (uint32_t*)ebp_ptr[0];
    }
    printf("-------------------------------\n");
    xbe::kernel::apu_stop_worker();
    exit(0);
}

    extern "C" void hal_init_usb_mmio() {
        void* mmio = mmap((void*)0xFED00000, 0x00010000, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
        if (mmio == MAP_FAILED) {
            printf("[HAL] USB MMIO 0xFED00000 mmap BASARISIZ! (Hata: %s)\n", strerror(errno));
        } else {
            printf("[HAL] USB MMIO sahte donanim alani (0xFED00000 - 64KB) basariyla eslendi.\n");

        void* apic_mmio = mmap((void*)0xFEC00000, 0x00010000, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
        if (apic_mmio == MAP_FAILED) {
            printf("[HAL] APIC MMIO 0xFEC00000 mmap BASARISIZ! (Hata: %s)\n", strerror(errno));
        } else {
            printf("[HAL] APIC MMIO sahte donanim alani (0xFEC00000 - 64KB) basariyla eslendi.\n");
            if (!mmio_initialize_interrupt_controller()) {
                printf("[HAL] APIC MMIO yazma izlemesi basarisiz.\n");
            }
        }


        void* apu_mmio = mmap((void*)0xFE800000, 0x00080000, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
        if (apu_mmio == MAP_FAILED) {
            printf("[HAL] APU MMIO 0xFE800000 mmap BASARISIZ! (Hata: %s)\n", strerror(errno));
        } else {
            printf("[HAL] APU MMIO sahte donanim alani (0xFE800000 - 512KB) basariyla eslendi.\n");
            if (!apu_initialize()) {
                printf("[HAL] APU register defaults could not be initialized.\n");
            }
        }

        }
    }

} // namespace kernel
} // namespace xbe

extern "C" uint32_t __attribute__((__stdcall__)) xbox_AvSendTVEncoderOption(
    void* OptionData,
    uint32_t OptionSize,
    uint32_t OptionType,
    void* ResultData
) {
    xbe::record_kernel_call(2);
    printf("[KERNEL] AvSendTVEncoderOption cagrildi -> Type: %u, Size: %u\n", OptionType, OptionSize);
    return 0; // STATUS_SUCCESS
}

#include <sys/mman.h>
#include <cerrno>

