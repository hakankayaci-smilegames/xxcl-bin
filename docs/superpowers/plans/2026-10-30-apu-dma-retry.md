# APU Pending DMA Retry Implementation Plan

**Goal:** Resume a real DSP DMA request once its guest scratch mapping becomes usable, without inventing a completion.

**Evidence:** Without verbose tracing, GDB observes an active DMA stopped at node `0x1E`, EOL clear, DSP `PC=0x121`, and the now-correct scratch limit `0x7D`. The same node completes under the slower traced run, indicating the request can arrive before its mapping update is processed.

**Approach:** Retain a failed active DMA as pending. Retry it at most once per DSP execution batch, using the existing descriptor and memory validation, and assert EOL only after actual completion. A stop, abort, or DSP bootstrap cancels pending work. The worker's existing execution schedule supplies retries, including when guest memory changes without an MMIO register write.

**Verification:** Extend the device-level DMA fixture to demonstrate failure before the limit update, completion after the update without a DSP reset, and resumption after a mapping table is populated or replaced. Verify cancellation and all existing tests, then repeat an isolated Sega GT launch and inspect the guest's actual waiting point.

**Files:** `xbe-inspect/src/kernel/apu_dsp.cpp`, `xbe-inspect/src/kernel/apu_dsp.hpp`, `xbe-inspect/src/kernel/apu.cpp`, `xbe-inspect/tests/apu_contract_tests.cpp`, and current status documentation.

**Result:** Device-level tests cover delayed limit, replaced table, late SGE entry, and STOP. An isolated two-launch runtime and GDB inspection show the formerly pending DMA at `next=0x401E` with EOL set and no pending request. The guest is still at `0x00162E2A`; no guest-wait or rendering success is claimed.
