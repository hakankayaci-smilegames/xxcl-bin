# NV2A First Visible Render Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Process an actual guest NV2A pushbuffer command and show its color-clear result in the SDL window.

**Architecture:** Keep NV2A register behavior, bounded command decoding, and framebuffer updates in small testable units. The worker consumes only validated commands and publishes pixel data; SDL remains owned by the main thread. Use guest physical-memory translation rather than title-specific addresses.

**Tech Stack:** C++20, CMake/CTest, existing physical-memory mapper, SDL2, isolated XXCL HDD runs.

**Spec:** `docs/superpowers/specs/2026-10-20-nv2a-first-visible-render-design.md`

## Global Constraints

- Do not hard-code the observed guest PUT value or title-specific behavior.
- Do not advance GET until the associated command has been successfully consumed.
- Reject unaligned, out-of-range, and unreadable command data without pretending it was consumed.
- Keep SDL calls on the video/main thread; keep the diagnostic overlay visible.
- Do not read or modify the repository `hdd/` tree.
- Do not expand this milestone into general 3D, audio, input, or cycle-accurate emulation.
- Each verified source slice gets its own commit and push; never stage existing build outputs or unrelated debug files.

## Review Focus

- MMIO reads and writes through both USER and PFIFO register addresses must see consistent channel state; cover a read-after-write through the alternate address.
- An invalid channel/mode or unaligned/out-of-range guest address must not crash or be reported as consumed; cover invalid and boundary inputs in the owning parser tests.
- A malformed/truncated pushbuffer packet must not advance GET; cover truncated header and payload fixtures.
- A command which is valid but not yet supported must leave GET unchanged and report a useful status; cover the unsupported-method path.
- A clear update from the worker must not call SDL off-thread or hide the existing overlay; cover framebuffer pixel publication independently and confirm in the runtime run.

---

### Task 1: Make NV2A command/state logic independently testable

**Files:**
- Create: `xbe-inspect/src/kernel/nv2a_pfifo.hpp`
- Test: `xbe-inspect/tests/nv2a_contract_tests.cpp`
- Modify: `xbe-inspect/CMakeLists.txt`

**Interfaces:**
- Produces: a small `Nv2aPfifoState` interface for active channel, DMA mode, GET, and PUT, used by later register and worker code.

- [ ] Add tests showing a PUT written through one supported register view is returned through the corresponding alternate view, and that channel/mode selection keeps unrelated state separate.
- [ ] Run `cmake --build xbe-inspect/build --target nv2a_contract_tests -j"$(nproc)" && ctest --test-dir xbe-inspect/build -R nv2a_contract_tests --output-on-failure`; confirm the new behavior fails before implementation.
- [ ] Implement only the state behavior required by those tests, without guessing register aliasing rules beyond the observed active-channel contract.
- [ ] Re-run the targeted test and full CTest suite, then commit and push this source slice.

### Task 2: Decode bounded PFIFO DMA packets

**Files:**
- Create: `xbe-inspect/src/kernel/nv2a_pfifo.cpp`
- Test: `xbe-inspect/tests/nv2a_contract_tests.cpp`
- Modify: `xbe-inspect/CMakeLists.txt`
- Modify: `xbe-inspect/src/kernel/nv2a.hpp`

**Interfaces:**
- Consumes: `Nv2aPfifoState` from Task 1 and `physical_memory_to_host(uint32_t, size_t)`.
- Produces: a packet decode result with command words, next GET, and explicit invalid/unsupported outcomes; no mutation of GET during decoding.

- [ ] Add literal-fixture tests for a valid packet, truncated payload, unaligned address, unmapped physical range, and unsupported method.
- [ ] Run the targeted test and confirm the invalid cases fail for missing decoder behavior.
- [ ] Implement a bounds-checked decoder and connect the test target to the real physical-memory mapper where needed.
- [ ] Re-run targeted and full CTest suites; commit and push this independently testable slice.

### Task 3: Consume commands only after successful processing

**Files:**
- Modify: `xbe-inspect/src/kernel/nv2a.cpp`
- Modify: `xbe-inspect/src/kernel/nv2a.hpp`
- Test: `xbe-inspect/tests/nv2a_contract_tests.cpp`
- Modify: `xbe-inspect/CMakeLists.txt`

**Interfaces:**
- Consumes: validated packet result from Task 2.
- Produces: worker processing that advances GET by exactly the consumed packet length and leaves it unchanged on decode/dispatch failure.

- [ ] Add tests proving GET advances after a supported command and remains unchanged after malformed or unsupported commands.
- [ ] Verify the tests fail before adding command dispatch.
- [ ] Implement the smallest useful command dispatch, initially recording a clear operation from an actual trace without title-specific branching.
- [ ] Build, run targeted and full tests, then commit and push.

### Task 4: Publish clear output to SDL and reach the runtime milestone

**Files:**
- Modify: `xbe-inspect/src/video/window.cpp`
- Modify: `xbe-inspect/src/video/window.hpp`
- Modify: `xbe-inspect/src/kernel/nv2a.cpp`
- Test: `xbe-inspect/tests/nv2a_contract_tests.cpp`
- Modify: `README.md` and the relevant architecture/runtime documentation identified during implementation.

**Interfaces:**
- Consumes: worker clear operation from Task 3.
- Produces: a thread-safe pixel/clear result that the SDL main loop displays behind the existing overlay.

- [ ] Add a test for the clear result's visible pixel/color output and for preserving the result until the video loop consumes it.
- [ ] Verify the test fails before the video handoff is implemented.
- [ ] Implement the minimal SDL-side presentation path; no SDL calls from the worker.
- [ ] Build and run all CTest tests. Run the XBE twice against a fresh temporary HDD root, confirming the reboot-on-first-run sequence, the `0x001A3238` wait is passed, a real command is consumed, and the clear result appears with the overlay still visible.
- [ ] Update the runtime/architecture docs and `xxcl` command only if the actual supported invocation changed; commit and push each verified source/doc slice separately.

