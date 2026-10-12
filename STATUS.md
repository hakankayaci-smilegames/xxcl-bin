# XXCL Development Status

## Current checkpoint

- The source builds in an isolated Debug build directory.
- The generic VFS, file-I/O, MMIO, APU, and physical-memory contract checks pass (5 CTest targets).
- The VFS now resolves Xbox drive paths and symbolic links beneath a configurable host root. File handles are tracked and validated; create dispositions, explicit/current offsets, close, supported file/volume queries, and CD-ROM write protection have contract checks.
- `NtCreateFile` (Ordinal 190) is already implemented and registered. The previous note that it was missing was stale.
- File and thread handles now share one kernel handle table. The game closes the thread handle successfully and passes its prior `0x00106CBC` startup fault.
- The Xbox HDD volume query reports 16 KiB FATX allocation units. `NtCreateFile` handles directory `FILE_OPEN_IF` through directory creation/opening instead of POSIX `O_CREAT | O_DIRECTORY`.
- The physical-address service assigns bounded Xbox physical ranges to contiguous host allocations and resolves addresses in both directions. Unknown host pages receive stable page mappings.
- With a new temporary HDD root, the first Sega GT launch creates `TDATA` and `UDATA` directories, writes the title metadata/images, and requests firmware reboot. The original second-launch DSOUND fault at `0x0015F8DB` came from a null audio object: APU startup failed because emulated MMIO did not acknowledge two synchronous command writes. A general MMIO write trap now acknowledges those writes, and the audio factory returns success. The existing `hdd/` tree was hash-identical before and after the integration runs.
- The APU backend initializes the empty VP PIO queue to `0x80` slots, and the NV2A worker no longer writes to the APU aperture. Sega GT passes its `0x00168874` free-space poll. The APU MMIO trap queues register writes for processing outside the signal handler, and VP input/output SGE handle/offset methods now populate the guest physical SGE table. A shared SGE transfer primitive validates every descriptor and mapped guest page before copying across pages; contract tests cover both DMA directions and reject truncated SGE ranges without partial writes. GP reset/bootstrap now loads 0x800 little-endian 24-bit program words through the configured scratch SGE list; the live trace confirms `GPSADDR=0x00974000`, `GPSMAXSGE=8`, and program words at P[0..15]. On an isolated second launch, Sega GT later raises `GPSMAXSGE` to 125, writes `3` to `[EBX+0x810]`, and waits at `0x00162E2A`. A fresh core after bootstrap again shows `EIP=0x00162E2A`, `EAX=3`, and caller chain `0x162FAA -> 0x15F638 -> 0x1605AE -> 0x000D9D3D`. The blocker is now narrowed to DSP instruction execution/frame scheduling and command completion; do not clear status unconditionally.

## Next move

Implement the general DSP56300 instruction core and GP frame/interrupt scheduling around the loaded program RAM and SGE DMA primitive. Start with tested instruction and interrupt behaviors observed in the loaded firmware, then run the GP worker against the posted command, and rerun the isolated two-launch Sega GT sequence toward the first stable in-game milestone.

Keep Xbox export signatures and calling conventions stable. Keep game-specific behavior out of generic VFS, file, and device dispatch; implement device behavior in device backends.
