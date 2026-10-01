#pragma once
#include <cstdint>

typedef int32_t NTSTATUS;

extern "C" {
    NTSTATUS __attribute__((__stdcall__)) xbox_NtClose(uint32_t Handle);
}
