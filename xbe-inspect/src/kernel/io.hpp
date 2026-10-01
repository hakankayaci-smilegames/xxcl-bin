#pragma once
#include <cstdint>
#include "types.hpp"

typedef int32_t NTSTATUS;

#pragma pack(push, 1)
struct XBOX_OBJECT_ATTRIBUTES {
    void* RootDirectory;
    xbe::kernel::STRING* ObjectName;
    uint32_t Attributes;
};
#pragma pack(pop)

namespace xbe {
namespace kernel {

extern "C" {
    NTSTATUS __attribute__((__stdcall__)) xbox_NtClose(uint32_t Handle);
    
    NTSTATUS __attribute__((__stdcall__)) xbox_NtOpenFile(
        void** FileHandle,
        uint32_t DesiredAccess,
        XBOX_OBJECT_ATTRIBUTES* ObjectAttributes,
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

    NTSTATUS __attribute__((__stdcall__)) xbox_NtQueryVolumeInformationFile(
        void* FileHandle,
        void* IoStatusBlock,
        void* FsInformation,
        uint32_t Length,
        uint32_t FsInformationClass
    );
}

} // namespace kernel
} // namespace xbe
