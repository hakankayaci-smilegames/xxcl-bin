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

namespace xbe {
namespace kernel {
extern "C" NTSTATUS __attribute__((__stdcall__)) xbox_RtlInitializeCriticalSection(void* CriticalSection) {
    xbe::record_kernel_call(291);
    printf("[KERNEL] RtlInitializeCriticalSection(CS: %p) cagrildi.\n", CriticalSection);
    if (CriticalSection) {
        // Initialize LockCount to -1 (Unlocked state in NT)
        // DebugInfo=0, LockCount=4, RecursionCount=8, OwningThread=12, LockSemaphore=16, SpinCount=20
        uint32_t* cs = (uint32_t*)CriticalSection;
        cs[0] = 0; // DebugInfo
        cs[1] = 0xFFFFFFFF; // LockCount = -1
        cs[2] = 0; // RecursionCount
        cs[3] = 0; // OwningThread
        cs[4] = 0; // LockSemaphore
        cs[5] = 0; // SpinCount
    }
    return 0; // STATUS_SUCCESS
}
}
}
