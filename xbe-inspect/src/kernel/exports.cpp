#include "exports.hpp"
#include "rtl.hpp"
#include "mm.hpp"
#include "hal.hpp"
#include "ps.hpp"
#include <unordered_map>

extern "C" uint32_t __attribute__((__stdcall__)) xbox_Ordinal187(uint32_t arg1);
extern "C" uint32_t __attribute__((__stdcall__)) xbox_Ordinal107();

namespace xbe {
namespace kernel {

static std::unordered_map<uint32_t, void*> kernel_exports = {
    {187, reinterpret_cast<void*>(&xbox_Ordinal187)},
    {107, reinterpret_cast<void*>(&xbox_Ordinal107)},
    {255, reinterpret_cast<void*>(&xbox_PsCreateSystemThreadEx)},
    {289, reinterpret_cast<void*>(&xbox_RtlInitAnsiString)},
    {300, reinterpret_cast<void*>(&xbox_RtlZeroMemory)},
    {301, reinterpret_cast<void*>(&xbox_RtlFillMemory)},
    {302, reinterpret_cast<void*>(&xbox_RtlMoveMemory)},
    {184, reinterpret_cast<void*>(&xbox_MmAllocateContiguousMemory)},
    {186, reinterpret_cast<void*>(&xbox_MmFreeContiguousMemory)},
    {84,  reinterpret_cast<void*>(&xbox_HalReturnToFirmware)},
};

void* get_kernel_export(uint32_t ordinal) {
    auto it = kernel_exports.find(ordinal);
    if (it != kernel_exports.end()) {
        return it->second;
    }
    return nullptr;
}

} // namespace kernel
} // namespace xbe
