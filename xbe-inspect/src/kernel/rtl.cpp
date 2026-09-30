#include "rtl.hpp"
#include "../kernel.hpp"
#include <cstring>
#include <iostream>
#include <cstdio>

namespace xbe {
namespace kernel {

static char empty_string[] = "";

extern "C" void __attribute__((__stdcall__)) xbox_RtlInitAnsiString(PANSI_STRING DestinationString, const char* SourceString) {
    xbe::record_kernel_call(255);
    printf("[KERNEL] RtlInitAnsiString cagrildi -> Dest: %p, Src: %p\n", DestinationString, SourceString);
    if (!DestinationString) {
        return;
    }

    if (SourceString) {
        uint16_t len = (uint16_t)strlen(SourceString);
        DestinationString->Buffer = (char*)SourceString;
        DestinationString->Length = len;
        DestinationString->MaximumLength = len + 1;
        printf("[KERNEL] RtlInitAnsiString Metin: '%s' (Uzunluk: %u)\n", SourceString, len);
    } else {
        DestinationString->Buffer = empty_string;
        DestinationString->Length = 0;
        DestinationString->MaximumLength = 1;
    }
}

extern "C" uint32_t __attribute__((__stdcall__)) xbox_RtlNtStatusToDosError(uint32_t Status) {
    xbe::record_kernel_call(300);
    printf("[KERNEL] RtlNtStatusToDosError(Status: 0x%X)\n", Status);
    // Simple passthrough or mapping. Actually NTSTATUS mapped to Win32 error.
    return Status;
}

extern "C" void __attribute__((__stdcall__)) xbox_RtlZeroMemory(void* Destination, size_t Length) {
    xbe::record_kernel_call(300);
    printf("[KERNEL] RtlZeroMemory(Dest: %p, Length: 0x%X)\n", Destination, (unsigned int)Length);
    if (Destination && Length > 0) {
        std::memset(Destination, 0, Length);
    }
}

extern "C" void __attribute__((__stdcall__)) xbox_RtlFillMemory(void* Destination, size_t Length, uint8_t Fill) {
    xbe::record_kernel_call(301);
    printf("[KERNEL] RtlFillMemory(Dest: %p, Length: 0x%X, Fill: 0x%02X)\n", Destination, (unsigned int)Length, Fill);
    if (Destination && Length > 0) {
        std::memset(Destination, Fill, Length);
    }
}

extern "C" void __attribute__((__stdcall__)) xbox_RtlMoveMemory(void* Destination, const void* Source, size_t Length) {
    xbe::record_kernel_call(302);
    printf("[KERNEL] RtlMoveMemory(Dest: %p, Src: %p, Length: 0x%X)\n", Destination, Source, (unsigned int)Length);
    if (Destination && Source && Length > 0) {
        std::memmove(Destination, Source, Length);
    }
}

} // namespace kernel
} // namespace xbe
