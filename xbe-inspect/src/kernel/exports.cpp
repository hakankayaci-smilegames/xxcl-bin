#include "exports.hpp"
#include "xbox_krnl_exports.hpp"
#include "rtl.hpp"
#include "ps.hpp"
#include "ex.hpp"
#include "ke.hpp"
#include "io.hpp"
#include "mm.hpp"
#include "hal.hpp"
#include "xe.hpp"
#include <unordered_map>
#include <cstdio>

namespace xbe {
namespace kernel {

static std::unordered_map<uint32_t, void*> kernel_exports = {
    {24,  reinterpret_cast<void*>(&xbox_ExQueryNonVolatileSetting)},
    {44, reinterpret_cast<void*>(&xbox_HalGetInterruptVector)},
    {47, reinterpret_cast<void*>(&xbox_HalRegisterShutdownNotification)},
    {49, reinterpret_cast<void*>(&xbox_HalReturnToFirmware)},
    {107, reinterpret_cast<void*>(&xbox_KeInitializeDpc)},
    {113, reinterpret_cast<void*>(&xbox_KeInitializeTimerEx)},
    {130, reinterpret_cast<void*>(&xbox_KeSetEvent)},
    {149, reinterpret_cast<void*>(&xbox_KeSetTimer)},
    {164, reinterpret_cast<void*>(&xbox_LaunchDataPage)},
    {165, reinterpret_cast<void*>(&xbox_MmAllocateContiguousMemory)},
    {166, reinterpret_cast<void*>(&xbox_MmAllocateContiguousMemoryEx)},
    {167, reinterpret_cast<void*>(&xbox_MmFreeContiguousMemory)},
    {178, reinterpret_cast<void*>(&xbox_MmPersistContiguousMemory)},
    {184, reinterpret_cast<void*>(&xbox_NtAllocateVirtualMemory)},
    {165, reinterpret_cast<void*>(&xbox_MmAllocateContiguousMemory)},
    {166, reinterpret_cast<void*>(&xbox_MmAllocateContiguousMemoryEx)},
    {167, reinterpret_cast<void*>(&xbox_MmFreeContiguousMemory)},
    {196, reinterpret_cast<void*>(&xbox_NtDeviceIoControlFile)},
    {199, reinterpret_cast<void*>(&xbox_NtFreeVirtualMemory)},
    {202, reinterpret_cast<void*>(&xbox_NtOpenFile)},
    {218, reinterpret_cast<void*>(&xbox_NtQueryVolumeInformationFile)},
    {186, reinterpret_cast<void*>(&xbox_MmFreeContiguousMemory)},
    {187, reinterpret_cast<void*>(&xbox_NtClose)},
    {190, reinterpret_cast<void*>(&xbox_NtCreateFile)},
    {211, reinterpret_cast<void*>(&xbox_NtQueryInformationFile)},
    {236, reinterpret_cast<void*>(&xbox_NtWriteFile)},
    {219, reinterpret_cast<void*>(&xbox_NtReadFile)},
    {255, reinterpret_cast<void*>(&xbox_PsCreateSystemThreadEx)},
    {277, reinterpret_cast<void*>(&xbox_RtlEnterCriticalSection)},
    {268, reinterpret_cast<void*>(&xbox_RtlCompareMemory)},
    {294, reinterpret_cast<void*>(&xbox_RtlLeaveCriticalSection)},
    {279, reinterpret_cast<void*>(&xbox_RtlEqualString)},
    {289, reinterpret_cast<void*>(&xbox_RtlInitAnsiString)},
    {291, reinterpret_cast<void*>(&xbox_RtlInitializeCriticalSection)},
    {300, reinterpret_cast<void*>(&xbox_RtlMultiByteToUnicodeSize)}, // TODO: check if actually implemented
    {301, reinterpret_cast<void*>(&xbox_RtlNtStatusToDosError)},
    {302, reinterpret_cast<void*>(&xbox_RtlRaiseException)}, // TODO: check if actually implemented
    {68, reinterpret_cast<void*>(&xbox_IoDeleteSymbolicLink)},
    {67, reinterpret_cast<void*>(&xbox_IoCreateSymbolicLink)},
    {327, reinterpret_cast<void*>(&xbox_XeLoadSection)},
    {328, reinterpret_cast<void*>(&xbox_XeUnloadSection)},
    {98, reinterpret_cast<void*>(&xbox_KeConnectInterrupt)},
    {109, reinterpret_cast<void*>(&xbox_KeInitializeInterrupt)},
    {168, reinterpret_cast<void*>(&xbox_MmClaimGpuInstanceMemory)}
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
