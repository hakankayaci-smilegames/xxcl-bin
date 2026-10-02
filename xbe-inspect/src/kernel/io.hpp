#pragma once
#include <cstdint>
#include "types.hpp"

typedef int32_t NTSTATUS;

#pragma pack(push, 1)

namespace xbe {
namespace kernel {

extern "C" {
    NTSTATUS __attribute__((__stdcall__)) xbox_NtClose(void* Handle);
    
    NTSTATUS __attribute__((__stdcall__)) xbox_NtOpenFile(
        void** FileHandle,
        uint32_t DesiredAccess,
        OBJECT_ATTRIBUTES* ObjectAttributes,
        void* IoStatusBlock,
        uint32_t ShareAccess,
        uint32_t OpenOptions
    );

    NTSTATUS __attribute__((__stdcall__)) xbox_NtDeviceIoControlFile(
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
    );

    NTSTATUS __attribute__((__stdcall__)) xbox_IoCreateSymbolicLink(
        xbe::kernel::STRING* SymbolicLinkName,
        xbe::kernel::STRING* DeviceName
    );

    NTSTATUS __attribute__((__stdcall__)) xbox_IoDeleteSymbolicLink(
        xbe::kernel::STRING* SymbolicLinkName
    );

    NTSTATUS __attribute__((__stdcall__)) xbox_NtQueryVolumeInformationFile(
        void* FileHandle,
        void* IoStatusBlock,
        void* FsInformation,
        uint32_t Length,
        uint32_t FsInformationClass
    );
    NTSTATUS __attribute__((__stdcall__)) xbox_NtCreateFile(
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
    );

    NTSTATUS __attribute__((__stdcall__)) xbox_NtReadFile(
        void* FileHandle,
        void* Event,
        void* ApcRoutine,
        void* ApcContext,
        IO_STATUS_BLOCK* IoStatusBlock,
        void* Buffer,
        uint32_t Length,
        int64_t* ByteOffset,
        uint32_t* Key
    );

    
    NTSTATUS __attribute__((__stdcall__)) xbox_NtQueryInformationFile(
        void* FileHandle,
        IO_STATUS_BLOCK* IoStatusBlock,
        void* FileInformation,
        uint32_t Length,
        uint32_t FileInformationClass
    );

    NTSTATUS __attribute__((__stdcall__)) xbox_NtWriteFile(
        void* FileHandle,
        void* Event,
        void* ApcRoutine,
        void* ApcContext,
        IO_STATUS_BLOCK* IoStatusBlock,
        void* Buffer,
        uint32_t Length,
        int64_t* ByteOffset,
        uint32_t* Key
    );
}

} // namespace kernel
} // namespace xbe
