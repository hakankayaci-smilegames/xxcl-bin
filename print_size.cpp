#include <iostream>
#include <fstream>
#include <cstdint>

int main() {
    std::ifstream file("/home/hakan/Desktop/Sega GT 2002 (Europe) (En,Ja,Fr,De,Es,It)/default.xbe", std::ios::binary);
    file.seekg(0x10c, std::ios::beg);
    uint32_t size;
    file.read((char*)&size, 4);
    std::cout << "SizeOfImage: 0x" << std::hex << size << std::endl;
    return 0;
}
