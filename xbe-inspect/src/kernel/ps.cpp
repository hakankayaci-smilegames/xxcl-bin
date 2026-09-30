#include "ps.hpp"
#include <cstdio>
#include <vector>

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
            
            // KPCR for this thread! (We need a KPCR for each thread in the future)
            // But for now just call it!
            
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
