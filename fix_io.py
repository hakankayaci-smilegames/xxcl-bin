import re

with open("xbe-inspect/src/kernel/io.cpp", "r") as f:
    code = f.read()

new_func = """extern "C" NTSTATUS __attribute__((__stdcall__)) xbox_IoCreateDevice(
    void* DriverObject,
    uint32_t DeviceExtensionSize,
    xbe::kernel::STRING* DeviceName,
    uint32_t DeviceType,
    uint8_t Exclusive,
    void** DeviceObject
) {
    xbe::record_kernel_call(65);
    
    uint32_t devObjSize = 0x40; // 64 bytes is enough for DEVICE_OBJECT
    void* mem = malloc(devObjSize + DeviceExtensionSize);
    memset(mem, 0, devObjSize + DeviceExtensionSize);
    
    uint32_t* devObj = (uint32_t*)mem;
    devObj[0] = 3; // Type (IO_TYPE_DEVICE)
    devObj[2] = (uint32_t)DriverObject; // DriverObject (0x08)
    devObj[6] = (uint32_t)mem + devObjSize; // DeviceExtension (0x18)
    devObj[7] = DeviceType; // DeviceType (0x1C)
    
    if (DeviceObject) {
        *DeviceObject = mem;
    }
    
    if (DeviceName && DeviceName->Buffer) {
        std::string name(DeviceName->Buffer, DeviceName->Length);
        printf("[KERNEL] IoCreateDevice(Name: '%s', Type: %u, ExtSize: %u) -> DevObj: %p, Ext: 0x%08X\\n", name.c_str(), DeviceType, DeviceExtensionSize, mem, devObj[6]);
    } else {
        printf("[KERNEL] IoCreateDevice(Name: NULL, Type: %u, ExtSize: %u) -> DevObj: %p, Ext: 0x%08X\\n", DeviceType, DeviceExtensionSize, mem, devObj[6]);
    }
    
    return 0; // STATUS_SUCCESS
}"""

old_func = re.search(r'extern "C" NTSTATUS __attribute__\(\(__stdcall__\)\) xbox_IoCreateDevice.*?return 0; // STATUS_SUCCESS\n}', code, re.DOTALL)

if old_func:
    code = code.replace(old_func.group(0), new_func)
    with open("xbe-inspect/src/kernel/io.cpp", "w") as f:
        f.write(code)
    print("Replaced IoCreateDevice")
else:
    print("Could not find IoCreateDevice in io.cpp")
