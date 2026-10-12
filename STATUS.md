# XXCL Development Status

## Current checkpoint

- The source builds in an isolated Debug build directory.
- The generic VFS and file-I/O contract checks pass (2 CTest targets).
- The VFS now resolves Xbox drive paths and symbolic links beneath a configurable host root. File handles are tracked and validated; create dispositions, explicit/current offsets, close, supported file/volume queries, and CD-ROM write protection have contract checks.
- `NtCreateFile` (Ordinal 190) is already implemented and registered. The previous note that it was missing was stale.
- File and thread handles now share one kernel handle table. The game closes the thread handle successfully and passes its prior `0x00106CBC` startup fault.
- The Xbox HDD volume query reports 16 KiB FATX allocation units. `NtCreateFile` handles directory `FILE_OPEN_IF` through directory creation/opening instead of POSIX `O_CREAT | O_DIRECTORY`.
- With a new temporary HDD root, the first Sega GT launch creates `TDATA` and `UDATA` directories, writes the title metadata/images, and requests firmware reboot. A second launch against that same temporary root passes language and graphics initialization, makes 2,357 kernel calls, and faults at XBE address `0x0015F8DB` in the DSOUND section (fault address `0x8`, last ordinal 277). The existing `hdd/` tree was hash-identical before and after both runs.

## Next move

Trace the reproduced `0x0015F8DB` DSOUND fault through its caller and the audio object it dereferences. Fix the responsible general audio or memory contract, then rerun the generic checks and the two-launch Sega GT sequence in an isolated runtime root.

Keep Xbox export signatures and calling conventions stable. Keep game-specific behavior out of generic VFS, file, and device dispatch; implement device behavior in device backends.
