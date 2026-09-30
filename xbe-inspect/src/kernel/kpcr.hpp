#pragma once
#include <cstdint>

namespace xbe {
namespace kernel {

#pragma pack(push, 1)
struct XBOX_KPCR {
    void* NtTib;               // 0x00: ExceptionList
    void* StackBase;           // 0x04
    void* StackLimit;          // 0x08
    void* SubSystemTib;        // 0x0C
    void* FiberData;           // 0x10
    void* ArbitraryUserPointer;// 0x14
    XBOX_KPCR* Self;           // 0x18: fs:[0x18] KPCR Self Pointer
    void* PCRB;                // 0x1C
    void* CurrentThread;       // 0x20
    uint8_t Irql;              // 0x24: fs:[0x24] Current IRQL (0 = PASSIVE_LEVEL)
    uint8_t Padding[3];        // 0x25-0x27
    void* CurrentThreadPtr;    // 0x28: fs:[0x28] PKTHREAD pointer
};
#pragma pack(pop)

void init_xbox_kpcr();

} // namespace kernel
} // namespace xbe
