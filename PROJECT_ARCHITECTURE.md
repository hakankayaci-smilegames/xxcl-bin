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

The loader maps XBE sections, resolves imports, initializes the Xbox KPCR, and invokes the entry point. The physical-memory service assigns bounded Xbox physical ranges to host allocations and resolves addresses in both directions. MMIO traps mapped writes and completes synchronous command acknowledgements before the guest resumes. APU writes are queued from the trap handler and consumed by the APU worker. The APU backend owns register defaults; its empty VP PIO queue reports 0x80 free slots. VP input/output SGE methods update guest physical mappings; the shared SGE transfer primitive validates descriptors before copying across pages. GP bootstrap loads 0x800 little-endian 24-bit program words into `ApuDspCore`, which owns DSP program/data/register state and its currently supported instructions. Atomic debug snapshots expose the DSP PC, nearby program words, instruction count, and worker state to the host overlay. MOVEP accesses to A/B use their accumulator-part aliases. `ApuDspDma` owns DMA register actions, descriptor walking, buffer callbacks, and EOL status separately. The observed format-6 scratch transfer now uses the shared SGE path; FIFO backends and broader DMA format coverage remain future work. The worker delivers start-frame interrupts at an approximate 667-microsecond cadence with a bounded instruction budget; it is not cycle-accurate. Short-relative BSR, accumulator aliases, descriptor transfer, PM5 direct X/Y moves, PM2 `CMP X0,A`, and long-relative `BEQ` have contract coverage. The latest isolated runtime passes the former `PC=0x064`, instruction `0x0D104A`, checkpoint. The DSP56300 NOP, unsigned `CMPU X0,A` (`0x0C1FF8`), and short `BCC` condition handling have contract coverage. The former short-JMP decode of `0x0C1FF8` was incorrect and has been removed. `BCC` evaluates the condition-code table and applies signed 9-bit displacement relative to current PC. PM2 `SUB X0,A` now stores its 56-bit result and updates arithmetic flags. The latest isolated run reaches the recurring DMA EOL poll at `PC=0x121` (`BRCLR #7,X:$C5`); tracing confirmed the DMA EOL flag is set and cleared. The worker then polls there repeatedly. The overlay recorded 298,782,000 DSP instructions, while NV2A FIFO GET=PUT only shows an empty queue at sampling time: the current worker copies PUT to GET without parsing the commands, so submission counters are needed before concluding the guest is idle.  EP execution, full GP instruction coverage, voice processing, and command completion remain future work. Optional APU tracing is disabled unless `XXCL_TRACE_APU=1` is set.

`video/window.cpp` renders a compact SDL debug overlay at the host event-loop cadence. It reports host FPS/frametime, the atomic APU DSP debug snapshot, and NV2A PFIFO DMA GET/PUT/STATUS values plus worker state. The host FPS is not a guest render rate. The overlay is currently drawn over a cleared host surface; it does not consume or display Xbox graphics. PFIFO DMA GET/PUT are read from `0xFD003244/0xFD003240`, and STATUS from `0xFD003214`; the worker observes pending updates without changing hardware pointers. Earlier diagnostics read the USER block at `0xFD800040/44` as if it were DMA pointers and are invalid.

## Current integration checkpoint

The generic VFS, file-I/O, MMIO, APU, physical-memory, DSP and DMA contract checks pass (8 CTest targets). On isolated roots, Sega GT creates title data and requests reboot on its first launch. MMIO acknowledgement moves execution past the former null dereference at `0x0015F8DB`; the APU PIO queue lets the guest pass its `0x00168874` free-space poll; VP SGE methods populate guest physical mappings. GP firmware bootstraps and passes its frame interrupt poll. DMA walks the observed terminal descriptor, transfers six 24-bit samples from SGE scratch, sets EOL, and gets past DSP `PC=0x121`. PM5 direct X/Y MOVE advances past `PC=0x060`; PM2 `CMP X0,A` advances past `PC=0x063`; long-relative `BEQ` passes the former `PC=0x064` checkpoint. The former `PC=0x171`, instruction `0x000000`, is a one-word NOP. `0x0C1FF8` is `CMPU X0,A`, not a short JMP. After implementing it and short conditional `BCC`, the isolated run reaches `PC=0x121`, where GP firmware polls DMA EOL. `SUB X0,A` is now implemented and covered. The worker remains in that wait loop after EOL is cleared. Prior NV2A interval evidence was invalid because the code sampled the USER register area (`0xFD800040/44`) and mirrored its PUT value to GET. The corrected worker samples PFIFO DMA GET/PUT (`0xFD003244/240`) and STATUS (`0xFD003214`) without mutating them; a new isolated runtime is needed to establish whether guest submissions are visible. The host overlay shows host-loop FPS/frametime, DSP progress, and current PFIFO registers. First actual game drawing still needs a guest pushbuffer parser and host graphics backend.

The architecture goal is reusable Xbox compatibility behavior across titles. Title-specific device responses belong in device backends only where a device protocol requires them, not in generic VFS or file-handle code.
