# I218d: Latin-1 file names at the AmigaDOS boundary

Ledger note: `thoughts/spot/todos.md` has no row I218d (rows I218, I218a, I218b exist). Not edited, per brief.

## Fix
Engine strings are UTF-8. AmigaDOS names are Latin-1. Conversion sits only in `src/core/amiga` (host POSIX is untouched).
- `src/library/latin1.{h,c}`: `sel_latin1_to_utf8` (moved from `hippo_sync.c`), new `sel_utf8_to_latin1`, `sel_latin1_is_ascii`.
- Code point above U+00FF (or malformed or overlong UTF-8, or no fit): `sel_utf8_to_latin1` returns 0 with an empty
  string and the OS call fails (ENOENT / 0 / -1). It never substitutes `?`: `?` is a legal wildcard-like name
  character and would name a different file; no such name can exist on a Latin-1 volume.
- `src/core/amiga/dos_name.h`: `sel_dos_path` wraps it. Used by `dir.c`, `config_mkdir.c`, `dos_quiet.c`.
- `src/core/amiga/stdio_latin1.c` plus `-Wl,--wrap=...` in `src/CMakeLists.txt`: libnix path calls convert UTF-8 to Latin-1.
  ASCII paths call straight through.
- `__wrap_fopen` with a converted path uses `open` + `fdopen`: libnix `fopen` returns ENOENT for a path with a byte >= 0x80
  without ever calling dos.library Open (vamos log shows no Open), while `open` with the same bytes succeeds. The cause
  inside libnix is UNEXPLAINED (no libnix source here); the workaround is measured, not understood.

## Entry points (count 1: grep of dos.library calls in src/core/amiga, line numbers after the fix)
- `dir.c`: Lock 14 (sel_dir_each), Examine 23, ExNext 25 (name converted at 28), Lock 43 + Examine 52 (sel_dir_mtime),
  Lock 63 + Examine 72 (sel_file_stat), SetFileDate 91 (sel_file_touch).
- `config_mkdir.c`: CreateDir 19, Lock 26.
- `dos_quiet.c`: Lock 38 (sel_amiga_name_exists).
- `machine.c:79` Lock of a constant ASCII `DEVS:` name: no conversion needed. `term.c` uses Read(Input()): not a path.
- Absent in the whole tree: LoadSeg, GetVar, SetVar, DeleteFile, Rename, SetProtection, MatchFirst, ExAll, dlopen.

Count 2 (from the structure that routes file access): (a) `src/library/dir.h` API (4 functions, all in dir.c),
(b) `sel_config_mkdir` (engine.h), (c) `sel_amiga_name_exists` (dos_quiet.h), (d) libnix stdio/libc path calls
(engine, library, sources, plugins, selecta-cli, sqlite via libnix): fopen, open, remove, rename, unlink, mkdir, rmdir,
stat, access, lstat, utimes, readlink, getcwd. Cross-check: `nm build/amiga/apps/selecta-cli/selecta-cli | grep ___wrap_`
lists 13 symbols = 13 `--wrap` options = 13 wrappers in `stdio_latin1.c` (lines 43..161). sqlite3.c.obj `nm -u`
references access, fchmod, fstat, getcwd, lstat, mkdir, open, readlink, stat, unlink, utimes; fchmod and fstat take no
path. `db/amiga_vfs.c` overrides only xFullPathname. Not converted: the `readlink` result (link target).

## Tests
- `latin1-unit` (`tests/library/latin1_test.c`): round trip 0xA0..0xFF, ASCII unchanged, U+0100 / euro / U+1F3B5 fail,
  malformed and overlong fail, capacity.
- `amiga-latin1-name` (`tests/amiga/run_latin1_name.cmake`): vamos runs `selecta-cli library browse <dir> --names`
  (new option, `apps/selecta-cli/library.c`). Expects `entry=Björk Ärger.mod` as UTF-8 and `open=... ok`.
- vamos and names: vamos exposes a host name to the 68k as Latin-1 bytes (0xF6) and maps Latin-1 paths back to the host
  name. macOS APFS refuses to create a Latin-1 named file ("Illegal byte sequence"), so the fixture is the UTF-8
  host name; the 68k binary sees the same Latin-1 spelling a real volume holds. A real-Amiga check is still open (rig).
- Also run, passing: amiga-config-dir, amiga-library-browse-vamos, amiga-build, hippo-publish, source-hippo.

## Revert check
Reverted `dir.c` to HEAD and made `sel_dos_path` a plain copy: `amiga-latin1-name` FAILED (entry arrived as
Latin-1 bytes `Bj\xf6rk \xc4rger.mod`). Restored: passes. The unit test cannot build without `latin1.c`, so no
separate mutation was done for it.

## Build
`build/amiga` and `build/host`: 0 warnings outside third_party. `selecta-cli commands --unimplemented | wc -l` = 8.
