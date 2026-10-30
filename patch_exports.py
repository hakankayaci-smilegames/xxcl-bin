import re

with open('xbe-inspect/src/kernel/exports.cpp', 'r') as f:
    content = f.read()

new_exports = """
    {2, reinterpret_cast<void*>(&xbox_AvSendTVEncoderOption)},
    {14, reinterpret_cast<void*>(&xbox_ExAllocatePool)},
    {15, reinterpret_cast<void*>(&xbox_ExAllocatePoolWithTag)},
    {17, reinterpret_cast<void*>(&xbox_ExFreePool)},
    {23, reinterpret_cast<void*>(&xbox_ExQueryPoolBlockSize)},
    {65, reinterpret_cast<void*>(&xbox_IoCreateDevice)},
    {123, reinterpret_cast<void*>(&xbox_KePulseEvent)},
    {129, reinterpret_cast<void*>(&xbox_KeRaiseIrqlToDpcLevel)},
    {151, reinterpret_cast<void*>(&xbox_KeStallExecutionProcessor)},
    {160, reinterpret_cast<void*>(&xbox_KfRaiseIrql)},
    {161, reinterpret_cast<void*>(&xbox_KfLowerIrql)},
    {173, reinterpret_cast<void*>(&xbox_MmGetPhysicalAddress)},
    {175, reinterpret_cast<void*>(&xbox_MmLockUnlockBufferPages)},
    {180, reinterpret_cast<void*>(&xbox_MmQueryAllocationSize)},
"""

pattern = r"(static std::unordered_map<uint32_t, void\*> kernel_exports = \{)"
replacement = r"\1\n" + new_exports

updated = re.sub(pattern, replacement, content)

with open('xbe-inspect/src/kernel/exports.cpp', 'w') as f:
    f.write(updated)
