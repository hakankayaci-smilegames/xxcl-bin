#pragma once
#include <cstdint>

// NTSTATUS (basitce)
typedef int32_t NTSTATUS;

extern "C" {
    NTSTATUS __attribute__((__stdcall__)) xbox_PsCreateSystemThreadEx(
        uint32_t* ThreadHandle,
        uint32_t arg2,
        uint32_t arg3,
        uint32_t arg4,
        uint32_t arg5,
        uint32_t arg6,
        uint32_t arg7,
        uint32_t arg8,
        uint32_t arg9,
        uint32_t arg10
    );
}
