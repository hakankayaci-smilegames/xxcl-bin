import struct
with open("/home/hakan/Desktop/Sega GT 2002 (Europe) (En,Ja,Fr,De,Es,It)/default.xbe", "rb") as f:
    f.seek(0x114)
    sec_count, sec_headers_addr = struct.unpack("<II", f.read(8))
    f.seek(0x104)
    base_addr = struct.unpack("<I", f.read(4))[0]
    f.seek(sec_headers_addr - base_addr)
    for i in range(sec_count):
        data = f.read(56)
        if not data or len(data) < 56: break
        flags, virt_addr, virt_size, raw_addr, raw_size = struct.unpack("<IIIII", data[:20])
        if virt_addr <= 0x104BE7 < virt_addr + virt_size:
            offset = raw_addr + (0x104BE7 - virt_addr)
            f.seek(offset)
            code = f.read(128)
            print("Code:", " ".join(f"{b:02X}" for b in code))
            break
