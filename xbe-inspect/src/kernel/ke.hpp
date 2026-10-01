#pragma once
#include <cstdint>

extern "C" {
    int32_t __attribute__((__stdcall__)) xbox_KeSetEvent(void* Event, int32_t Increment, uint8_t Wait);
}
