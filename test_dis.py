from capstone import *
code = bytes.fromhex("00 90 90 90 83 ec 08 6a 00 8d 44 24 04 50 6a 00 e8 a9 81 08 00 85 c0 7d 06 32 c0 83 c4 08 c3 8b 4c 24 00 53 8a 5c 24 10 84 db 89 0d 80 fc 40 00")
md = Cs(CS_ARCH_X86, CS_MODE_32)
for i in md.disasm(code, 0x1000):
    print("0x%x:\t%s\t%s" %(i.address, i.mnemonic, i.op_str))
