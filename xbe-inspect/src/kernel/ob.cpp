#include "ob.hpp"
#include "../kernel.hpp"
#include <cstdio>
#include <cstring>

namespace xbe {
namespace kernel {

extern "C" NTSTATUS __attribute__((__stdcall__)) xbox_NtClose(uint32_t Handle) {
    xbe::record_kernel_call(187);
    printf("[KERNEL] NtClose(Handle: 0x%08X) cagrildi.\n", Handle);
    return 0; // STATUS_SUCCESS
}

extern "C" NTSTATUS __attribute__((__stdcall__)) xbox_NtOpenFile(
    void** FileHandle,
    uint32_t DesiredAccess,
    XBOX_OBJECT_ATTRIBUTES* ObjectAttributes,
    void* IoStatusBlock,
    uint32_t ShareAccess,
    uint32_t OpenOptions
) {
    xbe::record_kernel_call(202);
    
    if (ObjectAttributes && ObjectAttributes->ObjectName && ObjectAttributes->ObjectName->Buffer) {
        // Obfuscate the printing a bit to avoid crashing if string is bad
        char safe_name[256];
        uint16_t len = ObjectAttributes->ObjectName->Length;
        if (len > 255) len = 255;
        std::memcpy(safe_name, ObjectAttributes->ObjectName->Buffer, len);
        safe_name[len] = '\0';
        printf("[KERNEL] NtOpenFile('%s', DesiredAccess: 0x%08X) cagrildi.\n", safe_name, DesiredAccess);
    } else {
        printf("[KERNEL] NtOpenFile(NULL, DesiredAccess: 0x%08X) cagrildi.\n", DesiredAccess);
    }

    if (FileHandle) {
        *FileHandle = (void*)0x8899AABB;
    }
    
    // Setup IO_STATUS_BLOCK
    if (IoStatusBlock) {
        uint32_t* iosb = (uint32_t*)IoStatusBlock;
        iosb[0] = 0; // Status
        iosb[1] = 0; // Information
    }

    return 0; // STATUS_SUCCESS
}

} // namespace kernel
} // namespace xbe

namespace xbe {
namespace kernel {

extern "C" NTSTATUS __attribute__((__stdcall__)) xbox_NtDeviceIoControlFile(
    void* FileHandle,
    void* Event,
    void* ApcRoutine,
    void* ApcContext,
    void* IoStatusBlock,
    uint32_t IoControlCode,
    void* InputBuffer,
    uint32_t InputBufferLength,
    void* OutputBuffer,
    uint32_t OutputBufferLength
) {
    xbe::record_kernel_call(196);
    printf("[KERNEL] NtDeviceIoControlFile(Handle: %p, IOCTL: 0x%08X) cagrildi.\n", FileHandle, IoControlCode);
    
    // Setup IO_STATUS_BLOCK
    if (IoStatusBlock) {
        uint32_t* iosb = (uint32_t*)IoStatusBlock;
        iosb[0] = 0; // Status = STATUS_SUCCESS
        iosb[1] = 0; // Information
    }

    return 0; // STATUS_SUCCESS
}

} // namespace kernel
} // namespace xbe
