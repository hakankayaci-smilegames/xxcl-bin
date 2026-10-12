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
│   │   ├── hal.cpp                # Hardware abstraction and fault compatibility
│   │   └── ...                    # Other kernel export families
│   └── video/                     # Graphics backends and stubs
└── tests/
    ├── vfs_contract_tests.cpp     # Host-root, path mapping and symbolic-link contracts
    ├── io_contract_tests.cpp      # Handle, disposition, offset and query contracts
    ├── mmio_contract_tests.cpp    # MMIO readiness, command acknowledgement and register writes
    └── physical_memory_contract_tests.cpp # Physical range and reverse-translation contracts
```

## I/O ownership boundaries

- **VFS** owns drive mappings, symbolic links, path normalization, traversal rejection, and configurable host-root translation. It does not perform file operations.
- **Kernel handles** provide shared identity and close semantics for file and thread objects. Closing a thread handle releases its reference while the host thread can continue executing.
- **File I/O** owns `NtCreateFile`, `NtOpenFile`, reads, writes, and supported file/volume information queries. Directory dispositions use directory creation/opening. HDD volume size queries expose 16 KiB FATX allocation units rather than host block size.
- **Device I/O** dispatches device-control requests to device-specific backends. The CD-ROM backend handles the supported SCSI media page request and enforces read-only behavior; generic file operations do not contain game-title checks.
- **Runtime root** defaults to `./hdd`; `--hdd-root <path>` selects an isolated root for integration runs.

## Execution and memory model

The loader maps the XBE at its expected virtual addresses, applies section permissions, resolves imported kernel ordinals, initializes the Xbox KPCR, and invokes the entry point. Memory-manager and HAL modules provide the guest address-space and hardware-compatibility behavior. Thread creation must initialize a valid per-thread KPCR/TLS context and return handles that agree with `NtClose` semantics. The physical-memory service assigns bounded physical ranges to host allocations and resolves them in both directions. The MMIO component traps writes to the mapped interrupt-controller page, replays the original guest instruction, and completes synchronous command acknowledgements before the guest resumes. The APU backend owns APU register defaults; its empty VP PIO queue reports 0x80 free slots. APU command dispatch, voice processing, and DSP DMA still need implementation using mapped guest buffers. Optional APU write tracing is disabled unless `XXCL_TRACE_APU=1` is set.

## Current integration checkpoint

The generic VFS, file-I/O, MMIO, APU, and physical-memory contract checks pass. On a fresh isolated root, Sega GT's first launch creates `TDATA` and `UDATA` title data and requests a reboot. MMIO acknowledgement lets the DSOUND audio factory succeed and moves execution past its former null dereference at `0x0015F8DB`. The former NV2A worker wrote an APU FIFO register; that cross-device write is removed. The APU empty VP PIO queue now reports 0x80 available slots, allowing the guest to pass its `0x00168874` free-space poll. A 45-second isolated run continued making physical-address queries and timed out without a new crash; the current guest instruction pointer still needs capture. The older `0x00162E2A` command wait is not yet confirmed as the next failure after this startup change.

The architecture goal is reusable Xbox compatibility behavior across titles. Title-specific device responses belong in device backends only where a device protocol requires them, not in generic VFS or file-handle code.
