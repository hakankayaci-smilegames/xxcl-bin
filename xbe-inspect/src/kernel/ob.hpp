#pragma once
#include <cstdint>
#include "types.hpp" // needed for STRING

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
}

} // namespace kernel
} // namespace xbe
