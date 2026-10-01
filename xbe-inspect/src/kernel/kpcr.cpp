#include "kpcr.hpp"
#include <sys/syscall.h>
#include <unistd.h>
#include <asm/ldt.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>

namespace xbe {
namespace kernel {

static XBOX_KPCR g_kpcr;
static uint8_t g_dummy_thread[0x2000]; // Sahte KTHREAD nesnesi

extern "C" void __attribute__((__stdcall__)) dummy_func_2args(uint32_t a, uint32_t b) {
    printf("[KERNEL] dummy_func_2args(%u, %u) cagrildi.\n", a, b);
}
static void dummy_func() {
    // Stub
}

void init_xbox_kpcr() {
    memset(&g_kpcr, 0, sizeof(XBOX_KPCR));
    memset(g_dummy_thread, 0, sizeof(g_dummy_thread));

    uint32_t* thread_dwords = reinterpret_cast<uint32_t*>(g_dummy_thread);
    uint32_t safe_ptr = reinterpret_cast<uint32_t>(g_dummy_thread + 0x1000);
    for (int i = 0; i < 256; i++) {
        thread_dwords[i] = safe_ptr;
    }
    
    // Sega GT: check at 0x104C3E requires CurrentThread+0x28 to be 0
    thread_dwords[0x28 / 4] = 0;

    // Remove the fake_object hack, we want CurrentThread+0x12C to be 0 so it uses the native path
    thread_dwords[0x12C / 4] = 0;
    thread_dwords[0x250 / 4] = safe_ptr;
    reinterpret_cast<uint32_t*>(safe_ptr)[0x14 / 4] = reinterpret_cast<uint32_t>(&dummy_func_2args);

    static uint8_t g_dummy_stack[0x10000];

    g_kpcr.Self = &g_kpcr;
    g_kpcr.StackBase = g_dummy_stack + sizeof(g_dummy_stack);
    g_kpcr.StackLimit = g_dummy_stack;
    g_kpcr.Irql = 0; // PASSIVE_LEVEL (cmp al, 2 kontrolünü rahatça geçsin)
    g_kpcr.CurrentThreadPtr = g_dummy_thread;
    g_kpcr.CurrentThread = g_dummy_thread;

    struct user_desc u_info;
    memset(&u_info, 0, sizeof(u_info));
    u_info.entry_number = -1; // Linux uygun bir GDT/LDT slotu seçsin
    u_info.base_addr = (unsigned long)&g_kpcr;
    u_info.limit = 0xFFFFF; // 4GB veya yeterli alan
    u_info.seg_32bit = 1;
    u_info.contents = 0;    // Veri segmenti
    u_info.read_exec_only = 0;
    u_info.limit_in_pages = 1;
    u_info.seg_not_present = 0;
    u_info.useable = 1;

    int res = syscall(SYS_set_thread_area, &u_info);
    if (res != 0) {
        perror("[KPCR ERROR] set_thread_area basarisiz");
        exit(1);
    }

    // u_info.entry_number Linux tarafından güncellenir.
    // Segment selector hesabı: (entry_number * 8) | 3 (RPL=3 user mode)
    unsigned int fs_selector = (u_info.entry_number << 3) | 3;
    asm volatile("movw %w0, %%fs" : : "r"(fs_selector));

    printf("[KPCR] Fake KPCR basariyla kuruldu. Base: %p, FS Selector: 0x%X\n", &g_kpcr, fs_selector);
}

} // namespace kernel
} // namespace xbe
