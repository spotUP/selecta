# M3 I158b report: Amiga library open crash

## Root cause (written before the first edit)

Invariant violated: every Amiga executable must run with a stack large enough for its deepest call chain; nothing in the build sets one, so the 68k binaries run on the launcher's default stack (vamos: 4 KiB; AmigaDOS shell default is also 4096 bytes).

Evidence:
- `vamos -C 68020 -m 8192 -- build/amiga/apps/selecta-cli/selecta-cli library stats` dies with `InvalidMemoryAccessError R(4): 2000008`, PC=0x31e90c, A0=0x02000004.
- PC minus the text base (Initial PC 0x210c) is 0x31c800, which `m68k-amigaos-nm` maps to libnix `_write` (0x31c7c4), at `movel a2@(20),a0 ; moveal a0@(4),a1`: the fd record it reads is garbage. Caller on the stack is `___fflush`. The stack was overwritten by SQLite's frames (sqlite3_open_v2 / schema migrate), so stdio state next to it was clobbered; this is a stack overflow, not a VFS, locking, mmap or malloc fault.
- Bisect by `vamos -s <KiB>`: 8 KiB fails, 16, 24, 32 and 48 KiB all print `db_open=1 schema_version=1 ... fts5=1 ... worker_requests=2`. `roundtrip` (no SQLite) passes on the default stack, which is why nothing earlier noticed.
- No `__stack` cookie anywhere in `apps/`, `src/` or `cmake/` (grep). libnix's `swapstack.o` (`___stkinit`, reads `___stack`) swaps to a bigger stack at startup when `unsigned long __stack` is defined.

Fix level: define the libnix `__stack` cookie in the Amiga executables (shared source `apps/common/amiga_stack.c`, linked by `selecta` and `selecta-cli` when SEL_AMIGA), 64 KiB, so the binary asks for its stack instead of inheriting the launcher's.

## Step 1 result and the fix (final)

- Cookie binary (WIP commit 83d8dc6, 64 KiB `__stack`, `__stkinit` forced in) under `vamos -s 128`: still crashes (InvalidMemoryAccessError R(2) at fffffffe), as the cause predicts: the swap copies a stack whose old bounds read 0.
- Without a cookie, `library stats` passes at 16 KiB and up (bisect above). With the new start-up, `vamos -s 128` still reports a swap (stack_swaps=1), i.e. vamos leaves tc_SPLower/tc_SPUpper at 0 even when `-s` is given, confirming the bounds are never written for the main process.
- Fix: `apps/common/amiga_start.{c,h}` `sel_amiga_run_with_stack(body, argc, argv, bytes)`: reads FindTask(0) bounds, when 0 or smaller than `bytes` AllocMem's the stack, exec StackSwap around the body, swaps back, frees. State kept in statics because A7-relative locals are unreachable after a swap. 128 KiB (Q90). `selecta` and `selecta-cli` main call it under `__amigaos__` (body renamed `app_main`); `amiga_stack.c` and the `__stkinit` reference are removed.
- Sentinel: `library stats` prints `stack_swaps=N stack_bytes=B` on the Amiga only. `selecta-cli library stats` under vamos with no `-s` prints `db_open=1 ... stack_swaps=1 stack_bytes=131072`.
- Test: `source-local-scan-vamos` (tests/m3/scan_vamos.cmake) now also runs `library stats` without `-s` and asserts db_open=1 and the swap sentinel. Its desktop-vs-vamos comparison was relaxed: the 68k build has no openmpt plugin, so it recognises 9 of the 11 desktop modules (scanned+skipped = 14 on both, dirs equal, db_songs == scanned_files <= desktop).
- Results: `-R 'vamos|amiga|no-float'` 22 of 24 passed at first run; source-local-scan-vamos green after the comparison fix. `no-float-engine` fails (expected 29 objects, found 49 in build/amiga plugin/library objects): not touched by this change (no engine or library file edited); unverified whether it fails on main too.
