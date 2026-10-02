#include "ps.hpp"
#include <cstdio>
#include <vector>
#include <sys/syscall.h>
#include <asm/ldt.h>
#include <unistd.h>
#include <cstring>
#include "kpcr.hpp"

static std::vector<pthread_t> g_xbox_threads;

extern "C" {
    NTSTATUS __attribute__((__stdcall__)) xbox_PsCreateSystemThreadEx(
        uint32_t* ThreadHandle,
        uint32_t arg2,
        uint32_t arg3,
        uint32_t arg4,
        uint32_t arg5,
        uint32_t arg6,
        uint32_t arg7,
        uint32_t arg8,
        uint32_t arg9,
        uint32_t arg10
    ) {
        printf("[KERNEL] PsCreateSystemThreadEx cagrildi.\n");
        printf("  ThreadHandle : %p\n", ThreadHandle);
        printf("  StartRoutine : 0x%08X\n", arg6);
        printf("  StartContext : 0x%08X\n", arg7);
        
        if (ThreadHandle) {
            *ThreadHandle = 0x8899AABB;
        }

        typedef void (__attribute__((__stdcall__)) *ThreadFunc)(uint32_t);
        ThreadFunc routine = (ThreadFunc)arg6;
        uint32_t context = arg7;

        pthread_t pt;
        pthread_create(&pt, nullptr, [](void* arg) -> void* {
            auto args = (std::pair<ThreadFunc, uint32_t>*)arg;
            printf("[KERNEL] Yeni Xbox Thread'i basliyor: %p\n", args->first);
            
            // Allocate thread-specific KPCR and Stack
            xbe::kernel::XBOX_KPCR* thread_kpcr = new xbe::kernel::XBOX_KPCR();
            memset(thread_kpcr, 0, sizeof(xbe::kernel::XBOX_KPCR));
            uint8_t* thread_stack = new uint8_t[0x20000]; // 128KB stack
            
            // We need a dummy thread object too
            uint8_t* dummy_thread = new uint8_t[0x2000];
            memset(dummy_thread, 0, 0x2000);
            
            thread_kpcr->Self = thread_kpcr;
            thread_kpcr->StackBase = thread_stack + 0x20000;
            thread_kpcr->StackLimit = thread_stack;
            thread_kpcr->Irql = 0;
            thread_kpcr->CurrentThreadPtr = dummy_thread;
            thread_kpcr->CurrentThread = dummy_thread;

            // Mock TLS array at top of stack
            // Xbox Tls array is essentially pointers. Let's make a dummy one.
            void** tls_array = new void*[64];
            for(int i=0; i<64; i++) {
                tls_array[i] = new uint8_t[0x1000]; // Dummy TLS data slot
            }
            
            // Sega GT accesses Tls at StackBase - 20 (index -5)
            // Let's just make the entire top of the stack act as the TLS pointer array!
            void** stack_top_as_tls = (void**)(thread_kpcr->StackBase);
            for(int i=-16; i<0; i++) {
                stack_top_as_tls[i] = new uint8_t[0x1000];
            }

            // Set up Linux FS segment
            struct user_desc u_info;
            memset(&u_info, 0, sizeof(u_info));
            u_info.entry_number = -1;
            u_info.base_addr = (unsigned long)thread_kpcr;
            u_info.limit = 0xFFFFF;
            u_info.seg_32bit = 1;
            u_info.contents = 0;
            u_info.read_exec_only = 0;
            u_info.limit_in_pages = 1;
            u_info.seg_not_present = 0;
            u_info.useable = 1;

            if (syscall(SYS_set_thread_area, &u_info) == 0) {
                unsigned int fs_selector = (u_info.entry_number << 3) | 3;
                asm volatile("movw %w0, %%fs" : : "r"(fs_selector));
            }

            args->first(args->second);
            
            printf("[KERNEL] Xbox Thread'i sonlandi.\n");
            delete args;
            return nullptr;
        }, new std::pair<ThreadFunc, uint32_t>(routine, context));

        g_xbox_threads.push_back(pt);

        return 0; // STATUS_SUCCESS
    }
}

extern "C" void wait_for_xbox_threads() {
    for (auto pt : g_xbox_threads) {
        pthread_join(pt, nullptr);
    }
}
