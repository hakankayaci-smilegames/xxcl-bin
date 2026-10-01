#pragma once
#include <cstdint>

extern "C" {
    void __attribute__((__stdcall__)) xbox_KeInitializeDpc(void* Dpc, void* DeferredRoutine, void* DeferredContext);
    void __attribute__((__stdcall__)) xbox_KeInitializeTimerEx(void* Timer, uint32_t Type);
    uint32_t __attribute__((__stdcall__)) xbox_KeSetTimer(void* Timer, uint32_t DueTimeLow, uint32_t DueTimeHigh, void* Dpc);
}
