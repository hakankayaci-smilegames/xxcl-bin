#pragma once
#include "types.hpp"
#include <cstddef>
#include <cstdint>

namespace xbe {
namespace kernel {

struct EXCEPTION_RECORD {
    uint32_t ExceptionCode;
    uint32_t ExceptionFlags;
    struct EXCEPTION_RECORD* ExceptionRecord;
    void* ExceptionAddress;
    uint32_t NumberParameters;
    uint32_t ExceptionInformation[15];
};

extern "C" void __attribute__((__stdcall__)) xbox_RtlInitAnsiString(PANSI_STRING DestinationString, const char* SourceString);
extern "C" void __attribute__((__stdcall__)) xbox_RtlZeroMemory(void* Destination, size_t Length);
extern "C" void __attribute__((__stdcall__)) xbox_RtlFillMemory(void* Destination, size_t Length, uint8_t Fill);
extern "C" void __attribute__((__stdcall__)) xbox_RtlMoveMemory(void* Destination, const void* Source, size_t Length);

} // namespace kernel
} // namespace xbe
