#include "ke.hpp"
#include <cstdio>
#include <cstring>

extern "C" {
    void __attribute__((__stdcall__)) xbox_KeInitializeDpc(void* Dpc, void* DeferredRoutine, void* DeferredContext) {
        printf("[KERNEL] KeInitializeDpc(Dpc: %p, Routine: %p, Context: %p) cagrildi.\n", Dpc, DeferredRoutine, DeferredContext);
        if (Dpc) {
            memset(Dpc, 0, 0x20); // Dummy size for KDPC
        }
    }

    void __attribute__((__stdcall__)) xbox_KeInitializeTimerEx(void* Timer, uint32_t Type) {
        printf("[KERNEL] KeInitializeTimerEx(Timer: %p, Type: %u) cagrildi.\n", Timer, Type);
        if (Timer) {
            memset(Timer, 0, 0x40); // Dummy size for KTIMER
        }
    }

    uint32_t __attribute__((__stdcall__)) xbox_KeSetTimer(void* Timer, uint32_t DueTimeLow, uint32_t DueTimeHigh, void* Dpc) {
        printf("[KERNEL] KeSetTimer(Timer: %p, DueTimeLow: %u, DueTimeHigh: %u, Dpc: %p) cagrildi.\n", Timer, DueTimeLow, DueTimeHigh, Dpc);
        // Returns BOOLEAN (TRUE if the timer was already in the timer queue)
        return 0; // FALSE
    }

    uint8_t launch_data_page_buffer[4096];
    void* xbox_LaunchDataPage = launch_data_page_buffer;
}

extern "C" int32_t __attribute__((__stdcall__)) xbox_KeSetEvent(void* Event, int32_t Increment, uint8_t Wait) {
    printf("[KERNEL] KeSetEvent(Event: %p, Increment: %d, Wait: %u) cagrildi.\n", Event, Increment, Wait);
    return 0;
}

extern "C" void __attribute__((__stdcall__)) xbox_KeInitializeInterrupt(
    void* Interrupt, void* ServiceRoutine, void* ServiceContext, void* SpinLock, 
    uint32_t Vector, uint8_t Irql, uint8_t SynchronizeIrql, uint32_t InterruptMode, uint8_t ShareVector
) {
    printf("[KERNEL] KeInitializeInterrupt(Interrupt: %p, Routine: %p, Vector: 0x%X) cagrildi.\n", Interrupt, ServiceRoutine, Vector);
    if (Interrupt) {
        memset(Interrupt, 0, 0x40); // Dummy KINTERRUPT
    }
}

extern "C" uint8_t __attribute__((__stdcall__)) xbox_KeConnectInterrupt(void* Interrupt) {
    printf("[KERNEL] KeConnectInterrupt(Interrupt: %p) cagrildi.\n", Interrupt);
    return 1; // TRUE (Basarili)
}
