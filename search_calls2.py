import sys

xbe_path = "/home/hakan/Desktop/Sega GT 2002 (Europe) (En,Ja,Fr,De,Es,It)/default.xbe"
target_addr = 0x001605f4

with open(xbe_path, "rb") as f:
    data = f.read()

v_base = 0x0015F540
r_base = 0x00151000
v_size = 0x0001E02C
dsound_data = data[r_base : r_base + v_size]

for i in range(len(dsound_data) - 4):
    if dsound_data[i] == 0xE8: # call rel32
        rel32 = int.from_bytes(dsound_data[i+1:i+5], byteorder='little', signed=True)
        call_addr = v_base + i + 5 + rel32
        if call_addr == target_addr:
            print(f"Found call to {hex(target_addr)} at {hex(v_base + i)}")
