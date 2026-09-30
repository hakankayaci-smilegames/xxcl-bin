#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>

int main() {
    std::ifstream file("/home/hakan/Desktop/Sega GT 2002 (Europe) (En,Ja,Fr,De,Es,It)/default.xbe", std::ios::binary);
    // Let's just find the section by parsing manually
    file.seekg(0x104);
    uint32_t base_addr;
    file.read((char*)&base_addr, 4);
    file.seekg(0x114);
    uint32_t sec_count;
    file.read((char*)&sec_count, 4);
    file.seekg(0x118);
    uint32_t sec_headers_addr;
    file.read((char*)&sec_headers_addr, 4);
    file.seekg(sec_headers_addr - base_addr);
    
    for (int i = 0; i < sec_count; i++) {
        uint32_t flags, virt_addr, virt_size, raw_addr, raw_size;
        file.read((char*)&flags, 4);
        file.read((char*)&virt_addr, 4);
        file.read((char*)&virt_size, 4);
        file.read((char*)&raw_addr, 4);
        file.read((char*)&raw_size, 4);
        file.seekg(36, std::ios::cur);
        if (virt_addr <= 0x001B6C6C && 0x001B6C6C < virt_addr + virt_size) {
            uint32_t offset = raw_addr + (0x001B6C6C - virt_addr);
            std::cout << "Found in section " << i << " offset " << std::hex << offset << "\n";
            file.seekg(offset);
            uint32_t val;
            file.read((char*)&val, 4);
            std::cout << "Value: " << std::hex << val << "\n";
            break;
        }
    }
}
