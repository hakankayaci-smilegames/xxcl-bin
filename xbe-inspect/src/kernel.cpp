#include "kernel.hpp"
#include "kernel/exports.hpp"
#include <iostream>
#include <sys/mman.h>
#include <cstring>
#include <cerrno>
#include <chrono>
#include <cstdlib>

namespace xbe {

static std::chrono::time_point<std::chrono::high_resolution_clock> g_start_time;
static uint64_t g_kernel_call_count = 0;
static uint32_t g_last_ordinal = 0;

void start_execution_timer() {
    g_start_time = std::chrono::high_resolution_clock::now();
    g_kernel_call_count = 0;
    g_last_ordinal = 0;
}

void record_kernel_call(uint32_t ordinal) {
    g_kernel_call_count++;
    g_last_ordinal = ordinal;
}

void print_execution_stats() {
    auto now = std::chrono::high_resolution_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - g_start_time).count();
    
    std::cout << "\n\033[1;36m[DEBUG INFO] --- YURUTME ISTATISTIKLERI ---\033[0m\n";
    std::cout << "Gecen Sure             : " << duration_ms << " ms\n";
    std::cout << "Yapilan Kernel Cagrisi : " << std::dec << g_kernel_call_count << " adet\n";
    std::cout << "Son Cagrilan Ordinal   : " << (g_last_ordinal ? std::to_string(g_last_ordinal) : "Yok") << "\n";
    std::cout << "\033[1;36m-------------------------------------------\033[0m\n";
}

void unresolved_stub(int ordinal) {
    uint32_t* esp_ptr;
    asm volatile("mov %%ebp, %0" : "=r"(esp_ptr)); 
    // Wait, unresolved_stub uses EBP for its frame.
    // So esp_ptr (EBP) points to saved EBP.
    // [EBP+0] = saved EBP
    // [EBP+4] = return address (to trampoline)
    // [EBP+8] = ordinal
    // The trampoline did: pushad (32 bytes). Before pushad, return address to game was on stack.
    // So the return address to the game is at EBP + 8 (ordinal) + 32 (pushad) + 4 (return address from call unresolved_stub)
    
    // Let's just use __builtin_return_address(0) which is trampoline, and __builtin_frame_address(1)? No.
    // Let's do it safely:
    uint32_t ebp_val;
    asm volatile("mov %%ebp, %0" : "=r"(ebp_val));
    uint32_t* ebp_ptr = (uint32_t*)ebp_val;
    
    // ebp_ptr[0] = saved EBP
    // ebp_ptr[1] = return address to trampoline
    // ebp_ptr[2] = ordinal
    // Actually, trampoline doesn't set up EBP.
    
    record_kernel_call(ordinal);
    std::cout << "\n\033[1;31m[UNIMPLEMENTED ORDINAL]\033[0m Ordinal " << std::dec << ordinal << " cagrildi!\n";
    
    // To find the return address to the game, let's just dump the stack upwards from EBP
    uint32_t ret_addr; asm volatile("mov 4(%%ebp), %0" : "=r"(ret_addr)); std::cout << "\n[UNRESOLVED_STUB] Return Address: 0x" << std::hex << ret_addr << "\n[STACK DUMP FROM UNRESOLVED_STUB]:\n";
    for(int i = 0; i < 20; i++) {
        std::cout << "EBP+" << (i*4) << ": 0x" << std::hex << ebp_ptr[i] << "\n";
    }
    
    print_execution_stats();
    std::cout << "Yurutme desteklenmeyen ordinal yuzunden guvenli sekilde durduruldu.\n";
    exit(1);
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
    int resolved_count = 0;
    
    for (size_t i = 0; i < res.kernel_imports.size(); ++i) {
        uint32_t ordinal = res.kernel_imports[i];
        
        // Eger ordinal moduler kernel implementasyonunda varsa dogrudan function pointer ver
        void* handler = kernel::get_kernel_export(ordinal);
        if (handler) {
            thunk_table[i] = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(handler));
            std::cout << "[KERNEL HOOK MAP] Ordinal " << std::dec << ordinal << " -> NATIVE (__stdcall) baglandi.\n";
            resolved_count++;
        } else {
            std::cout << "[KERNEL HOOK MAP] Ordinal " << std::dec << ordinal << " -> TRAMPOLINE (Desteklenmiyor) baglandi.\n";
            // Desteklenmeyen (unresolved) ordinaller icin coken (veya en azindan loglayan) trampoline
            uint8_t* tramp = trampolines + (i * 32);
            
            int p = 0;
            // pushad (save all registers)
            tramp[p++] = 0x60;
            
            // push ordinal (cdecl arg 1)
            tramp[p++] = 0x68;
            *reinterpret_cast<uint32_t*>(&tramp[p]) = ordinal;
            p += 4;
            
            // mov eax, &unresolved_stub
            tramp[p++] = 0xB8;
            *reinterpret_cast<uint32_t*>(&tramp[p]) = reinterpret_cast<uint32_t>(&unresolved_stub);
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
            
            // ret (Dikkat: Sadece debug amacli donus, orjinal cagri parametreliyse stack kirilir)
            tramp[p++] = 0xC3;

            thunk_table[i] = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(tramp));
        }
        hooked_count++;
    }
    
    std::cout << "[KERNEL] " << std::dec << hooked_count << " adet kernel fonksiyonu kancalandi. (" 
              << resolved_count << " tanesi native hook).\n";
}

} // namespace xbe
