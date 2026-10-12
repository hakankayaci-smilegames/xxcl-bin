#include "io.hpp"
#include "vfs.hpp"
#include "device_io.hpp"
#include "handles.hpp"
#include "../kernel.hpp"
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <cerrno>
#include <cstdint>

namespace xbe {
namespace kernel {

static constexpr NTSTATUS STATUS_SUCCESS = 0;
static constexpr NTSTATUS STATUS_INVALID_PARAMETER = static_cast<NTSTATUS>(0xC000000D);
static constexpr NTSTATUS STATUS_INVALID_HANDLE = static_cast<NTSTATUS>(0xC0000008);
static constexpr NTSTATUS STATUS_OBJECT_NAME_NOT_FOUND = static_cast<NTSTATUS>(0xC0000034);
static constexpr NTSTATUS STATUS_OBJECT_NAME_COLLISION = static_cast<NTSTATUS>(0xC0000035);
static constexpr NTSTATUS STATUS_OBJECT_PATH_NOT_FOUND = static_cast<NTSTATUS>(0xC000003A);
static constexpr NTSTATUS STATUS_ACCESS_DENIED = static_cast<NTSTATUS>(0xC0000022);
static constexpr NTSTATUS STATUS_MEDIA_WRITE_PROTECTED = static_cast<NTSTATUS>(0xC00000A2);
static constexpr NTSTATUS STATUS_NOT_SUPPORTED = static_cast<NTSTATUS>(0xC00000BB);
static constexpr NTSTATUS STATUS_UNSUCCESSFUL = static_cast<NTSTATUS>(0xC0000001);
static constexpr NTSTATUS STATUS_INVALID_INFO_CLASS = static_cast<NTSTATUS>(0xC0000003);
static constexpr NTSTATUS STATUS_INFO_LENGTH_MISMATCH = static_cast<NTSTATUS>(0xC0000004);

static NTSTATUS status_from_errno(int error) {
    switch (error) {
        case ENOENT: return STATUS_OBJECT_NAME_NOT_FOUND;
        case ENOTDIR: return STATUS_OBJECT_PATH_NOT_FOUND;
        case EEXIST: return STATUS_OBJECT_NAME_COLLISION;
        case EACCES:
        case EPERM: return STATUS_ACCESS_DENIED;
        case EBADF: return STATUS_ACCESS_DENIED;
        case EROFS: return STATUS_MEDIA_WRITE_PROTECTED;
        case EISDIR: return static_cast<NTSTATUS>(0xC00000BA);
        default: return STATUS_UNSUCCESSFUL;
    }
}

static int get_fd(void* handle) {
    FileHandleRecord record{};
    return lookup_file_handle(handle, record) ? record.fd : -1;
}

static bool is_cdrom_handle(void* handle) {
    FileHandleRecord record{};
    return lookup_file_handle(handle, record) && record.cdrom;
}

static void set_io_status(IO_STATUS_BLOCK* iosb, NTSTATUS status, uintptr_t information = 0) {
    if (!iosb) return;
    iosb->Status = static_cast<uint32_t>(status);
    iosb->Information = information;
}

static NTSTATUS get_xbox_path(OBJECT_ATTRIBUTES* attributes, std::string& path) {
    if (!attributes || !attributes->ObjectName || !attributes->ObjectName->Buffer ||
        attributes->ObjectName->Length == 0) {
        return STATUS_INVALID_PARAMETER;
    }
    path.assign(attributes->ObjectName->Buffer, attributes->ObjectName->Length);
    if (path.find('\0') != std::string::npos) return STATUS_INVALID_PARAMETER;
    return STATUS_SUCCESS;
}

static bool requests_write_access(uint32_t desired_access) {
    constexpr uint32_t GENERIC_WRITE = 0x40000000;
    constexpr uint32_t FILE_WRITE_DATA = 0x00000002;
    constexpr uint32_t FILE_APPEND_DATA = 0x00000004;
    constexpr uint32_t FILE_WRITE_EA = 0x00000010;
    constexpr uint32_t FILE_WRITE_ATTRIBUTES = 0x00000100;
    constexpr uint32_t DELETE_ACCESS = 0x00010000;
    return (desired_access & (GENERIC_WRITE | FILE_WRITE_DATA | FILE_APPEND_DATA |
                              FILE_WRITE_EA | FILE_WRITE_ATTRIBUTES | DELETE_ACCESS)) != 0;
}

static int access_mode(uint32_t desired_access) {
    constexpr uint32_t GENERIC_READ = 0x80000000;
    constexpr uint32_t FILE_READ_DATA = 0x00000001;
    constexpr uint32_t FILE_READ_EA = 0x00000008;
    constexpr uint32_t FILE_READ_ATTRIBUTES = 0x00000080;
    constexpr uint32_t FILE_EXECUTE = 0x00000020;
    const bool write = requests_write_access(desired_access);
    const bool read = (desired_access & (GENERIC_READ | FILE_READ_DATA | FILE_READ_EA |
                                         FILE_READ_ATTRIBUTES | FILE_EXECUTE)) != 0;
    if (write && read) return O_RDWR;
    if (write) return O_WRONLY;
    return O_RDONLY;
}

extern "C" NTSTATUS __attribute__((__stdcall__)) xbox_NtClose(void* Handle) {
    xbe::record_kernel_call(187);
    printf("[KERNEL] NtClose(Handle: %p) cagrildi.\n", Handle);
    const int error = close_kernel_handle(Handle);
    return error == 0 ? STATUS_SUCCESS :
           error == EBADF ? STATUS_INVALID_HANDLE : status_from_errno(error);
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
    if (!FileHandle) return STATUS_INVALID_PARAMETER;
    *FileHandle = nullptr;
    std::string xbox_path;
    NTSTATUS status = get_xbox_path(ObjectAttributes, xbox_path);
    if (status != STATUS_SUCCESS) {
        if (IoStatusBlock) static_cast<IO_STATUS_BLOCK*>(IoStatusBlock)->Status = static_cast<uint32_t>(status);
        return status;
    }
    const std::string linux_path = vfs_translate_path(xbox_path);
    if (linux_path.empty()) status = STATUS_OBJECT_PATH_NOT_FOUND;
    else if (vfs_is_cdrom_path(xbox_path) && requests_write_access(DesiredAccess)) status = STATUS_MEDIA_WRITE_PROTECTED;
    int flags = access_mode(DesiredAccess);
    if ((OpenOptions & 0x00000001) != 0) flags |= O_DIRECTORY;
    const int fd = status == STATUS_SUCCESS ? open(linux_path.c_str(), flags | O_CLOEXEC) : -1;
    if (fd < 0 && status == STATUS_SUCCESS) status = status_from_errno(errno);
    if (fd >= 0) *FileHandle = register_file_handle(fd, vfs_is_cdrom_path(xbox_path));
    if (IoStatusBlock) {
        auto* iosb = static_cast<IO_STATUS_BLOCK*>(IoStatusBlock);
        iosb->Status = static_cast<uint32_t>(status);
        iosb->Information = status == STATUS_SUCCESS ? 1 : 0;
    }
    printf("[KERNEL] NtOpenFile('%s' -> '%s') status: 0x%08X\n", xbox_path.c_str(), linux_path.c_str(), static_cast<uint32_t>(status));
    return status;
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
    if (get_fd(FileHandle) < 0) {
        set_io_status(static_cast<IO_STATUS_BLOCK*>(IoStatusBlock), STATUS_INVALID_HANDLE);
        return STATUS_INVALID_HANDLE;
    }
    const NTSTATUS status = xbox_device_control_dispatch(
        is_cdrom_handle(FileHandle), IoControlCode, InputBuffer, InputBufferLength,
        OutputBuffer, OutputBufferLength, static_cast<IO_STATUS_BLOCK*>(IoStatusBlock));
    printf("[KERNEL] NtDeviceIoControlFile(Handle: %p, IOCTL: 0x%08X) status: 0x%08X\n",
           FileHandle, IoControlCode, static_cast<uint32_t>(status));
    return status;
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

    if (get_fd(FileHandle) < 0) {
        set_io_status(static_cast<IO_STATUS_BLOCK*>(IoStatusBlock), STATUS_INVALID_HANDLE);
        return STATUS_INVALID_HANDLE;
    }

    uint32_t info_size = 0;
    NTSTATUS status = STATUS_SUCCESS;

    if (!FsInformation) {
        status = STATUS_INVALID_PARAMETER;
    } else if (FsInformationClass == 4) { // FileFsDeviceInformation
            struct FILE_FS_DEVICE_INFORMATION {
                uint32_t DeviceType;
                uint32_t Characteristics;
            };
            if (Length >= sizeof(FILE_FS_DEVICE_INFORMATION)) {
                memset(FsInformation, 0, sizeof(FILE_FS_DEVICE_INFORMATION));
                auto* info = (FILE_FS_DEVICE_INFORMATION*)FsInformation;
                info->DeviceType = is_cdrom_handle(FileHandle) ? 2 : 7;
                info->Characteristics = is_cdrom_handle(FileHandle) ? 0x22 : 0x20;
                info_size = sizeof(FILE_FS_DEVICE_INFORMATION);
            } else status = STATUS_INFO_LENGTH_MISMATCH;
    } else if (FsInformationClass == 3) { // FileFsSizeInformation
            struct FILE_FS_SIZE_INFORMATION {
                uint64_t TotalAllocationUnits;
                uint64_t AvailableAllocationUnits;
                uint32_t SectorsPerAllocationUnit;
                uint32_t BytesPerSector;
            };
            if (Length >= sizeof(FILE_FS_SIZE_INFORMATION)) {
                struct statvfs volume_info{};
                if (fstatvfs(get_fd(FileHandle), &volume_info) != 0) {
                    status = status_from_errno(errno);
                } else {
                    memset(FsInformation, 0, sizeof(FILE_FS_SIZE_INFORMATION));
                    auto* info = (FILE_FS_SIZE_INFORMATION*)FsInformation;
                    constexpr uint32_t xbox_sector_bytes = 512;
                    constexpr uint32_t fatx_allocation_bytes = 16 * 1024;
                    const uint32_t allocation_bytes = is_cdrom_handle(FileHandle)
                        ? static_cast<uint32_t>(volume_info.f_frsize) : fatx_allocation_bytes;
                    info->TotalAllocationUnits =
                        (static_cast<uint64_t>(volume_info.f_blocks) * volume_info.f_frsize) / allocation_bytes;
                    info->AvailableAllocationUnits =
                        (static_cast<uint64_t>(volume_info.f_bavail) * volume_info.f_frsize) / allocation_bytes;
                    info->SectorsPerAllocationUnit = (allocation_bytes + xbox_sector_bytes - 1) / xbox_sector_bytes;
                    info->BytesPerSector = xbox_sector_bytes;
                    info_size = sizeof(FILE_FS_SIZE_INFORMATION);
                }
            } else status = STATUS_INFO_LENGTH_MISMATCH;
    } else {
        status = STATUS_INVALID_INFO_CLASS;
    }

    set_io_status(static_cast<IO_STATUS_BLOCK*>(IoStatusBlock), status, info_size);
    return status;
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
    if (!FileHandle) {
        set_io_status(IoStatusBlock, STATUS_INVALID_PARAMETER);
        return STATUS_INVALID_PARAMETER;
    }
    *FileHandle = nullptr;

    std::string xbox_path;
    NTSTATUS status = get_xbox_path(ObjectAttributes, xbox_path);
    if (status != STATUS_SUCCESS) {
        set_io_status(IoStatusBlock, status);
        return status;
    }
    const std::string linux_path = vfs_translate_path(xbox_path);
    if (linux_path.empty()) {
        set_io_status(IoStatusBlock, STATUS_OBJECT_PATH_NOT_FOUND);
        return STATUS_OBJECT_PATH_NOT_FOUND;
    }
    if (CreateDisposition > 5) {
        set_io_status(IoStatusBlock, STATUS_INVALID_PARAMETER);
        return STATUS_INVALID_PARAMETER;
    }

    constexpr uint32_t FILE_DIRECTORY_FILE = 0x00000001;
    constexpr uint32_t FILE_NON_DIRECTORY_FILE = 0x00000040;
    const bool directory = (CreateOptions & FILE_DIRECTORY_FILE) != 0;
    if (directory && (CreateOptions & FILE_NON_DIRECTORY_FILE) != 0) {
        set_io_status(IoStatusBlock, STATUS_INVALID_PARAMETER);
        return STATUS_INVALID_PARAMETER;
    }

    const bool create = CreateDisposition == 0 || CreateDisposition == 2 ||
                        CreateDisposition == 3 || CreateDisposition == 5;
    const bool exclusive = CreateDisposition == 2;
    const bool truncate = CreateDisposition == 0 || CreateDisposition == 4 || CreateDisposition == 5;
    if (directory && truncate) {
        set_io_status(IoStatusBlock, STATUS_INVALID_PARAMETER);
        return STATUS_INVALID_PARAMETER;
    }
    if (vfs_is_cdrom_path(xbox_path) && (requests_write_access(DesiredAccess) || create || truncate)) {
        set_io_status(IoStatusBlock, STATUS_MEDIA_WRITE_PROTECTED);
        return STATUS_MEDIA_WRITE_PROTECTED;
    }
    const bool existed = access(linux_path.c_str(), F_OK) == 0;

    int fd = -1;
    if (directory) {
        if (create && mkdir(linux_path.c_str(), 0777) != 0 &&
            (errno != EEXIST || exclusive)) {
            status = status_from_errno(errno);
            set_io_status(IoStatusBlock, status);
            return status;
        }
        fd = open(linux_path.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    } else {
        int flags = access_mode(DesiredAccess) | O_CLOEXEC;
        if (create) flags |= O_CREAT;
        if (exclusive) flags |= O_EXCL;
        if (truncate) flags |= O_TRUNC;
        fd = open(linux_path.c_str(), flags, 0666);
    }
    if (fd < 0) {
        status = status_from_errno(errno);
        set_io_status(IoStatusBlock, status);
        printf("[KERNEL] NtCreateFile('%s' -> '%s') failed: %s\n", xbox_path.c_str(), linux_path.c_str(), strerror(errno));
        return status;
    }

    *FileHandle = register_file_handle(fd, vfs_is_cdrom_path(xbox_path));
    uintptr_t information = existed ? 1 : 2;
    if (existed && CreateDisposition == 0) information = 0;
    else if (existed && (CreateDisposition == 4 || CreateDisposition == 5)) information = 3;
    set_io_status(IoStatusBlock, STATUS_SUCCESS, information);
    printf("[KERNEL] NtCreateFile('%s' -> '%s') opened handle %p\n", xbox_path.c_str(), linux_path.c_str(), *FileHandle);
    return STATUS_SUCCESS;
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

    const int fd = get_fd(FileHandle);
    if (fd < 0 || (Length > 0 && !Buffer)) {
        const NTSTATUS status = fd < 0 ? STATUS_INVALID_HANDLE : STATUS_INVALID_PARAMETER;
        set_io_status(IoStatusBlock, status);
        return status;
    }
    if (ByteOffset && *ByteOffset < 0) {
        set_io_status(IoStatusBlock, STATUS_INVALID_PARAMETER);
        return STATUS_INVALID_PARAMETER;
    }
    ssize_t bytes_read = -1;

    if (ByteOffset) {
        bytes_read = pread(fd, Buffer, Length, *ByteOffset);
    } else {
        bytes_read = read(fd, Buffer, Length);
    }

    if (bytes_read < 0) {
        printf("[KERNEL] NtReadFile -> Okuma basarisiz! (Hata: %s)\n", strerror(errno));
        const NTSTATUS status = status_from_errno(errno);
        set_io_status(IoStatusBlock, status);
        return status;
    }

    set_io_status(IoStatusBlock, STATUS_SUCCESS, static_cast<uintptr_t>(bytes_read));
    return STATUS_SUCCESS;
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

    const int fd = get_fd(FileHandle);
    if (fd < 0) {
        set_io_status(IoStatusBlock, STATUS_INVALID_HANDLE);
        return STATUS_INVALID_HANDLE;
    }

    if (!FileInformation) {
        set_io_status(IoStatusBlock, STATUS_INVALID_PARAMETER);
        return STATUS_INVALID_PARAMETER;
    }

    struct stat stat_info{};
    if (fstat(fd, &stat_info) != 0) {
        const NTSTATUS status = status_from_errno(errno);
        set_io_status(IoStatusBlock, status);
        return status;
    }

    if (FileInformationClass == 5) {
        struct FILE_STANDARD_INFORMATION {
            uint64_t AllocationSize;
            uint64_t EndOfFile;
            uint32_t NumberOfLinks;
            uint8_t DeletePending;
            uint8_t Directory;
            uint8_t Reserved[2];
        };
        if (Length < sizeof(FILE_STANDARD_INFORMATION)) {
            set_io_status(IoStatusBlock, STATUS_INFO_LENGTH_MISMATCH);
            return STATUS_INFO_LENGTH_MISMATCH;
        }
        auto* info = static_cast<FILE_STANDARD_INFORMATION*>(FileInformation);
        memset(info, 0, sizeof(*info));
        info->AllocationSize = static_cast<uint64_t>(stat_info.st_blocks) * 512;
        info->EndOfFile = stat_info.st_size;
        info->NumberOfLinks = static_cast<uint32_t>(stat_info.st_nlink);
        info->Directory = S_ISDIR(stat_info.st_mode) ? 1 : 0;
        set_io_status(IoStatusBlock, STATUS_SUCCESS, sizeof(*info));
        return STATUS_SUCCESS;
    }

    if (FileInformationClass == 0x22) {
        struct FILE_NETWORK_OPEN_INFORMATION {
            uint64_t CreationTime;
            uint64_t LastAccessTime;
            uint64_t LastWriteTime;
            uint64_t ChangeTime;
            uint64_t AllocationSize;
            uint64_t EndOfFile;
            uint32_t FileAttributes;
            uint32_t Reserved;
        };
        if (Length < sizeof(FILE_NETWORK_OPEN_INFORMATION)) {
            set_io_status(IoStatusBlock, STATUS_INFO_LENGTH_MISMATCH);
            return STATUS_INFO_LENGTH_MISMATCH;
        }
        auto* info = static_cast<FILE_NETWORK_OPEN_INFORMATION*>(FileInformation);
        memset(info, 0, sizeof(*info));
        info->AllocationSize = static_cast<uint64_t>(stat_info.st_blocks) * 512;
        info->EndOfFile = stat_info.st_size;
        info->FileAttributes = S_ISDIR(stat_info.st_mode) ? 0x10 : 0x80;
        set_io_status(IoStatusBlock, STATUS_SUCCESS, sizeof(*info));
        return STATUS_SUCCESS;
    }

    set_io_status(IoStatusBlock, STATUS_INVALID_INFO_CLASS);
    return STATUS_INVALID_INFO_CLASS;
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
        set_io_status(IoStatusBlock, STATUS_INVALID_PARAMETER);
        return STATUS_INVALID_PARAMETER;
    }

    const int fd = get_fd(FileHandle);
    if (fd < 0) {
        printf("[KERNEL] NtWriteFile -> Gecersiz Handle: %p\n", FileHandle);
        set_io_status(IoStatusBlock, STATUS_INVALID_HANDLE);
        return STATUS_INVALID_HANDLE;
    }
    if (is_cdrom_handle(FileHandle)) {
        set_io_status(IoStatusBlock, STATUS_MEDIA_WRITE_PROTECTED);
        return STATUS_MEDIA_WRITE_PROTECTED;
    }
    if (ByteOffset && *ByteOffset < 0) {
        set_io_status(IoStatusBlock, STATUS_INVALID_PARAMETER);
        return STATUS_INVALID_PARAMETER;
    }

    ssize_t bytes_written = 0;
    if (ByteOffset && *ByteOffset >= 0) {
        bytes_written = pwrite(fd, Buffer, Length, (off_t)*ByteOffset);
    } else {
        bytes_written = write(fd, Buffer, Length);
    }

    if (bytes_written < 0) {
        printf("[KERNEL] NtWriteFile -> Yazma hatasi: %s (FD: %d)\n", strerror(errno), fd);
        const NTSTATUS status = status_from_errno(errno);
        set_io_status(IoStatusBlock, status);
        return status;
    }

    set_io_status(IoStatusBlock, STATUS_SUCCESS, static_cast<uintptr_t>(bytes_written));

    if (Event) {
        xbox_KeSetEvent(Event, 0, 0);
    }

    printf("[KERNEL] NtWriteFile -> Handle: %p (FD: %d), Boyut: %u, Yazilan: %zd bayt\n",
           FileHandle, fd, Length, bytes_written);

    return STATUS_SUCCESS;
}

} // namespace kernel
} // namespace xbe

