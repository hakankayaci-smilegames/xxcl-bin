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
- The APU backend initializes the empty VP PIO queue to `0x80` slots, and the NV2A worker no longer writes to the APU aperture. Sega GT now passes its `0x00168874` free-space poll, configures APU voice registers, and reaches a shared transfer command wait at `0x00162E2A`. A core dump confirms the guest writes `3` to `[EBX+0x810]` and spins until the device clears it; in the captured run, `EBX=0xD94DC810`, `[EBX+0x804]=0x783`, `[EBX+0x808]=0x2624`, and `[EBX+0x80C]=0x740`. The caller chain is `0x162FAA -> 0x15F638 -> 0x1605AE -> 0x000D9D3D`. The APU MMIO trace configures its current input SGE at physical `0x00964000` and programs VP voice methods before this transfer. This establishes the current blocker as an unconsumed shared APU transfer command; it does not yet prove which DSP/SGE operation completes the request.

## Next move

Implement the APU shared-transfer command path using its configured SGE/voice state and the physical-address service; determine the descriptor format before changing the command status. Add backend contract tests for descriptor validation and completion, then rerun the isolated two-launch Sega GT sequence and continue the run/trace/fix cycle toward the first stable in-game milestone.

Keep Xbox export signatures and calling conventions stable. Keep game-specific behavior out of generic VFS, file, and device dispatch; implement device behavior in device backends.
