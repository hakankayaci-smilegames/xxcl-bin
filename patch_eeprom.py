import re

with open('xbe-inspect/src/kernel/ex.cpp', 'r') as f:
    content = f.read()

new_func = """    NTSTATUS __attribute__((__stdcall__)) xbox_ExQueryNonVolatileSetting(
        uint32_t ValueIndex,
        uint32_t* Type,
        uint8_t* Value,
        uint32_t ValueLength,
        uint32_t* ResultLength
    ) {
        printf("[KERNEL] ExQueryNonVolatileSetting(Index: %u, Length: %u) called.\\n", ValueIndex, ValueLength);
        
        if (ResultLength) *ResultLength = 4;
        if (Type) *Type = 4; // REG_DWORD by default
        
        uint32_t dummy_val = 0;
        
        switch(ValueIndex) {
            case 7: // XC_LANGUAGE
                dummy_val = 1; // English
                break;
            case 8: // XC_VIDEO
                dummy_val = 0; // standard
                break;
            case 9: // XC_AUDIO
                dummy_val = 0; // Stereo
                break;
            case 10: // XC_P_CONTROL_GAMES
                dummy_val = 0; // All ages
                break;
            case 17: // XC_MISC (0x11)
                dummy_val = 0;
                break;
            case 259: // XC_FACTORY_AV_REGION (0x103)
                dummy_val = 2; // PAL, or 1 for NTSC
                break;
            case 260: // XC_FACTORY_GAME_REGION (0x104)
                dummy_val = 4; // Europe/Rest of World. 1=NA, 2=Japan, 4=RoW
                break;
            default:
                dummy_val = 0;
                break;
        }
        
        if (Value && ValueLength >= 4) {
            *(uint32_t*)Value = dummy_val;
        } else if (Value && ValueLength > 0) {
            memset(Value, 0, ValueLength);
        }
        return 0;
    }
"""

# Replace the old function
content = re.sub(
    r"    NTSTATUS __attribute__\(\(__stdcall__\)\) xbox_ExQueryNonVolatileSetting\(.*?return 0;\n    \}",
    new_func,
    content,
    flags=re.DOTALL
)

with open('xbe-inspect/src/kernel/ex.cpp', 'w') as f:
    f.write(content)
