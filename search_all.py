import sys

xbe_path = "/home/hakan/Desktop/Sega GT 2002 (Europe) (En,Ja,Fr,De,Es,It)/default.xbe"
targets = [0x001605f0, 0x001605f4, 0x00160600, 0x00160614]

with open(xbe_path, "rb") as f:
    data = f.read()

# We need to parse sections or just scan the whole file for E8 calls
# But we need virtual addresses. Let's just do a naive scan for the E8 pattern.
# Actually, the base address of the XBE image is 0x10000.
# A naive scan would need to know the offset to virtual address.
# I'll just use the known sections from previous sessions.
sections = [
    (0x00010000, 0x00010000, 0x000EB480), # .text
    (0x000FB480, 0x000FB480, 0x0000C160), # .rdata
    (0x001075E0, 0x001075E0, 0x00010020), # .data
    (0x00117600, 0x00117600, 0x00021A40), # D3D
    (0x00139040, 0x00139040, 0x000185A0), # XPP
    (0x001515E0, 0x001515E0, 0x0000DF60), # DSOUND ? wait, where was DSOUND?
]
# Wait, I'll just parse the XBE headers.

import struct

base_addr = struct.unpack_from("<I", data, 0x104)[0]
cert_addr = struct.unpack_from("<I", data, 0x118)[0]
num_sections = struct.unpack_from("<I", data, 0x11C)[0]
sec_headers_addr = struct.unpack_from("<I", data, 0x120)[0]

sec_offset = sec_headers_addr - base_addr

for i in range(num_sections):
    flags, v_addr, v_size, raw_addr, raw_size, p_name = struct.unpack_from("<IIIIII", data, sec_offset + i * 56)
    sec_data = data[raw_addr : raw_addr + raw_size]
    for j in range(len(sec_data) - 4):
        if sec_data[j] == 0xE8:
            rel32 = int.from_bytes(sec_data[j+1:j+5], byteorder='little', signed=True)
            call_addr = v_addr + j + 5 + rel32
            if call_addr in targets:
                print(f"Found call to {hex(call_addr)} at {hex(v_addr + j)}")
