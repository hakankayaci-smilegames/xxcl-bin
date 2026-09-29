#include "kernel.hpp"
#include <iostream>
#include <sys/mman.h>
#include <cstring>
#include <cerrno>

namespace xbe {

void generic_stub(int ordinal) {
    std::cout << "[KERNEL HOOK] Cagrilan Ordinal: " << std::dec << ordinal << std::endl;
}

static uint8_t* trampolines = nullptr;

void resolve_kernel_imports(const ParseResult& res, uintptr_t delta) {
    if (res.kernel_imports.empty()) return;

    if (!trampolines) {
        // 32-bit ortamda her mmap 32-bit alanina duser, MAP_32BIT tanimsiz veya gereksizdir.
        trampolines = (uint8_t*)mmap(nullptr, 4096, PROT_READ | PROT_WRITE | PROT_EXEC, 
                                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (trampolines == MAP_FAILED) {
            std::cerr << "[KERNEL] Trampoline alani ayrilamadi! Hata: " << strerror(errno) << "\n";
            return;
        }
    }

    uint32_t* thunk_table = reinterpret_cast<uint32_t*>(res.kernel_thunk_address_retail + delta);
    int hooked_count = 0;
    
    for (size_t i = 0; i < res.kernel_imports.size(); ++i) {
        uint32_t ordinal = res.kernel_imports[i];
        
        // Generate 32-bit x86 trampoline
        uint8_t* tramp = trampolines + (i * 32);
        
        int p = 0;
        // pushad (save all registers)
        tramp[p++] = 0x60;
        
        // push ordinal (cdecl arg 1)
        tramp[p++] = 0x68;
        *reinterpret_cast<uint32_t*>(&tramp[p]) = ordinal;
        p += 4;
        
        // mov eax, &generic_stub
        tramp[p++] = 0xB8;
        *reinterpret_cast<uint32_t*>(&tramp[p]) = reinterpret_cast<uint32_t>(&generic_stub);
        p += 4;
        
        // call eax
        tramp[p++] = 0xFF;
        tramp[p++] = 0xD0;
        
        // add esp, 4 (clean up our pushed ordinal)
        tramp[p++] = 0x83;
        tramp[p++] = 0xC4;
        tramp[p++] = 0x04;
        
        // popad (restore all registers)
        tramp[p++] = 0x61;
        
        // ret (Warning: Xbox uses stdcall, this might leave stack dirty if original func had args)
        tramp[p++] = 0xC3;

        // Overwrite thunk entry
        thunk_table[i] = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(tramp));
        hooked_count++;
    }
    
    std::cout << "[KERNEL] " << std::dec << hooked_count << " adet kernel fonksiyonu kancalandi (Hooked).\n";
}

} // namespace xbe
