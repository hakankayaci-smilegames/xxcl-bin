#include <iostream>
#include <fstream>
#include <vector>
#include <iomanip>
#include <capstone/capstone.h>

int main(int argc, char** argv) {
    if (argc < 2) return 1;
    std::ifstream file(argv[1], std::ios::binary);
    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> data(size);
    file.read(reinterpret_cast<char*>(data.data()), size);

    uint32_t raw_addr = 0x815b0 - 0x10000 + 0x10000; // wait, .text raw address is 0x10000, virtual address is 0x10000.
    // So 0x815b0 virtual == 0x815b0 raw (since base=0x10000, raw=0x10000)
    
    csh handle;
    if (cs_open(CS_ARCH_X86, CS_MODE_32, &handle) != CS_ERR_OK) return 1;
    cs_insn *insn;
    size_t count = cs_disasm(handle, data.data() + 0x815a0, 0x50, 0x815a0, 0, &insn);
    if (count > 0) {
        for (size_t i = 0; i < count; i++) {
            printf("0x%08" PRIx64 ":\t%s\t%s\n", insn[i].address, insn[i].mnemonic, insn[i].op_str);
        }
        cs_free(insn, count);
    }
    cs_close(&handle);
    return 0;
}
