# XXCL Development Status

## Current checkpoint

- The source builds in an isolated Debug build directory.
- The generic VFS and file-I/O contract checks pass (2 CTest targets).
- The VFS now resolves Xbox drive paths and symbolic links beneath a configurable host root. File handles are tracked and validated; create dispositions, explicit/current offsets, close, supported file/volume queries, and CD-ROM write protection have contract checks.
- `NtCreateFile` (Ordinal 190) is already implemented and registered. The previous note that it was missing was stale.
- A fresh Sega GT 2002 run used a temporary HDD root. The existing `hdd/` tree was hash-identical before and after.
- The fresh run stops before file I/O: the game creates a system thread, later closes the placeholder handle `0x8899AABB`, receives `STATUS_INVALID_HANDLE`, then its guest status-to-error helper attempts to write through a null TLS slot and faults at XBE address `0x00106CBC` (read/write fault address `0x4`). This is earlier than the October 9 log's `0x0015F8DB` audio-path crash, which remains unconfirmed against a fresh run.

## Next move

Trace and correct the general thread-object handle and per-thread TLS setup in the process/thread and KPCR layers. Add a focused contract check for creation/close and TLS initialization. Then rerun the generic checks and Sega GT in a fresh temporary runtime root, recording the next checkpoint. Continue toward the older audio crash only if the new run reaches it.

Keep Xbox export signatures and calling conventions stable. Keep game-specific behavior out of generic VFS, file, and device dispatch; implement device behavior in device backends.
