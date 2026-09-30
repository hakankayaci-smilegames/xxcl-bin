import struct
with open("/home/hakan/Desktop/Sega GT 2002 (Europe) (En,Ja,Fr,De,Es,It)/default.xbe", "rb") as f:
    # Kernel thunks is at file offset 0x1A9000
    f.seek(0x1A9000)
    for i in range(121):
        val = struct.unpack("<I", f.read(4))[0]
        if val == 0: break
        ordinal = val & 0x7FFFFFFF
        va = 0x1B6BA0 + i * 4
        if va == 0x1B6C6C:
            print(f"Address 0x1B6C6C contains ordinal {ordinal} (Index {i})")
