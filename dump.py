import sys

def parse_xbe():
    with open("/home/hakan/Desktop/Sega GT 2002 (Europe) (En,Ja,Fr,De,Es,It)/default.xbe", "rb") as f:
        data = f.read()
    
    # BaseAddress is at 0x104
    base_addr = int.from_bytes(data[0x104:0x108], "little")
    
    print(f"Base Address: {hex(base_addr)}")
    
    # We want to look at Virtual Address 0x1AF13B
    va = 0x1AF13B
    
    # Let's search for the byte sequence around it
    # "18 6A 5C 59 33 C0 8B FA F3 AB 8B 45 FC 89 02 89 72 04 6A 04 59 89 4A 0C"
    seq = bytes.fromhex("18 6A 5C 59 33 C0 8B FA F3 AB 8B 45 FC 89 02 89 72 04 6A 04 59 89 4A 0C")
    idx = data.find(seq)
    
    if idx != -1:
        print(f"Found sequence at file offset {hex(idx)}")
        # Let's dump 30 bytes before and 30 bytes after
        start = idx - 30
        end = idx + len(seq)
        dump = data[start:end]
        print("Hex dump:")
        print(dump.hex(" "))
        
        # Write to a bin file to disassemble with ndisasm
        with open("dump.bin", "wb") as f2:
            f2.write(dump)
    else:
        print("Sequence not found in file")

parse_xbe()
