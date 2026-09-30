#include "xbe_parser.hpp"
#include <iostream>
int main() {
    xbe::XbeParser parser("/home/hakan/Desktop/Sega GT 2002 (Europe) (En,Ja,Fr,De,Es,It)/default.xbe");
    xbe::ParseResult res = parser.parse();
    std::cout << "Kernel Thunks: 0x" << std::hex << res.kernel_thunk_address_retail << "\n";
    std::cout << "Kernel Thunks End: 0x" << std::hex << res.kernel_thunk_address_retail + res.kernel_imports.size() * 4 << "\n";
}
