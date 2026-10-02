#include <cstddef>
#pragma once
#include <cstdint>
#include "types.hpp"

typedef int32_t NTSTATUS;

namespace xbe {
namespace kernel {

extern "C" {
    void __attribute__((__stdcall__)) xbox_RtlInitAnsiString(PANSI_STRING DestinationString, const char* SourceString);
    NTSTATUS __attribute__((__stdcall__)) xbox_RtlMultiByteToUnicodeSize(uint32_t* BytesInUnicodeString, const char* MultiByteString, uint32_t BytesInMultiByteString);
    uint32_t __attribute__((__stdcall__)) xbox_RtlNtStatusToDosError(NTSTATUS Status);
    void __attribute__((__stdcall__)) xbox_RtlRaiseException(void* ExceptionRecord);
    NTSTATUS __attribute__((__stdcall__)) xbox_RtlInitializeCriticalSection(void* CriticalSection);
    uint8_t __attribute__((__stdcall__)) xbox_RtlEqualString(
        const xbe::kernel::STRING* String1,
        const xbe::kernel::STRING* String2,
        uint8_t CaseInSensitive
    );

    size_t __attribute__((__stdcall__)) xbox_RtlCompareMemory(
        const void* Source1,
        const void* Source2,
        size_t Length
    );
}
} // namespace kernel
} // namespace xbe
