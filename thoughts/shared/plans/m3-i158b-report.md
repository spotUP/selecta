# M3 I158b report: Amiga library open crash

## Root cause (written before the first edit)

Invariant violated: every Amiga executable must run with a stack large enough for its deepest call chain; nothing in the build sets one, so the 68k binaries run on the launcher's default stack (vamos: 4 KiB; AmigaDOS shell default is also 4096 bytes).

Evidence:
- `vamos -C 68020 -m 8192 -- build/amiga/apps/selecta-cli/selecta-cli library stats` dies with `InvalidMemoryAccessError R(4): 2000008`, PC=0x31e90c, A0=0x02000004.
- PC minus the text base (Initial PC 0x210c) is 0x31c800, which `m68k-amigaos-nm` maps to libnix `_write` (0x31c7c4), at `movel a2@(20),a0 ; moveal a0@(4),a1`: the fd record it reads is garbage. Caller on the stack is `___fflush`. The stack was overwritten by SQLite's frames (sqlite3_open_v2 / schema migrate), so stdio state next to it was clobbered; this is a stack overflow, not a VFS, locking, mmap or malloc fault.
- Bisect by `vamos -s <KiB>`: 8 KiB fails, 16, 24, 32 and 48 KiB all print `db_open=1 schema_version=1 ... fts5=1 ... worker_requests=2`. `roundtrip` (no SQLite) passes on the default stack, which is why nothing earlier noticed.
- No `__stack` cookie anywhere in `apps/`, `src/` or `cmake/` (grep). libnix's `swapstack.o` (`___stkinit`, reads `___stack`) swaps to a bigger stack at startup when `unsigned long __stack` is defined.

Fix level: define the libnix `__stack` cookie in the Amiga executables (shared source `apps/common/amiga_stack.c`, linked by `selecta` and `selecta-cli` when SEL_AMIGA), 64 KiB, so the binary asks for its stack instead of inheriting the launcher's.

## Status at the 50-call budget: NOT fixed, test still red

- Measured stack need under vamos (`-s <KiB>`, stack given by the launcher): `library stats` needs 16 KiB (8 fails); `library scan song:` runs at 64 KiB and above (the scan also dies on the default stack, second crash: memsys5 `memsys5Link` writes through a corrupted pool pointer, PC offset in sqlite3 memsys5, A0=b2040396, which is the same overflow showing up as heap-metadata corruption).
- The `__stack` cookie (`apps/common/amiga_stack.c`, 64 KiB) does NOT take effect: `selecta-cli library stats` without `-s` still crashes, and the crash SP stays about 8.7 KiB under the initial SP. `nm` shows `___stack` present but `swapstack.o` (`___stkinit`, `___stkswap`) was not linked; forcing a reference to `__stkinit` links and still does not change the result under vamos. Next step: find how bebbo libnix (`-mcrt=nix20`) activates stack swapping (startup code `ncrt0.o`/`libnix20.a` objects that reference `___stkinit`, or an explicit `StackSwap()` / own-stack launch in `main` using AllocMem + `StackSwap`, as `src/core/amiga/audio_task.c` already allocates task stacks), and whether vamos emulates exec `StackSwap`. A robust alternative that does not depend on libnix: run the real `main` body on a stack the program allocates (`AllocMem` + `StackSwap`) from one shared Amiga start-up file.
- `source-local-scan-vamos` is enabled (DISABLED removed) and fails on the Amiga run (vamos rc=1); it is the regression test (red before, red now).

| topic | default | alternative |
|---|---|---|
| stack mechanism | explicit StackSwap in a shared Amiga start-up, 128 KiB | find the libnix cookie activation and keep `__stack` |
| stack size | 128 KiB (scan runs at 64 KiB under vamos, headroom for decoders) | 64 KiB |
