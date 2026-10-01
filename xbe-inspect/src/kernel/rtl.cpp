#include "rtl.hpp"
#include "../kernel.hpp"
#include <cstring>
#include <cstdio>

namespace xbe {
namespace kernel {

extern "C" void __attribute__((__stdcall__)) xbox_RtlInitAnsiString(PANSI_STRING DestinationString, const char* SourceString) {
    xbe::record_kernel_call(289);
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
        DestinationString->Buffer = nullptr;
        DestinationString->Length = 0;
        DestinationString->MaximumLength = 0;
    }
}

extern "C" NTSTATUS __attribute__((__stdcall__)) xbox_RtlMultiByteToUnicodeSize(uint32_t* BytesInUnicodeString, const char* MultiByteString, uint32_t BytesInMultiByteString) {
    xbe::record_kernel_call(300);
    printf("[KERNEL] RtlMultiByteToUnicodeSize cagrildi.\n");
    if (BytesInUnicodeString) {
        *BytesInUnicodeString = BytesInMultiByteString * 2;
    }
    return 0;
}

extern "C" uint32_t __attribute__((__stdcall__)) xbox_RtlNtStatusToDosError(NTSTATUS Status) {
    xbe::record_kernel_call(301);
    printf("[KERNEL] RtlNtStatusToDosError(Status: 0x%X) cagrildi.\n", Status);
    return Status;
}

extern "C" void __attribute__((__stdcall__)) xbox_RtlRaiseException(void* ExceptionRecord) {
    xbe::record_kernel_call(302);
    printf("[KERNEL] RtlRaiseException(Record: %p) cagrildi. CRITICAL ERROR!\n", ExceptionRecord);
}

} // namespace kernel
} // namespace xbe
