import struct
import sys

def va_to_raw(va):
    xbe_path = "/home/hakan/Desktop/Sega GT 2002 (Europe) (En,Ja,Fr,De,Es,It)/default.xbe"
    with open(xbe_path, "rb") as f: data = f.read()
    sec_headers_addr = struct.unpack_from("<I", data, 0x120)[0]
    base_addr = struct.unpack_from("<I", data, 0x104)[0]
    num_sections = struct.unpack_from("<I", data, 0x11C)[0]
    for i in range(num_sections):
        _, v_addr, v_size, raw_addr, raw_size, _ = struct.unpack_from("<IIIIII", data, (sec_headers_addr - base_addr) + i * 56)
        if v_addr <= va < v_addr + v_size:
            return raw_addr + (va - v_addr)
    return None

if __name__ == "__main__":
    va = int(sys.argv[1], 16)
    raw = va_to_raw(va)
    if raw is None:
        print("VA not found")
        sys.exit(1)
    
    xbe_path = "/home/hakan/Desktop/Sega GT 2002 (Europe) (En,Ja,Fr,De,Es,It)/default.xbe"
    with open(xbe_path, "rb") as f:
        f.seek(raw)
        with open("snippet.bin", "wb") as out:
            out.write(f.read(0x40))
