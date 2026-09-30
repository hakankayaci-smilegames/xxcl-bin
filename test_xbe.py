import struct
with open("/home/hakan/Desktop/Sega GT 2002 (Europe) (En,Ja,Fr,De,Es,It)/default.xbe", "rb") as f:
    f.seek(0x114)
    sec_count, sec_headers_addr = struct.unpack("<II", f.read(8))
    f.seek(0x104)
    base_addr = struct.unpack("<I", f.read(4))[0]
    print(f"Base: {hex(base_addr)}, SecCount: {sec_count}, SecHeaders: {hex(sec_headers_addr)}")
    f.seek(sec_headers_addr - base_addr)
    for i in range(sec_count):
        data = f.read(56)
        if len(data) < 56: break
        flags, virt_addr, virt_size, raw_addr, raw_size = struct.unpack("<IIIII", data[:20])
        print(f"Sec {i}: Virt {hex(virt_addr)} - {hex(virt_addr+virt_size)}, Raw {hex(raw_addr)}")
        if virt_addr <= 0x104C20 < virt_addr + virt_size:
            offset = raw_addr + (0x104C20 - virt_addr)
            print(f"Found! Offset: {hex(offset)}")
            saved_pos = f.tell()
            f.seek(offset)
            code = f.read(128)
            print("Code:", " ".join(f"{b:02X}" for b in code))
            f.seek(saved_pos)