extern "C" NTSTATUS __attribute__((__stdcall__)) xbox_IoCreateDevice(
    void* DriverObject,
    uint32_t DeviceExtensionSize,
    xbe::kernel::STRING* DeviceName,
    uint32_t DeviceType,
    uint8_t Exclusive,
    void** DeviceObject
) {
    xbe::record_kernel_call(65);
    
    uint32_t devObjSize = 0x40; // 64 bytes is enough for DEVICE_OBJECT
    void* mem = malloc(devObjSize + DeviceExtensionSize);
    memset(mem, 0, devObjSize + DeviceExtensionSize);
    
    uint32_t* devObj = (uint32_t*)mem;
    devObj[0] = 3; // Type (IO_TYPE_DEVICE)
    devObj[2] = (uint32_t)DriverObject; // DriverObject (0x08)
    devObj[6] = (uint32_t)mem + devObjSize; // DeviceExtension (0x18)
    devObj[7] = DeviceType; // DeviceType (0x1C)
    
    if (DeviceObject) {
        *DeviceObject = mem;
    }
    
    if (DeviceName && DeviceName->Buffer) {
        std::string name(DeviceName->Buffer, DeviceName->Length);
        printf("[KERNEL] IoCreateDevice(Name: '%s', Type: %u, ExtSize: %u) -> DevObj: %p, Ext: 0x%08X\n", name.c_str(), DeviceType, DeviceExtensionSize, mem, devObj[6]);
    } else {
        printf("[KERNEL] IoCreateDevice(Name: NULL, Type: %u, ExtSize: %u) -> DevObj: %p, Ext: 0x%08X\n", DeviceType, DeviceExtensionSize, mem, devObj[6]);
    }
    
    return 0; // STATUS_SUCCESS
}
