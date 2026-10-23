# M3 Phase 4b report (I158a done, I159 partly done)

Branch worktree-agent-a0906c5e55c49c4b3.

## Done
- I158a: LIBRARY_SCAN runs on cfg.library; the library_scan/library_ctx hook is gone. New
  sel_engine_scan_info() returns the counters. T71 (source-local) asserts `library search source:local`
  hits from scanned files. Host tests green.
- I159 (host): zip via vendored miniz (third_party/miniz), lha/lzh via vendored lhasa
  (third_party/lhasa), both read from memory (src/library/sources/archive.c). The local scan treats an
  archive as a folder: members become songs at `<archive>/<member>`, counters `archives` and
  `archive_members`. T71 archive part: test `source-archive` (tests/m3/source_archive.sh,
  make_archives.py) builds a zip, an lzh and a broken zip, asserts counters and search keys. Green.
- Amiga build (build/amiga) compiles with the archive library.

## Tests
source-local, source-archive, library-search, view-library-source, link-warnings: all pass.

## Not done / blocked
- vamos scan test (tests/m3/scan_vamos.cmake) is written and registered but DISABLED. Cause:
  sel_library_open crashes under vamos (InvalidMemoryAccess); `selecta-cli library stats` crashes the
  same way, so the Amiga SQLite open is broken independent of this work. Needs its own fix (row to add).
- unlzx (lzx) and ancient (xpk, PowerPacker): not ported.
- xad/xfd on Amiga: no xad headers in the toolchain, not implemented. Not testable under vamos.
- Browsing INTO an archive in LIBRARY_BROWSE / sel_browse_list (archives as folders in the browser): scan only.
- Rescan and watcher (I160), Modland listing (I162).

## Open questions
1. Archive libraries. Default: miniz + lhasa vendored from the libopenmpt tree, portable C also on 68k.
   Alternative: temisu `ancient` for every format it covers (adds lzx, xpk, PowerPacker in one library).
2. Amiga xad/xfd. Default: portable path on Amiga too. Alternative: xadmaster.library adapter in src/core/amiga/.
3. sel_source.parse takes a scratch buffer (differs from the design doc), carried over from 4a.
