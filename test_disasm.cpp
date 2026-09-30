#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
#include <cstdio>

int main() {
    std::ifstream file("/home/hakan/Desktop/Sega GT 2002 (Europe) (En,Ja,Fr,De,Es,It)/default.xbe", std::ios::binary);
    file.seekg(0x104C20 - 0x10000 + 0x1000);
    std::vector<uint8_t> code(100);
    file.read((char*)code.data(), 100);
    
    std::cout << "Code at 0x104C20: ";
    for(int i=0; i<100; i++) {
        printf("%02X ", code[i]);
    }
    std::cout << "\n";
    return 0;
}
