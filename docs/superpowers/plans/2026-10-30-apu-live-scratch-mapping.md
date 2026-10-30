# APU Live Scratch Mapping Implementation Plan

**Goal:** Let a running GP DSP use the current guest-configured scratch mapping for its next DMA transfer.

**Evidence:** The guest's maximum SGE register reaches `0x7d` while the DSP retains its bootstrap value `8`. A DMA request targeting scratch offset `0xb000` fails its range check and the guest waits for completion.

**Approach:** Propagate each scratch-table address and maximum-index update to the DSP without reloading or resetting its program. Preserve the existing bounds and transfer validation, and do not report failed DMA as complete. The first-render plan remains the subsequent goal.

**Verification:** Add a device-level test which bootstraps with two pages, increases the live maximum index, and checks that an actual DSP DMA writes a third mapped page. Also check that the same transfer is rejected before the update. Run all tests and an isolated two-launch Sega GT runtime trace; document whether the observed wait is passed.

**Files:** `xbe-inspect/src/kernel/apu.cpp`, `xbe-inspect/src/kernel/apu_dsp.cpp`, `xbe-inspect/src/kernel/apu_dsp.hpp`, `xbe-inspect/tests/apu_contract_tests.cpp`, and current status documentation as warranted by the runtime evidence.

**Result:** The live limit and table base are covered by device-level DMA tests. On a fresh two-launch runtime, the formerly failed scratch DMA now completes and both GP and DSP report limit `0x7D`. The guest still polls at `0x00162E2A`; this change does not claim to resolve that separate wait.
