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
    
    if (IoStatusBlock) {
        uint32_t* iosb = (uint32_t*)IoStatusBlock;
        iosb[0] = 0; // Status
        iosb[1] = 0; // Information
    }

    return 0; // STATUS_SUCCESS
}

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
    if (IoControlCode == 0x0004D014 && InputBuffer != nullptr) {
        uint8_t* in_buf = (uint8_t*)InputBuffer;
        uint8_t cdb_opcode = in_buf[28];
        uint8_t cdb_page = in_buf[30];
        uint32_t data_transfer_length = *((uint32_t*)(in_buf + 12));
        printf("[KERNEL] SCSI_PASS_THROUGH_DIRECT (Opcode: 0x%02X, Page: 0x%02X, TransferLen: %u) cagrildi.\n", cdb_opcode, cdb_page, data_transfer_length);
        in_buf[2] = 0; /* ScsiStatus = GOOD */

        void* data_buffer = *((void**)(in_buf + 20));
        if (data_buffer && data_transfer_length >= 28 && cdb_opcode == 0x5A) {
            uint8_t* out = (uint8_t*)data_buffer;
            memset(out, 0, data_transfer_length);
            out[0] = 0; out[1] = 26; /* Mode Data Length */
            out[2] = 0; /* Medium Type */
            out[3] = 0; /* Device-Specific Parameter */
            out[4] = 0; out[5] = 0; /* Reserved */
            out[6] = 0; out[7] = 0; /* Block Descriptor Length */
            out[8] = 0x3E; /* Page Code */
            out[9] = 0x12; /* Page Length (18 bytes) */
            out[10] = 0x01; /* Authenticated */
            out[11] = 0x01; /* Required by Sega GT check */
            out[12] = 0x0D; /* Booktype Xbox */
            out[13] = 0x00;
            out[14] = 0x01; /* Partition Select? */
            printf("[KERNEL] SCSI_PASS_THROUGH_DIRECT (MODE SENSE 10) mocked for Page 3E!\n");
        }

        if (IoStatusBlock) {
            uint32_t* iosb = (uint32_t*)IoStatusBlock;
            iosb[0] = 0; /* Status */
            iosb[1] = data_transfer_length; /* Information */
        }
        return 0;
    }
    printf("[KERNEL] NtDeviceIoControlFile(Handle: %p, IOCTL: 0x%08X) cagrildi.\n", FileHandle, IoControlCode);
    
    if (IoStatusBlock) {
        uint32_t* iosb = (uint32_t*)IoStatusBlock;
        iosb[0] = 0; // Status = STATUS_SUCCESS
        iosb[1] = 0; // Information
    }

    return 0; // STATUS_SUCCESS
}

} // namespace kernel
} // namespace xbe
