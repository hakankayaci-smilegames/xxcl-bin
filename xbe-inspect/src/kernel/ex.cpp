#include "ex.hpp"
#include <cstdio>
#include <cstring>

extern "C" {
    NTSTATUS __attribute__((__stdcall__)) xbox_ExQueryNonVolatileSetting(
        uint32_t ValueIndex,
        uint32_t* Type,
        uint8_t* Value,
        uint32_t ValueLength,
        uint32_t* ResultLength
    ) {
        printf("[KERNEL] ExQueryNonVolatileSetting(Index: %u, Length: %u) cagrildi.\n", ValueIndex, ValueLength);
        
        // Mocking EEPROM/Refurb info reading.
        // Return STATUS_SUCCESS (0).
        if (ResultLength) {
            *ResultLength = 0;
        }
        if (Type) {
            *Type = 4; // REG_DWORD or something?
        }
        if (Value && ValueLength > 0) {
            memset(Value, 0, ValueLength); // Zero out the buffer
        }
        
        return 0; // STATUS_SUCCESS
    }
}
