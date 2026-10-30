# DSP MOVEC Modifier Register Implementation Plan

**Goal:** Load a DSP address modifier register from an immediate word.

**Evidence:** The next runtime instruction is `0x05F420 0xFFFFFF`, the DSP56300 `MOVEC #$FFFFFF,M0` form. The extension word must be truncated to the 16-bit modifier-register width.

**Approach:** Implement the immediate MOVEC form for M0–M7 only. Leave other MOVEC forms and unrelated addressing modes unsupported until implemented; advance by two words only for valid instructions.

**Verification:** Use literal DSP programs to check M0 and M7 values, 16-bit truncation, program-counter advancement and unchanged condition flags. Run the complete test suite and an isolated two-launch game runtime.

**Reference:** https://www.nxp.com/docs/en/reference-manual/DSP56300FM.pdf

**Result:** Completed. The test failed with exit code 44 before implementation and passed afterward; the full build and 8/8 CTest targets pass. An isolated two-launch runtime passes the previous `PC=0x070` instruction and reaches unsupported `PC=0x072`, `P=0x0461A0`. A follow-up diagnostic trace confirms `stopped result=1`. Guest audio-wait completion and rendering remain unproven; the next instruction is left for the next task.
