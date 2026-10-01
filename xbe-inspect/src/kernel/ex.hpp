#pragma once
#include <cstdint>
#include <cstddef>

typedef int32_t NTSTATUS;

extern "C" {
    NTSTATUS __attribute__((__stdcall__)) xbox_ExQueryNonVolatileSetting(
        uint32_t ValueIndex,
        uint32_t* Type,
        uint8_t* Value,
        uint32_t ValueLength,
        uint32_t* ResultLength
    );
}
