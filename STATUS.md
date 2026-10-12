# XXCL Development Status

## Current checkpoint

- The source builds in an isolated Debug build directory.
- The generic VFS, file-I/O, MMIO, and physical-memory contract checks pass (4 CTest targets).
- The VFS now resolves Xbox drive paths and symbolic links beneath a configurable host root. File handles are tracked and validated; create dispositions, explicit/current offsets, close, supported file/volume queries, and CD-ROM write protection have contract checks.
- `NtCreateFile` (Ordinal 190) is already implemented and registered. The previous note that it was missing was stale.
- File and thread handles now share one kernel handle table. The game closes the thread handle successfully and passes its prior `0x00106CBC` startup fault.
- The Xbox HDD volume query reports 16 KiB FATX allocation units. `NtCreateFile` handles directory `FILE_OPEN_IF` through directory creation/opening instead of POSIX `O_CREAT | O_DIRECTORY`.
- The physical-address service assigns bounded Xbox physical ranges to contiguous host allocations and resolves addresses in both directions. Unknown host pages receive stable page mappings.
- With a new temporary HDD root, the first Sega GT launch creates `TDATA` and `UDATA` directories, writes the title metadata/images, and requests firmware reboot. The original second-launch DSOUND fault at `0x0015F8DB` came from a null audio object: APU startup failed because emulated MMIO did not acknowledge two synchronous command writes. A general MMIO write trap now acknowledges those writes, and the audio factory returns success. The existing `hdd/` tree was hash-identical before and after the integration runs.
- After the physical-address change, the second-launch stall remains at `0x00162E2A` in DSOUND. The guest writes `3` to a DMA command word at an allocated host address (`EBX=0xD70DC810` in the latest attached run) and spins until the APU clears it. The physical mapping now returns stable addresses for these allocations, but no APU worker consumes the command. The APU range is currently only anonymous register memory without front-end method dispatch, voice processing, or DSP DMA behavior.

## Next move

Implement a modular APU device backend that handles register access and the command path, using the physical-address service to find guest buffers. Trace how the guest submits this command, then add device-level checks for the observed protocol before rerunning all generic checks and the two-launch Sega GT sequence in an isolated runtime root.

Keep Xbox export signatures and calling conventions stable. Keep game-specific behavior out of generic VFS, file, and device dispatch; implement device behavior in device backends.
