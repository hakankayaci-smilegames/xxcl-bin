#pragma once
#include <cstdint>
#include "types.hpp"

typedef int32_t NTSTATUS;

extern "C" {
    void __attribute__((__stdcall__)) xbox_RtlInitAnsiString(xbe::kernel::PANSI_STRING DestinationString, const char* SourceString);
    NTSTATUS __attribute__((__stdcall__)) xbox_RtlMultiByteToUnicodeSize(uint32_t* BytesInUnicodeString, const char* MultiByteString, uint32_t BytesInMultiByteString);
    uint32_t __attribute__((__stdcall__)) xbox_RtlNtStatusToDosError(NTSTATUS Status);
    void __attribute__((__stdcall__)) xbox_RtlRaiseException(void* ExceptionRecord);
}
