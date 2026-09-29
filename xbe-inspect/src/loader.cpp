#include "loader.hpp"
#include "kernel.hpp"
#include <iostream>
#include <fstream>
#include <sys/mman.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>
#include <signal.h>
#include <ucontext.h>

namespace xbe {

static void print_str(const char* str) {
    size_t len = 0;
    while (str[len]) len++;
    write(STDERR_FILENO, str, len);
}

static void print_hex(uintptr_t val) {
    char buf[32];
    buf[0] = '0'; buf[1] = 'x';
    int pos = 2;
    for (int i = (sizeof(val) * 2) - 1; i >= 0; --i) {
        int nibble = (val >> (i * 4)) & 0xF;
        buf[pos++] = nibble < 10 ? '0' + nibble : 'A' + (nibble - 10);
    }
    buf[pos++] = '\n';
    write(STDERR_FILENO, buf, pos);
}

static void sigsegv_handler(int sig, siginfo_t* info, void* ucontext) {
    (void)sig; // unused
    ucontext_t* context = reinterpret_cast<ucontext_t*>(ucontext);
    
    // i386 (32-bit) ucontext_t structure
    uintptr_t eip = context->uc_mcontext.gregs[REG_EIP];
    uintptr_t fault_addr = reinterpret_cast<uintptr_t>(info->si_addr);

    print_str("\n\033[1;31m[CRASH]\033[0m SIGSEGV yakalandi!\n");
    print_str("Crash Adresi (Fault): ");
    print_hex(fault_addr);
    print_str("Instruction Pointer (EIP): ");
    print_hex(eip);
    print_str("(Muhtemelen baglanmamis bir Kernel Ordinal cagirildi veya invalid bellek erisimi)\n");

    _exit(1);
}

bool load_and_run(const ParseResult& res, const std::string& filepath) {
    if (!res.valid) return false;

    // Signal handler kurulumu
    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = sigsegv_handler;
    sigaction(SIGSEGV, &sa, nullptr);

    // Sanal bellek tahsisi icin max VA'i bulalim
    uint32_t min_va = res.base_address;
    uint32_t max_va = min_va;
    for (const auto& sec : res.sections) {
        if (sec.virtual_address + sec.virtual_size > max_va) {
            max_va = sec.virtual_address + sec.virtual_size;
        }
    }
    
    size_t total_size = max_va - min_va;
    if (total_size == 0) total_size = 0x1000000; // Eger section yoksa fallback (16MB)

    // Base address'i page sınırına hizala
    uintptr_t base_addr = res.base_address;
    size_t page_size = sysconf(_SC_PAGESIZE);
    uintptr_t aligned_base = base_addr & ~(page_size - 1);
    size_t offset = base_addr - aligned_base;
    size_t alloc_size = ((total_size + offset + page_size - 1) & ~(page_size - 1));

    // Belirtilen (aligned) base adresten bellek ayir
    void* mem = mmap(reinterpret_cast<void*>(aligned_base), alloc_size, 
                     PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);

    uintptr_t delta = 0;
    if (mem == MAP_FAILED) {
        // MAP_FIXED basarisiz olursa isletim sistemine birak
        mem = mmap(nullptr, alloc_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (mem == MAP_FAILED) {
            std::cerr << "[LOADER] mmap basarisiz! Hata: " << strerror(errno) << "\n";
            return false;
        }
        delta = reinterpret_cast<uintptr_t>(mem) - aligned_base;
        std::cout << "[LOADER] MAP_FIXED basarisiz, isletim sistemi 0x" << std::hex << reinterpret_cast<uintptr_t>(mem) 
                  << " adresine atadi. Delta offset hesaplandi: 0x" << delta << std::dec << "\n";
    } else {
        std::cout << "[LOADER] Bellek 0x" << std::hex << aligned_base << " (Aligned Base) adresine basariyla allocate edildi.\n" << std::dec;
    }

    std::ifstream file(filepath, std::ios::binary);
    if (!file) {
        std::cerr << "[LOADER] Dosya okunamadi: " << filepath << "\n";
        return false;
    }

    // XBE oyunlari Base Address (0x10000) alaninda XBE Header'in oldugunu varsayar.
    // Ilk olarak header'i (genelde sizeof_headers, ama kabaca 4096 byte yeterli) 0x10000'a kopyalayalim.
    // parse result'ta sizeof_headers yok ama 0x1000 byte garanti yapiyor.
    file.clear();
    file.seekg(0, std::ios::beg);
    uint8_t* header_dest = reinterpret_cast<uint8_t*>(aligned_base + delta);
    if (!file.read(reinterpret_cast<char*>(header_dest), 0x1000)) {
        std::cerr << "[LOADER] UYARI: XBE Header tam okunamadi!\n";
    } else {
        std::cout << "[LOADER] XBE Header 0x" << std::hex << (aligned_base + delta) << " adresine yuklendi.\n" << std::dec;
    }

    // Section'lari yukle ve yetkilendir
    for (const auto& sec : res.sections) {
        if (sec.raw_size == 0) continue;
        
        file.clear(); // Reset EOF/Fail bits
        file.seekg(sec.raw_address, std::ios::beg);
        uint8_t* dest = reinterpret_cast<uint8_t*>(sec.virtual_address + delta);
        
        // Memcpy yerine dogrudan okuma
        if (!file.read(reinterpret_cast<char*>(dest), sec.raw_size)) {
            std::cerr << "[LOADER] UYARI: " << sec.name << " bolumu tam okunamadi!\n";
        }
        
        // Xbox oyunlari memory'de sıkca self-modifying code yapar veya thunk tablolari
        // rdata icinde degistirilir. En guvenli yontem hepsi icin RWX vermektir.
        int prot = PROT_READ | PROT_WRITE | PROT_EXEC;
        
        // mprotect icin sayfa (page) hizalamasi yapalim
        uintptr_t dest_addr = reinterpret_cast<uintptr_t>(dest);
        uintptr_t aligned_addr = dest_addr & ~(page_size - 1);
        size_t aligned_size = ((dest_addr + sec.virtual_size + page_size - 1) & ~(page_size - 1)) - aligned_addr;
        
        if (mprotect(reinterpret_cast<void*>(aligned_addr), aligned_size, prot) != 0) {
            std::cerr << "[LOADER] mprotect basarisiz (" << sec.name << ")! Hata: " << strerror(errno) << "\n";
        } else {
            std::cout << "[LOADER] " << sec.name << " bolumu yuklendi ve yetkilendirildi. (Virtual Address: 0x" 
                      << std::hex << dest_addr << ")\n" << std::dec;
        }
    }

    // Resolve Kernel Imports
    xbe::resolve_kernel_imports(res, delta);

    // Entry Point calistirma
    uintptr_t ep_addr = res.entry_point_retail + delta;
    
    std::cout << "[LOADER] Entry Point (0x" << std::hex << ep_addr << std::dec << ") cagiriliyor...\n";
    
    typedef int (*XboxEntryPointFunc)();
    XboxEntryPointFunc ep_func = reinterpret_cast<XboxEntryPointFunc>(ep_addr);
    
    int ret_val = ep_func();
    
    std::cout << "\n\033[1;32m[BASARILI]\033[0m Entry Point calistirildi! Donus degeri (EAX): " << ret_val << "\n";
    
    return true;
}

} // namespace xbe
