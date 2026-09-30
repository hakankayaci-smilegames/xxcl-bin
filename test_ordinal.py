import struct
with open("/home/hakan/Desktop/Sega GT 2002 (Europe) (En,Ja,Fr,De,Es,It)/default.xbe", "rb") as f:
    f.seek(0x158)
    kernel_thunk, _, _, xapi_thunk = struct.unpack("<IIII", f.read(16))
    print(f"Kernel Thunk: {hex(kernel_thunk)}, XAPI Thunk: {hex(xapi_thunk)}")
    
    if xapi_thunk != 0:
        index = (0x1B6C6C - xapi_thunk) // 4
        print(f"Index in XAPI thunks: {index}")
