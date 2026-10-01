#include "exports.hpp"
#include "xbox_krnl_exports.hpp"
#include "rtl.hpp"
#include "ps.hpp"
#include "ex.hpp"
#include "ke.hpp"
#include "io.hpp"
#include "mm.hpp"
#include "hal.hpp"
#include <unordered_map>
#include <cstdio>

namespace xbe {
namespace kernel {

static std::unordered_map<uint32_t, void*> kernel_exports = {
    {24,  reinterpret_cast<void*>(&xbox_ExQueryNonVolatileSetting)},
    {49,  reinterpret_cast<void*>(&xbox_HalReturnToFirmware)},
    {107, reinterpret_cast<void*>(&xbox_KeInitializeDpc)},
    {113, reinterpret_cast<void*>(&xbox_KeInitializeTimerEx)},
    {149, reinterpret_cast<void*>(&xbox_KeSetTimer)},
    {164, reinterpret_cast<void*>(&xbox_LaunchDataPage)},
    {165, reinterpret_cast<void*>(&xbox_MmAllocateContiguousMemory)},
    {178, reinterpret_cast<void*>(&xbox_MmPersistContiguousMemory)},
    {184, reinterpret_cast<void*>(&xbox_NtAllocateVirtualMemory)},
    {196, reinterpret_cast<void*>(&xbox_NtDeviceIoControlFile)},
    {199, reinterpret_cast<void*>(&xbox_NtFreeVirtualMemory)},
    {202, reinterpret_cast<void*>(&xbox_NtOpenFile)},
    {218, reinterpret_cast<void*>(&xbox_NtQueryVolumeInformationFile)},
    {186, reinterpret_cast<void*>(&xbox_MmFreeContiguousMemory)},
    {187, reinterpret_cast<void*>(&xbox_NtClose)},
    {255, reinterpret_cast<void*>(&xbox_PsCreateSystemThreadEx)},
    {289, reinterpret_cast<void*>(&xbox_RtlInitAnsiString)},
    {291, reinterpret_cast<void*>(&xbox_RtlInitializeCriticalSection)},
    {300, reinterpret_cast<void*>(&xbox_RtlMultiByteToUnicodeSize)}, // TODO: check if actually implemented
    {301, reinterpret_cast<void*>(&xbox_RtlNtStatusToDosError)},
    {302, reinterpret_cast<void*>(&xbox_RtlRaiseException)}, // TODO: check if actually implemented
    {68, reinterpret_cast<void*>(&xbox_IoDeleteSymbolicLink)},
    {67, reinterpret_cast<void*>(&xbox_IoCreateSymbolicLink)},
};

void* get_kernel_export(uint32_t ordinal) {
    auto it = kernel_exports.find(ordinal);
    if (it != kernel_exports.end()) {
        return it->second;
    }
    return nullptr;
}

const char* get_kernel_export_name(uint32_t ordinal) {
    if (ordinal < 400) {
        return xbox_krnl_names[ordinal];
    }
    return "Unknown";
}

} // namespace kernel
} // namespace xbe
