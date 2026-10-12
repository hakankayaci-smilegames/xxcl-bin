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
│   │   ├── ps.cpp                 # Xbox process/thread compatibility
│   │   ├── kpcr.cpp/.hpp          # Per-host-thread Xbox processor control region setup
│   │   ├── mm.cpp                 # Xbox memory manager compatibility
│   │   ├── hal.cpp                # Hardware abstraction and fault compatibility
│   │   └── ...                    # Other kernel export families
│   └── video/                     # Graphics backends and stubs
└── tests/
    ├── vfs_contract_tests.cpp     # Host-root, path mapping and symbolic-link contracts
    └── io_contract_tests.cpp      # Handle, disposition, offset and query contracts
```

## I/O ownership boundaries

- **VFS** owns drive mappings, symbolic links, path normalization, traversal rejection, and configurable host-root translation. It does not perform file operations.
- **File I/O** owns host descriptor lifetime and the Xbox handle table. `NtCreateFile`, `NtOpenFile`, reads, writes, close, and supported file/volume information queries use this table and report invalid handles or host errors explicitly.
- **Device I/O** dispatches device-control requests to device-specific backends. The CD-ROM backend handles the supported SCSI media page request and enforces read-only behavior; generic file operations do not contain game-title checks.
- **Runtime root** defaults to `./hdd`; `--hdd-root <path>` selects an isolated root for integration runs.

## Execution and memory model

The loader maps the XBE at its expected virtual addresses, applies section permissions, resolves imported kernel ordinals, initializes the Xbox KPCR, and invokes the entry point. Memory-manager and HAL modules provide the guest address-space and hardware-compatibility behavior. Thread creation must initialize a valid per-thread KPCR/TLS context and return handles that agree with `NtClose` semantics.

## Current integration checkpoint

The generic VFS/file contract checks pass. The latest isolated Sega GT run stops in startup before file I/O: the game closes a placeholder system-thread handle, receives `STATUS_INVALID_HANDLE`, and the guest status-to-error helper faults while accessing a null TLS slot at XBE address `0x00106CBC`. The next investigation belongs in the general thread-handle and KPCR/TLS layers. The older `0x0015F8DB` audio-path fault has not been reproduced in this run.

The architecture goal is reusable Xbox compatibility behavior across titles. Title-specific device responses belong in device backends only where a device protocol requires them, not in generic VFS or file-handle code.
