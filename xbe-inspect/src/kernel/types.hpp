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

} // namespace kernel
} // namespace xbe
