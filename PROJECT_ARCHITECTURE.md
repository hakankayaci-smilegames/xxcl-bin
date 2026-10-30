# XXCL Project Architecture

This document describes the current module boundaries and the compatibility approach. Xbox kernel export signatures and calling conventions are part of the external ABI and should remain stable while internal implementations are reorganized.

## Source layout

```text
xbe-inspect/
├── src/
│   ├── loader.cpp                 # Maps XBE sections and resolves kernel imports
│   ├── main.cpp                   # CLI, including isolated --hdd-root selection
│   ├── kernel/
│   │   ├── exports.cpp            # Ordinal-to-implementation registry
│   │   ├── vfs.cpp/.hpp           # Xbox path, symbolic-link, drive and host-root mapping
│   │   ├── io.cpp/.hpp            # File handles and Nt* file/volume operations
│   │   ├── device_io.cpp/.hpp     # Device-control dispatch and device backends
│   │   ├── handles.cpp/.hpp       # Shared file/thread kernel handle ownership
│   │   ├── ps.cpp                 # Xbox process/thread compatibility
│   │   ├── kpcr.cpp/.hpp          # Per-host-thread Xbox processor control region setup
│   │   ├── mm.cpp                 # Xbox memory manager compatibility
│   │   ├── physical_memory.cpp/.hpp # Host allocation ↔ Xbox physical address tracking
│   │   ├── mmio.cpp/.hpp          # Trapped hardware-register writes and synchronous acknowledgements
│   │   ├── nv2a_pfifo.hpp         # PFIFO state and bounded command/control decoding
│   │   ├── apu.cpp/.hpp           # APU register state, MMIO queue and GP worker scheduling
│   │   ├── apu_dsp.cpp/.hpp       # GP DSP memory and instruction subset
│   │   ├── apu_dsp_dma.cpp/.hpp   # DSP DMA register actions and status
│   │   ├── hal.cpp                # Hardware abstraction and fault compatibility
│   │   └── ...                    # Other kernel export families
│   └── video/                     # SDL window and host diagnostics overlay
└── tests/
    ├── vfs_contract_tests.cpp     # Host-root, path mapping and symbolic-link contracts
    ├── io_contract_tests.cpp      # Handle, disposition, offset and query contracts
    ├── mmio_contract_tests.cpp    # MMIO readiness, command acknowledgement and register writes
    ├── apu_dsp_contract_tests.cpp # DSP instruction, peripheral and frame contracts
    ├── apu_dsp_dma_contract_tests.cpp # DSP DMA register contracts
    └── physical_memory_contract_tests.cpp # Physical range and reverse-translation contracts
```

## I/O ownership boundaries

- **VFS** owns drive mappings, symbolic links, path normalization, traversal rejection, and configurable host-root translation. It does not perform file operations.
- **Kernel handles** provide shared identity and close semantics for file and thread objects. Closing a thread handle releases its reference while the host thread can continue executing.
- **File I/O** owns `NtCreateFile`, `NtOpenFile`, reads, writes, and supported file/volume information queries. Directory dispositions use directory creation/opening. HDD volume size queries expose 16 KiB FATX allocation units rather than host block size.
- **Device I/O** dispatches device-control requests to device-specific backends. The CD-ROM backend handles the supported SCSI media page request and enforces read-only behavior; generic file operations do not contain game-title checks.
- **Runtime root** defaults to `./hdd`; `--hdd-root <path>` selects an isolated root for integration runs.

## Execution and memory model

The loader maps XBE sections, resolves kernel imports, initializes the Xbox KPCR,
and invokes the guest entry point. The physical-memory service tracks bounded
Xbox physical ranges and host allocations in both directions. Contiguous
allocations used by the guest's DMA addressing are reserved inside a bounded
address window; the NV2A worker resolves command addresses through registered
allocations or the mapped physical-memory mirror.

MMIO write faults acknowledge synchronous device commands and queue APU writes.
The APU worker owns GP state, delivers approximate frame events every 667
microseconds, and executes a bounded 27,000-instruction DSP batch. This is
bring-up scheduling, not cycle-accurate timing.

GP bootstrap loads 0x800 little-endian 24-bit program words from scratch memory.
The running DSP receives live scratch-table address and limit changes. Pending
DMA is retried on a later batch using the same range validation; EOL is asserted
only after real completion. STOP, ABORT, and bootstrap cancel pending work.
`ApuDspDma` owns descriptors, format handling and circular-offset writeback;
`ApuDspCore` owns instruction execution, registers, flags and interrupts.
Unknown instructions stop execution and remain visible in diagnostic snapshots.

NV2A shares active USER and PFIFO DMA GET/PUT state and executes validated DMA
JUMP control words. Method packets can be decoded independently, but the worker
does not yet execute graphics methods or publish guest pixels. GET is not forced
to PUT to bypass a guest wait.

`video/window.cpp` owns SDL rendering and the diagnostic overlay. The overlay
reports host-loop timing, DSP snapshots and FIFO registers. Host FPS is not a
guest render rate. Optional APU tracing is enabled with `XXCL_TRACE_APU=1`.

## Current integration checkpoint

Verified on 30 October 2026:

- A fresh isolated HDD launch requests firmware reboot; the second launch uses
  the same root and starts the guest's GPU/DSP initialization.
- NV2A processes the real DMA JUMP to `0x0470D000`. Earlier GDB evidence confirms
  the guest passes its GET/PUT wait and reaches `0x001A3255`.
- Live scratch mapping, pending-DMA retry, the signed `CMP X0,A` fix, direct-memory
  JCLR/JSET, and immediate MOVEC to M0–M7 now have contract coverage.
- The latest clean run passes DSP `PC=0x070` and stops at unsupported
  `PC=0x072`, instruction `0x0461A0`; following words are `0x0462A0/0x0463A0`.
  A diagnostic trace confirms the unsupported-instruction result.
- The latest build and all eight CTest targets pass. The command `./xxcl`
  launches the rebuilt `xbe-inspect/build/xbe-inspect` target.
- Completion of the guest's audio wait is not yet proven, and no game image has
  been rendered. The next instruction is recorded for the next development task.

Historical intermediate checkpoints are recorded in Git and `STATUS.md`.
`SESSION_HANDOFF.md` contains the current resume instructions and run commands.

The architecture goal is reusable Xbox compatibility across titles. Keep
device protocol behavior in device backends and preserve kernel export calling
conventions; do not introduce title-specific branches into generic services.
