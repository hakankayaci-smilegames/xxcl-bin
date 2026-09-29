import struct

MAGIC = b'XBEH'
BASE_ADDRESS = 0x00010000
CERT_OFFSET = 0x178
SEC_HDR_OFFSET = CERT_OFFSET + 464
NUM_SECTIONS = 2
SEC_NAME_OFFSET = SEC_HDR_OFFSET + (NUM_SECTIONS * 56)

TEXT_RAW_OFFSET = 0x1000
DATA_RAW_OFFSET = 0x2000

def write_dummy_xbe(filename):
    with open(filename, 'wb') as f:
        f.write(b'\x00' * 0x1000)
        
        f.seek(0)
        f.write(MAGIC)
        f.seek(0x104)
        f.write(struct.pack('<I', BASE_ADDRESS))
        f.write(struct.pack('<I', 0x1000)) # sizeof_headers
        
        ep_va = BASE_ADDRESS + TEXT_RAW_OFFSET
        ep = ep_va ^ 0xA8FC57AB
        
        f.seek(0x118)
        f.write(struct.pack('<I', BASE_ADDRESS + CERT_OFFSET))
        f.write(struct.pack('<I', NUM_SECTIONS))
        f.write(struct.pack('<I', BASE_ADDRESS + SEC_HDR_OFFSET))
        
        f.seek(0x128)
        f.write(struct.pack('<I', ep))
        
        # Thunk table address (XORed with 0x5B6D40B6)
        thunk_va = BASE_ADDRESS + DATA_RAW_OFFSET
        thunk_encoded = thunk_va ^ 0x5B6D40B6
        f.seek(0x164)
        f.write(struct.pack('<I', thunk_encoded))
        
        f.seek(CERT_OFFSET)
        f.write(struct.pack('<I', 464))
        f.seek(CERT_OFFSET + 8)
        f.write(struct.pack('<I', 0x12345678))
        
        title_str = "Dummy Xbox Game".encode('utf-16le').ljust(80, b'\x00')
        f.write(title_str)
        
        f.seek(SEC_HDR_OFFSET)
        name1_va = BASE_ADDRESS + SEC_NAME_OFFSET
        f.write(struct.pack('<IIIII', 0x1, BASE_ADDRESS + TEXT_RAW_OFFSET, 0x1000, TEXT_RAW_OFFSET, 0x1000))
        f.write(struct.pack('<I', name1_va))
        f.write(b'\x00' * (56 - 24))
        
        name2_va = BASE_ADDRESS + SEC_NAME_OFFSET + 6
        f.write(struct.pack('<IIIII', 0x2, BASE_ADDRESS + DATA_RAW_OFFSET, 0x2000, DATA_RAW_OFFSET, 0x2000))
        f.write(struct.pack('<I', name2_va))
        f.write(b'\x00' * (56 - 24))
        
        f.seek(SEC_NAME_OFFSET)
        f.write(b'.text\x00.data\x00')

        f.seek(TEXT_RAW_OFFSET)
        # mov edi, 0x12000       (BF 00 20 01 00)
        # mov eax, dword [rdi]   (8B 07)
        # call rax               (FF D0)
        # mov edi, 0x12004       (BF 04 20 01 00)
        # mov eax, dword [rdi]   (8B 07)
        # call rax               (FF D0)
        # mov eax, 1             (B8 01 00 00 00)
        # ret                    (C3)
        x86_code = b'\xbf\x00\x20\x01\x00\x8b\x07\xff\xd0\xbf\x04\x20\x01\x00\x8b\x07\xff\xd0\xb8\x01\x00\x00\x00\xc3'
        text_section_data = x86_code.ljust(0x1000, b'\x90')
        f.write(text_section_data)

        f.seek(DATA_RAW_OFFSET)
        # Thunk table (Array of imported ordinals)
        # Oridnals are masked with 0x80000000 by convention
        # Ordinal 165 (e.g., debug print), Ordinal 123, then 0 to terminate
        f.write(struct.pack('<I', 0x80000000 | 165))
        f.write(struct.pack('<I', 0x80000000 | 123))
        f.write(struct.pack('<I', 0))
        # pad the rest
        f.write(b'\x00' * (0x2000 - 12))

    print(f"{filename} kernel thunk tablosu ve call komutlariyla olusturuldu.")

if __name__ == "__main__":
    write_dummy_xbe("dummy.xbe")
