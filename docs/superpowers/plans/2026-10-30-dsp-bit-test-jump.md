# DSP Direct-Memory Bit-Test Jump Plan

**Goal:** Execute the DSP56300 direct-memory bit-test jumps needed by the running GP firmware.

**Evidence:** After correcting CMP, the guest reaches `P[0x066]=0x0A0480`, followed by target `0x0084`: `JCLR #0,X:$04,P:$0084`. Its source is DSP data memory, not the peripheral register space already handled by the emulator. See the DSP56300 family manual, JCLR/JSET instruction formats: https://www.nxp.com/docs/en/reference-manual/DSP56300FM.pdf

**Approach:** Implement the absolute six-bit X/Y data-memory forms of JCLR and JSET, consuming the extension word in either branch and leaving condition flags unchanged. Reject invalid bit numbers and truncated instructions without advancing the program counter.

**Verification:** Test taken and untaken jumps, X and Y memory selection, bit numbers above 15 and the invalid range, and preserved state using DSP programs. Run the full suite and a clean isolated game launch; report its next actual checkpoint.

**Files:** `xbe-inspect/src/kernel/apu_dsp.cpp`, `xbe-inspect/tests/apu_dsp_contract_tests.cpp`, and current runtime documentation.

**Result:** Direct-memory JCLR/JSET behavior passes the requested positive and negative cases and the full suite. In a clean two-launch game run, DSP execution advances past `PC=0x066` to unsupported `PC=0x070`, `P=0x05F420`.
