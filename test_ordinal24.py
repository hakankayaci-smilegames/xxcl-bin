import struct
with open("/home/hakan/Desktop/Sega GT 2002 (Europe) (En,Ja,Fr,De,Es,It)/default.xbe", "rb") as f:
    f.seek(0x1A9000)
    for i in range(121):
        val = struct.unpack("<I", f.read(4))[0]
        if val == 0: break
        ordinal = val & 0x7FFFFFFF
        va = 0x1B6BA0 + i * 4
        if ordinal == 24:
            print(f"Ordinal 24 found at 0x{va:X} (Index {i})")
