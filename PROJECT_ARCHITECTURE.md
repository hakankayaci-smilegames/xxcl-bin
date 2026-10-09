# XXCL (Xbox Compatibility Layer) Architecture

XXCL is a modular compatibility layer that maps original Xbox NT Kernel (XTL) calls directly to Linux system calls (using POSIX APIs). This allows untouched `.xbe` executable binaries from the Xbox (like Sega GT 2002) to run on a modern Linux kernel without requiring high-level hardware CPU emulation, essentially acting like WINE for Xbox.

## Core Structure & Accomplishments

### 1. VFS (Virtual File System) [`src/kernel/vfs.hpp/.cpp`]
- Handles the translation between Xbox NT Object Manager paths (e.g., `\Device\Harddisk0\partition1\`) and native Linux paths (e.g., `./hdd/E/`).
- Automatically translates backslashes `\` to forward slashes `/`.
- Simulates Symbolic Link mapping (e.g., `IoCreateSymbolicLink` mapping `\??\D:` to the current directory).
- Successfully handled `NtCreateFile`, `NtReadFile`, `NtWriteFile`, allowing the game to save/load EEPROM, save icons (`$$XTIMAGE`, `$$XSIMAGE`), and read the `TitleMeta.xbx` file on the native Linux disk!

### 2. Executive & Memory Management (MM / XE) [`src/kernel/mm.cpp`, `src/kernel/xe.cpp`]
- **`MmAllocateContiguousMemory` / `Ex`:** Maps directly to `posix_memalign` to provide page-aligned physical memory buffers. Corrected standard NT `__stdcall` signature counts to prevent stack frame corruption.
- **`NtAllocateVirtualMemory`:** Emulates both `MEM_RESERVE` and `MEM_COMMIT` logic. When games use custom CRT heaps, they expect a reserved address range to be individually committed later. XXCL tracks these memory ranges.
- **Physical Memory Mirrors (KSEG0 / KSEG1):** Emulates Xbox's Direct Physical Memory mapped ranges `0x80000000` (Cached) and `0xA0000000` (Uncached) via `mmap(MAP_FIXED)`. This allowed the game to write its GPU GART (Page Table) base directly to `0x80000000`.
- **`MmClaimGpuInstanceMemory`:** Correctly implements physical graphics allocations.
- **`XeLoadSection` / `UnloadSection`:** Mocks the dynamic section loading (overlay) mechanism of the Xbox XBE format, tricking games into thinking XBE `.text`/`.data` sections are loaded/unloaded physically (successfully verified with the `langus` section).

### 3. Hardware Emulation via `SIGSEGV` (CPU Ring 3 Bypasses) [`src/loader.cpp`]
- Instead of using a Hypervisor or requiring `root` permissions with `iopl(3)`, XXCL traps privileged x86 instructions directly using a `sigaction(SIGSEGV)` and `ucontext_t` handler.
- **IN/OUT Port Emulation:** Successfully emulates x86 hardware I/O instructions (`0xEE` for `OUT DX, AL`, `0xEF` for `OUT DX, EAX`) used to hit PCI/VGA ports.
- **Privileged Cache Instructions:** Bypasses `WBINVD` (`0F 09`) and `INVD` (`0F 08`) instructions. These are Ring 0 commands used by Xbox to flush the CPU cache when writing page tables for the GPU DMA. XXCL traps and skips them cleanly (`EIP += 2`).

### 4. GPU MMIO & PCI Subsystem [`src/kernel/nv2a.cpp`, `src/kernel/hal.cpp`]
- **Fake NV2A MMIO Mapping:** Uses `mmap(MAP_FIXED)` at `0xFD000000` (16MB) to catch and mock direct memory-mapped register reads/writes by the Xbox game.
- **Stateful PCI Space (`HalReadWritePCISpace`):** Implements a 256-byte stateful array responding to Bus 1, Slot 0 NV2A queries. Exposes Vendor ID `0x10DE`, Device ID `0x02A0`, and specific AGP mappings like Register `0x4C` (`0x0000007F`).

### 5. Threading & Synchronization (PS / RTL) [`src/kernel/ps.cpp`, `src/kernel/rtl.cpp`]
- Xbox threads are implemented as native Linux pthreads via `syscall(SYS_set_thread_area)`.
- A fake KPCR (Kernel Processor Control Region) is mounted to the Thread-Local Storage (TLS) `fs` segment. This prevents the `fs:[0x04]` MSVC compiler crash on `__declspec(thread)` variables.
- All Xbox RTL String and Critical Section operations map to empty stubs or string comparators (`strncasecmp`).

### 6. Interrupt Subsystem (KE / HAL) [`src/kernel/ke.cpp`, `src/kernel/hal.cpp`]
- **`KeInitializeInterrupt` / `KeConnectInterrupt`:** Successfully traps IRQ mapping (e.g., IRQ 3 for VBlank synchronization). Corrected the `__stdcall` signature to exactly 7 arguments to fix severe stack corruption during GPU initialization.

---

## Current State & Next Steps (Context for Next Session)

**Where We Are:**
The emulator successfully initializes the VFS, memory manager, thread pools, and loads the XBE. It processes NV2A PCI initialization, maps KSEG0, configures the GPU page tables (GART) at `0x80000000`, flushes the cache (`WBINVD`), and correctly catches everything through the SIGSEGV hardware trap!

**The Current Problem:**
Immediately after the `WBINVD` instruction bypass, the Xbox game enters an infinite spin-loop.
The game thread consumes 100% CPU because it is continuously polling an NV2A MMIO status register (specifically `0xFD001804` and similar offsets), waiting for the GPU to report "Idle" or a specific status bit. Since we mapped `0xFD000000` via `mmap(PROT_READ | PROT_WRITE)` to anonymous zero-filled memory, the CPU reads `0x00000000` infinitely and the status never changes.

**Next Steps for the AI / User:**
1. **Break the MMIO Spin-Loop:**
   We need to populate the `0xFD000000` MMIO region with the correct dummy values the game expects.
   - Option A: In `nv2a_init_mmio`, directly write the "Idle" bit values to the most common offsets (like `0x1804`, `0x400`, `0x3204`, etc.) so the game breaks out of the loops. You can fill the whole block with `0xFFFFFFFF` as a dirty test.
   - Option B: Use `PROT_NONE` on the `0xFD000000` mapping, and extend the `SIGSEGV` handler in `loader.cpp` to decode x86 memory reads (e.g., `mov eax, [0xFD001804]`), dynamically returning the mocked GPU status and advancing the instruction pointer. This is harder but provides exact tracing of what the game is doing with the GPU registers.
2. Once the GPU queue loops are broken, we need to inspect the DMA PushBuffers to extract the actual 3D rendering methods (D3D state) being sent to the GPU, and translate them to OpenGL or Vulkan.

**Rules for Continuation:**
- Do not introduce game-specific hacks. The MMIO layout and GPU polling bypasses must mimic real Xbox hardware behavior.
- Always use `__attribute__((__stdcall__))` with the *exact* number of Xbox kernel arguments to avoid stack corruption (like the one we fixed with `KeInitializeInterrupt`).
