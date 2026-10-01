#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>

int main() {
    std::ifstream file("/home/hakan/Desktop/Sega GT 2002 (Europe) (En,Ja,Fr,De,Es,It)/default.xbe", std::ios::binary);
    file.seekg(0x164, std::ios::beg); // Wait, XBE.cpp checked 0x164? No, xbe.cpp says 0x150-0x180
    // Let me just run xbe-inspect and grep!
    return 0;
}
