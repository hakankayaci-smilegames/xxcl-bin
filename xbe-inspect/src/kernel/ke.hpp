#pragma once
#include <cstdint>

extern "C" {
    void __attribute__((__stdcall__)) xbox_KeInitializeDpc(void* Dpc, void* DeferredRoutine, void* DeferredContext);
    void __attribute__((__stdcall__)) xbox_KeInitializeTimerEx(void* Timer, uint32_t Type);
    uint32_t __attribute__((__stdcall__)) xbox_KeSetTimer(void* Timer, uint32_t DueTimeLow, uint32_t DueTimeHigh, void* Dpc);
    extern void* xbox_LaunchDataPage;
    int32_t __attribute__((__stdcall__)) xbox_KeSetEvent(void* Event, int32_t Increment, uint8_t Wait);
    void __attribute__((__stdcall__)) xbox_KeInitializeInterrupt(void* Interrupt, void* ServiceRoutine, void* ServiceContext, void* SpinLock, uint32_t Vector, uint8_t Irql, uint8_t SynchronizeIrql, uint32_t InterruptMode, uint8_t ShareVector);
    uint8_t __attribute__((__stdcall__)) xbox_KeConnectInterrupt(void* Interrupt);
}
