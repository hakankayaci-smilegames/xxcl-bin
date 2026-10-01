#include "io.hpp"
#include "vfs.hpp"
#include "../kernel.hpp"
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

namespace xbe {
namespace kernel {

extern "C" NTSTATUS __attribute__((__stdcall__)) xbox_NtClose(uint32_t Handle) {
    xbe::record_kernel_call(187);
    printf("[KERNEL] NtClose(Handle: 0x%08X) cagrildi.\n", Handle);
    if (Handle != 0x8899AABB && Handle < 0x10000) {
        close((int)Handle);
    }
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
    
    std::string xbox_path = "";
    if (ObjectAttributes && ObjectAttributes->ObjectName && ObjectAttributes->ObjectName->Buffer) {
        uint16_t len = ObjectAttributes->ObjectName->Length;
        if (len > 255) len = 255;
        char temp[256];
        std::memcpy(temp, ObjectAttributes->ObjectName->Buffer, len);
        temp[len] = '\0';
        xbox_path = temp;
    }

    std::string linux_path = vfs_translate_path(xbox_path);
    printf("[KERNEL] NtOpenFile('%s' -> '%s', DesiredAccess: 0x%08X) cagrildi.\n", xbox_path.c_str(), linux_path.c_str(), DesiredAccess);

    if (FileHandle) {
        int fd = open(linux_path.c_str(), O_RDWR);
        if (fd < 0) {
            fd = open(linux_path.c_str(), O_RDONLY);
        }
        
        if (fd >= 0) {
            *FileHandle = (void*)(intptr_t)fd;
        } else {
            *FileHandle = (void*)0x8899AABB;
        }
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

extern "C" NTSTATUS __attribute__((__stdcall__)) xbox_NtQueryVolumeInformationFile(
    void* FileHandle,
    void* IoStatusBlock,
    void* FsInformation,
    uint32_t Length,
    uint32_t FsInformationClass
) {
    xbe::record_kernel_call(218);
    printf("[KERNEL] NtQueryVolumeInformationFile(Handle: %p, Class: %u, Length: %u) cagrildi.\n", FileHandle, FsInformationClass, Length);

    uint32_t info_size = 0;

    if (FsInformation && Length > 0) {
        memset(FsInformation, 0, Length);

        if (FsInformationClass == 4) { // FileFsDeviceInformation
            struct FILE_FS_DEVICE_INFORMATION {
                uint32_t DeviceType;
                uint32_t Characteristics;
            };
            if (Length >= sizeof(FILE_FS_DEVICE_INFORMATION)) {
                auto* info = (FILE_FS_DEVICE_INFORMATION*)FsInformation;
                info->DeviceType = 7; // FILE_DEVICE_DISK
                info->Characteristics = 0x20; // FILE_DEVICE_IS_MOUNTED
                info_size = sizeof(FILE_FS_DEVICE_INFORMATION);
            }
        } else if (FsInformationClass == 3) { // FileFsSizeInformation
            struct FILE_FS_SIZE_INFORMATION {
                uint64_t TotalAllocationUnits;
                uint64_t AvailableAllocationUnits;
                uint32_t SectorsPerAllocationUnit;
                uint32_t BytesPerSector;
            };
            if (Length >= sizeof(FILE_FS_SIZE_INFORMATION)) {
                auto* info = (FILE_FS_SIZE_INFORMATION*)FsInformation;
                info->TotalAllocationUnits = 1000000;
                info->AvailableAllocationUnits = 800000;
                info->SectorsPerAllocationUnit = 32; // Sega GT expects 16KB clusters (32 * 512 = 16384)
                info->BytesPerSector = 512;
                info_size = sizeof(FILE_FS_SIZE_INFORMATION);
            }
        } else {
            info_size = Length;
        }
    }

    if (IoStatusBlock) {
        uint32_t* iosb = (uint32_t*)IoStatusBlock;
        iosb[0] = 0; // STATUS_SUCCESS
        iosb[1] = info_size;
    }

    return 0; // STATUS_SUCCESS
}

} // namespace kernel
} // namespace xbe
