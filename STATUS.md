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
- The next second-launch blocker was an APU VP PIO free-space poll at `0x00168874`, reading `0xFE820010` and waiting for at least `0x20` free slots. The NV2A worker had been writing `0x200` into this APU register every 500 microseconds, crossing the GPU/APU boundary. That write is removed. The APU backend now initializes the empty VP PIO queue to `0x80` slots, matching the reference device empty-queue response, and has a standalone contract test. Sega GT passed this poll and proceeded into further physical-address queries; a 45-second isolated run remained alive until its timeout, with no new crash captured. The earlier `0x00162E2A` DMA command wait is not yet reproduced after this startup change, so it remains an open APU command-path question, not a confirmed current checkpoint.

## Next move

Capture the current guest thread instruction pointer after the PIO free-space poll, then continue modeling the APU register and command paths in the APU backend. Use the physical-address service to find guest buffers, add device-level checks for each observed protocol, and rerun the two-launch Sega GT sequence in an isolated runtime root.

Keep Xbox export signatures and calling conventions stable. Keep game-specific behavior out of generic VFS, file, and device dispatch; implement device behavior in device backends.
