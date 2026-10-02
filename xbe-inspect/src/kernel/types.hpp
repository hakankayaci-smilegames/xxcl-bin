#pragma once
#include <cstdint>

namespace xbe {
namespace kernel {

#pragma pack(push, 4)
typedef struct _STRING {
    uint16_t Length;
    uint16_t MaximumLength;
    char* Buffer;
} STRING, *PSTRING, ANSI_STRING, *PANSI_STRING;
#pragma pack(pop)


#pragma pack(push, 1)
struct OBJECT_ATTRIBUTES {
    void* RootDirectory;
    STRING* ObjectName;
    uint32_t Attributes;
};

struct IO_STATUS_BLOCK {
    union {
        uint32_t Status;
        void* Pointer;
    };
    uintptr_t Information;
};
#pragma pack(pop)

} // namespace kernel
} // namespace xbe
