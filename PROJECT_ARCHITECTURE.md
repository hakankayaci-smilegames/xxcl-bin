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
│   │   ├── apu.cpp/.hpp           # APU register state, MMIO queue and GP worker scheduling
│   │   ├── apu_dsp.cpp/.hpp       # GP DSP memory and instruction subset
│   │   ├── apu_dsp_dma.cpp/.hpp   # DSP DMA register actions and status
│   │   ├── hal.cpp                # Hardware abstraction and fault compatibility
│   │   └── ...                    # Other kernel export families
│   └── video/                     # Graphics backends and stubs
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

The loader maps XBE sections, resolves imports, initializes the Xbox KPCR, and invokes the entry point. The physical-memory service assigns bounded Xbox physical ranges to host allocations and resolves addresses in both directions. MMIO traps mapped writes and completes synchronous command acknowledgements before the guest resumes. APU writes are queued from the trap handler and consumed by the APU worker. The APU backend owns register defaults; its empty VP PIO queue reports 0x80 free slots. VP input/output SGE methods update guest physical mappings; the shared SGE transfer primitive validates descriptors before copying across pages. GP bootstrap loads 0x800 little-endian 24-bit program words into `ApuDspCore`, which owns DSP program/data/register state and its currently supported instructions. `ApuDspDma` owns DMA register actions and status separately. The worker delivers start-frame interrupts at an approximate 667-microsecond cadence with a bounded instruction budget; it is not cycle-accurate. Short-relative BSR now passes; current runtime reaches DSP PC `0x121`, polling the DMA end-of-list bit. Descriptor transfer and EOL signaling are not implemented yet. EP execution, full GP instruction coverage, DMA descriptor transfers, voice processing, and command completion remain future work. Optional APU tracing is disabled unless `XXCL_TRACE_APU=1` is set.

## Current integration checkpoint

The generic VFS, file-I/O, MMIO, APU, physical-memory, DSP and DMA contract checks pass (7 CTest targets). On isolated roots, Sega GT creates title data and requests reboot on its first launch. MMIO acknowledgement moves execution past the former null dereference at `0x0015F8DB`; the APU PIO queue lets the guest pass its `0x00168874` free-space poll; VP SGE methods populate guest physical mappings. GP firmware now bootstraps, handles observed DMA actions, and passes its start-frame interrupt poll. Short-relative BSR now passes. The latest run reaches DSP PC `0x121`, polling the DMA end-of-list bit. The repository `hdd/` remains hash-identical after isolated runs. The next step is generic descriptor transfer and EOL signaling.

The architecture goal is reusable Xbox compatibility behavior across titles. Title-specific device responses belong in device backends only where a device protocol requires them, not in generic VFS or file-handle code.
