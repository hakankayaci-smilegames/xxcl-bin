# XXCL (Xbox Compatibility Layer) Architecture

XXCL is a modular compatibility layer that maps original Xbox NT Kernel (XTL) calls directly to Linux system calls (using POSIX APIs). This allows untouched `.xbe` executable binaries from the Xbox (like Sega GT) to run on a modern Linux kernel without requiring high-level hardware emulation, essentially acting like WINE for Xbox.

## Core Structure
- **VFS (Virtual File System) [`src/kernel/vfs.hpp/.cpp`]:**
  - Handles the translation between Xbox NT Object Manager paths (e.g., `\Device\Harddisk0\partition1\`) and native Linux paths (e.g., `./hdd/E/`).
  - Automatically translates backslashes `\` to forward slashes `/`.
  - Simulates Symbolic Link mapping (e.g., `IoCreateSymbolicLink` mapping `\??\U:` to the UDATA folder).
- **Executive & Memory Management (MM / XE) [`src/kernel/mm.cpp`, `src/kernel/xe.cpp`]:**
  - **`MmAllocateContiguousMemory` / `Ex`:** Maps directly to `posix_memalign` to provide page-aligned physical memory buffers.
  - **`NtAllocateVirtualMemory`:** Emulates both `MEM_RESERVE` and `MEM_COMMIT` logic. When games use custom CRT heaps, they expect a reserved address range to be individually committed later. XXCL tracks these memory ranges.
  - **`XeLoadSection` / `UnloadSection`:** Mocks the dynamic section loading (overlay) mechanism of the Xbox XBE format, tricking games into thinking sections are loaded/unloaded physically.
- **Thread & Synchronization (PS / RTL) [`src/kernel/ps.cpp`, `src/kernel/rtl.cpp`]:**
  - Xbox threads are implemented as native Linux pthreads via `syscall(SYS_set_thread_area)`.
  - A fake KPCR (Kernel Processor Control Region) is mounted to the Thread-Local Storage (TLS) `fs` segment. This prevents the `fs:[0x04]` MSVC compiler crash on `__declspec(thread)` variables.
  - All Xbox RTL String and Critical Section operations map to empty stubs or string comparators (`strncasecmp`).
- **Input/Output (I/O) [`src/kernel/io.cpp`]:**
  - NT IOCTL requests (`NtDeviceIoControlFile`) map to dummy `STATUS_SUCCESS` responses to bypass DVD and HDD hardware verification.
  - `NtCreateFile`, `NtReadFile`, `NtWriteFile` use standard POSIX `open`, `read`, `write`.

## Calling Conventions
- All exported kernel functions strictly use `__attribute__((__stdcall__))` to handle x86 stack cleanup correctly and match the Xbox executable's `call` instructions.

## Status
- **Current Phase:** Video / Audio Hardware Initialization.
- The VFS and Kernel Executive subsystems are functional enough to handle EEPROM queries, game saves (`$$XTIMAGE`), and dynamic section loading (`langus`). The game is currently attempting to write directly to NV2A MMIO registers (`0xFD000000`).
