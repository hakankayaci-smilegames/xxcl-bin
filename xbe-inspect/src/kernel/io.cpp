#include "io.hpp"
#include "vfs.hpp"
#include "../kernel.hpp"
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <map>
#include <cerrno>

namespace xbe {
namespace kernel {

static std::map<void*, int> g_handle_to_fd;

extern "C" NTSTATUS __attribute__((__stdcall__)) xbox_NtClose(void* Handle) {
    xbe::record_kernel_call(187);
    printf("[KERNEL] NtClose(Handle: %p) cagrildi.\n", Handle);
    if (g_handle_to_fd.count(Handle)) {
        close(g_handle_to_fd[Handle]);
        g_handle_to_fd.erase(Handle);
    }
    return 0; // STATUS_SUCCESS
}

extern "C" NTSTATUS __attribute__((__stdcall__)) xbox_NtOpenFile(
    void** FileHandle,
    uint32_t DesiredAccess,
    OBJECT_ATTRIBUTES* ObjectAttributes,
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
            void* handle_val = (void*)(uintptr_t)(fd + 0x10000);
            *FileHandle = handle_val;
            g_handle_to_fd[handle_val] = fd;
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


extern "C" NTSTATUS __attribute__((__stdcall__)) xbox_NtCreateFile(
    void** FileHandle,
    uint32_t DesiredAccess,
    OBJECT_ATTRIBUTES* ObjectAttributes,
    IO_STATUS_BLOCK* IoStatusBlock,
    int64_t* AllocationSize,
    uint32_t FileAttributes,
    uint32_t ShareAccess,
    uint32_t CreateDisposition,
    uint32_t CreateOptions,
    void* EaBuffer,
    uint32_t EaLength
) {
    xbe::record_kernel_call(190);
    if (!FileHandle || !ObjectAttributes || !ObjectAttributes->ObjectName || !ObjectAttributes->ObjectName->Buffer) {
        if (IoStatusBlock) IoStatusBlock->Status = 0xC000000D; // STATUS_INVALID_PARAMETER
        return 0xC000000D;
    }

    std::string xbox_path(ObjectAttributes->ObjectName->Buffer, ObjectAttributes->ObjectName->Length);
    printf("[KERNEL] NtCreateFile -> Xbox Yolu: '%s', Disposition: 0x%X\n", xbox_path.c_str(), CreateDisposition);

    std::string linux_path = vfs_translate_path(xbox_path);
    printf("[KERNEL] NtCreateFile -> Linux Yolu: '%s'\n", linux_path.c_str());

    // POSIX open() bayraklarini hazirla
        // Dizin olusturma kontrolu
    if (CreateOptions & 1) { // FILE_DIRECTORY_FILE = 1
        if (CreateDisposition == 1 || CreateDisposition == 3) {
            mkdir(linux_path.c_str(), 0777);
            // Dizinleri acmak icin O_RDONLY kullanilir
            int fd = open(linux_path.c_str(), O_RDONLY | O_DIRECTORY);
            if (fd >= 0) {
                void* handle_val = (void*)(uintptr_t)(fd + 0x10000);
                *FileHandle = handle_val;
                g_handle_to_fd[handle_val] = fd;
                if (IoStatusBlock) { IoStatusBlock->Status = 0; IoStatusBlock->Information = 1; }
                printf("[KERNEL] NtCreateFile -> DIRECTORY BASARILI! Handle: %p (FD: %d)\n", *FileHandle, fd);
                return 0;
            }
        }
    }

    int flags = O_RDONLY;
    if ((DesiredAccess & 0x40000000) || (DesiredAccess & 0x80000000) || (DesiredAccess & 0x00100000) || (DesiredAccess & 0x02000000)) { 
        if (CreateDisposition == 1) flags = O_RDWR;
        else if (CreateDisposition == 2) flags = O_RDWR | O_CREAT | O_EXCL;
        else if (CreateDisposition == 3) flags = O_RDWR | O_CREAT;
        else if (CreateDisposition == 4) flags = O_RDWR | O_CREAT;
        else if (CreateDisposition == 5) flags = O_RDWR | O_CREAT | O_TRUNC;
    }

    int fd = open(linux_path.c_str(), flags, 0666);
    if (fd < 0 && flags != O_RDONLY) {
        // Fallback for directories or read-only files
        fd = open(linux_path.c_str(), O_RDONLY);
    }

    if (fd < 0) {
        printf("[KERNEL] NtCreateFile -> Dosya acilamadi! (Hata: %s)\n", strerror(errno));
        if (IoStatusBlock) {
            IoStatusBlock->Status = 0xC0000034; // STATUS_OBJECT_NAME_NOT_FOUND
            IoStatusBlock->Information = 0;
        }
        return 0xC0000034;
    }

    void* handle_val = (void*)(uintptr_t)(fd + 0x10000);
    *FileHandle = handle_val;
    g_handle_to_fd[handle_val] = fd;

    if (IoStatusBlock) {
        IoStatusBlock->Status = 0; // STATUS_SUCCESS
        IoStatusBlock->Information = 1; // FILE_OPENED
    }

    printf("[KERNEL] NtCreateFile -> BASARILI! Handle: %p (FD: %d)\n", *FileHandle, fd);
    return 0; // STATUS_SUCCESS
}

extern "C" NTSTATUS __attribute__((__stdcall__)) xbox_NtReadFile(
    void* FileHandle,
    void* Event,
    void* ApcRoutine,
    void* ApcContext,
    IO_STATUS_BLOCK* IoStatusBlock,
    void* Buffer,
    uint32_t Length,
    int64_t* ByteOffset,
    uint32_t* Key
) {
    xbe::record_kernel_call(219);
    printf("[KERNEL] NtReadFile -> Handle: %p, Length: %u\n", FileHandle, Length);

    if (!g_handle_to_fd.count(FileHandle)) {
        printf("[KERNEL] NtReadFile -> Gecersiz handle!\n");
        return 0xC0000008; // STATUS_INVALID_HANDLE
    }

    int fd = g_handle_to_fd[FileHandle];
    ssize_t bytes_read = -1;

    if (ByteOffset) {
        bytes_read = pread(fd, Buffer, Length, *ByteOffset);
    } else {
        bytes_read = read(fd, Buffer, Length);
    }

    if (bytes_read < 0) {
        printf("[KERNEL] NtReadFile -> Okuma basarisiz! (Hata: %s)\n", strerror(errno));
        if (IoStatusBlock) {
            IoStatusBlock->Status = 0xC0000034;
            IoStatusBlock->Information = 0;
        }
        return 0xC0000034;
    }

    if (IoStatusBlock) {
        IoStatusBlock->Status = 0;
        IoStatusBlock->Information = bytes_read;
    }
    return 0;
}


extern "C" NTSTATUS __attribute__((__stdcall__)) xbox_IoCreateSymbolicLink(
    xbe::kernel::STRING* SymbolicLinkName,
    xbe::kernel::STRING* DeviceName
) {
    xbe::record_kernel_call(67);
    if (!SymbolicLinkName || !DeviceName || !SymbolicLinkName->Buffer || !DeviceName->Buffer) {
        return 0xC000000D; // STATUS_INVALID_PARAMETER
    }
    
    std::string link(SymbolicLinkName->Buffer, SymbolicLinkName->Length);
    std::string target(DeviceName->Buffer, DeviceName->Length);
    
    printf("[KERNEL] IoCreateSymbolicLink -> Link: '%s' ===> Target: '%s'\n", link.c_str(), target.c_str());
    
    vfs_register_symlink(link, target);
    
    return 0; // STATUS_SUCCESS
}

extern "C" NTSTATUS __attribute__((__stdcall__)) xbox_IoDeleteSymbolicLink(
    xbe::kernel::STRING* SymbolicLinkName
) {
    xbe::record_kernel_call(68);
    if (!SymbolicLinkName || !SymbolicLinkName->Buffer) {
        return 0xC000000D;
    }
    
    std::string link(SymbolicLinkName->Buffer, SymbolicLinkName->Length);
    printf("[KERNEL] IoDeleteSymbolicLink -> Link: '%s'\n", link.c_str());
    
    vfs_delete_symlink(link);
    
    return 0; // STATUS_SUCCESS
}


extern "C" int32_t __attribute__((__stdcall__)) xbox_KeSetEvent(void* Event, int32_t Increment, uint8_t Wait);


extern "C" NTSTATUS __attribute__((__stdcall__)) xbox_NtQueryInformationFile(
    void* FileHandle,
    IO_STATUS_BLOCK* IoStatusBlock,
    void* FileInformation,
    uint32_t Length,
    uint32_t FileInformationClass
) {
    xbe::record_kernel_call(211);
    printf("[KERNEL] NtQueryInformationFile(Handle: %p, Class: %u, Length: %u) cagrildi.\n", FileHandle, FileInformationClass, Length);

    if (!g_handle_to_fd.count(FileHandle)) {
        if (IoStatusBlock) IoStatusBlock->Status = 0xC0000008;
        return 0xC0000008; // STATUS_INVALID_HANDLE
    }
    
    int fd = g_handle_to_fd[FileHandle];

    if (FileInformation && Length > 0) {
        memset(FileInformation, 0, Length);
        
        if (FileInformationClass == 0x22) { // FileStandardInformation (34) OR maybe FileNetworkOpenInformation? 0x22 is FileNetworkOpenInformation on Windows (34)
            // Wait, FileStandardInformation is 5.
            // On Windows NT, 34 is FileNetworkOpenInformation.
            // Structure: 
            // LARGE_INTEGER CreationTime;
            // LARGE_INTEGER LastAccessTime;
            // LARGE_INTEGER LastWriteTime;
            // LARGE_INTEGER ChangeTime;
            // LARGE_INTEGER AllocationSize;
            // LARGE_INTEGER EndOfFile;
            // ULONG FileAttributes;
            // ULONG Unknown; // padding
            // Size is 56 bytes (0x38).
            // Matches length exactly!
            
            uint64_t* times = (uint64_t*)FileInformation;
            times[0] = 0; // Creation
            times[1] = 0; // LastAccess
            times[2] = 0; // LastWrite
            times[3] = 0; // Change
            
            off_t current_pos = lseek(fd, 0, SEEK_CUR);
            off_t file_size = lseek(fd, 0, SEEK_END);
            lseek(fd, current_pos, SEEK_SET);
            
            times[4] = (file_size + 511) & ~511; // AllocationSize
            times[5] = file_size; // EndOfFile
            uint32_t* attrs = (uint32_t*)(&times[6]);
            attrs[0] = 0x80; // FILE_ATTRIBUTE_NORMAL
            
            if (IoStatusBlock) IoStatusBlock->Information = 56;
        } else if (FileInformationClass == 5) { // FileStandardInformation
            struct FILE_STANDARD_INFORMATION {
                uint64_t AllocationSize;
                uint64_t EndOfFile;
                uint32_t NumberOfLinks;
                uint8_t DeletePending;
                uint8_t Directory;
                uint8_t Reserved[2];
            };
            if (Length >= sizeof(FILE_STANDARD_INFORMATION)) {
                auto* info = (FILE_STANDARD_INFORMATION*)FileInformation;
                off_t current_pos = lseek(fd, 0, SEEK_CUR);
                off_t file_size = lseek(fd, 0, SEEK_END);
                lseek(fd, current_pos, SEEK_SET);
                
                info->AllocationSize = (file_size + 511) & ~511;
                info->EndOfFile = file_size;
                info->NumberOfLinks = 1;
                info->DeletePending = 0;
                info->Directory = 0;
                if (IoStatusBlock) IoStatusBlock->Information = sizeof(FILE_STANDARD_INFORMATION);
            }
        }
    }

    if (IoStatusBlock) IoStatusBlock->Status = 0;
    return 0; // STATUS_SUCCESS
}

extern "C" NTSTATUS __attribute__((__stdcall__)) xbox_NtWriteFile(
    void* FileHandle,
    void* Event,
    void* ApcRoutine,
    void* ApcContext,
    IO_STATUS_BLOCK* IoStatusBlock,
    void* Buffer,
    uint32_t Length,
    int64_t* ByteOffset,
    uint32_t* Key
) {
    xbe::record_kernel_call(236);
    if (!FileHandle || !Buffer) {
        if (IoStatusBlock) IoStatusBlock->Status = 0xC000000D; // STATUS_INVALID_PARAMETER
        return 0xC000000D;
    }

    auto it = g_handle_to_fd.find(FileHandle);
    if (it == g_handle_to_fd.end()) {
        printf("[KERNEL] NtWriteFile -> Gecersiz Handle: %p\n", FileHandle);
        if (IoStatusBlock) IoStatusBlock->Status = 0xC0000008; // STATUS_INVALID_HANDLE
        return 0xC0000008;
    }
    int fd = it->second;

    ssize_t bytes_written = 0;
    if (ByteOffset && *ByteOffset >= 0) {
        bytes_written = pwrite(fd, Buffer, Length, (off_t)*ByteOffset);
    } else {
        bytes_written = write(fd, Buffer, Length);
    }

    if (bytes_written < 0) {
        printf("[KERNEL] NtWriteFile -> Yazma hatasi: %s (FD: %d)\n", strerror(errno), fd);
        if (IoStatusBlock) {
            IoStatusBlock->Status = 0xC0000001; // STATUS_UNSUCCESSFUL
            IoStatusBlock->Information = 0;
        }
        return 0xC0000001;
    }

    if (IoStatusBlock) {
        IoStatusBlock->Status = 0; // STATUS_SUCCESS
        IoStatusBlock->Information = (uintptr_t)bytes_written;
    }

    if (Event) {
        xbox_KeSetEvent(Event, 0, 0);
    }

    printf("[KERNEL] NtWriteFile -> Handle: %p (FD: %d), Boyut: %u, Yazilan: %zd bayt\n",
           FileHandle, fd, Length, bytes_written);

    return 0; // STATUS_SUCCESS
}

} // namespace kernel
} // namespace xbe
