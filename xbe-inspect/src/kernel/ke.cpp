#include "ke.hpp"
#include <cstdio>

// Xbox/NT kernel DISPATCHER_HEADER mock
struct XBOX_DISPATCHER_HEADER {
    uint8_t Type;
    uint8_t Absolute;
    uint8_t Size;
    uint8_t Inserted;
    int32_t SignalState;
    // LIST_ENTRY WaitListHead...
};

extern "C" {
    int32_t __attribute__((__stdcall__)) xbox_KeSetEvent(void* Event, int32_t Increment, uint8_t Wait) {
        printf("[KERNEL] KeSetEvent cagrildi. Event: %p\n", Event);
        
        if (Event) {
            auto* header = reinterpret_cast<XBOX_DISPATCHER_HEADER*>(Event);
            int32_t previous_state = header->SignalState;
            header->SignalState = 1; // Event signaled
            return previous_state;
        }
        
        return 0;
    }
}
