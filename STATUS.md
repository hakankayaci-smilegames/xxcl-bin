# XXCL Development Status

## Current checkpoint

- The source builds in an isolated Debug build directory.
- The generic VFS, file-I/O, and MMIO contract checks pass (3 CTest targets).
- The VFS now resolves Xbox drive paths and symbolic links beneath a configurable host root. File handles are tracked and validated; create dispositions, explicit/current offsets, close, supported file/volume queries, and CD-ROM write protection have contract checks.
- `NtCreateFile` (Ordinal 190) is already implemented and registered. The previous note that it was missing was stale.
- File and thread handles now share one kernel handle table. The game closes the thread handle successfully and passes its prior `0x00106CBC` startup fault.
- The Xbox HDD volume query reports 16 KiB FATX allocation units. `NtCreateFile` handles directory `FILE_OPEN_IF` through directory creation/opening instead of POSIX `O_CREAT | O_DIRECTORY`.
- With a new temporary HDD root, the first Sega GT launch creates `TDATA` and `UDATA` directories, writes the title metadata/images, and requests firmware reboot. The original second-launch DSOUND fault at `0x0015F8DB` came from a null audio object: APU startup failed because emulated MMIO did not acknowledge two synchronous command writes. A general MMIO write trap now acknowledges those writes, and the audio factory returns success. The existing `hdd/` tree was hash-identical before and after the integration runs.
- The next observed second-launch stall is at `0x00162E2A` in DSOUND. The guest writes `3` to a DMA command word at an allocated host address (`EBX=0xD86DC810` in the captured run) and spins until the APU clears it. This is a new checkpoint, not yet an implemented APU DMA contract.

## Next move

Build a general APU DMA command path: track contiguous allocations and their Xbox physical addresses, identify which APU register points to the command buffer, and process/acknowledge command words through an audio device backend. Reproduce the `0x00162E2A` wait first, add a device-level contract check, then rerun all generic checks and the two-launch Sega GT sequence in an isolated runtime root.

Keep Xbox export signatures and calling conventions stable. Keep game-specific behavior out of generic VFS, file, and device dispatch; implement device behavior in device backends.
