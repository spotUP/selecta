---
row: I218d2
date: 2026-10-24
status: no defect in Selecta found; byte guard added; hardware check open
---

# I218d2: o-umlaut and A-umlaut draw as box glyphs on the Replay console

Design doc section 7 (text UI). Follows I218d (Latin-1 names at the AmigaDOS boundary).

## Verdict

Selecta sends the console the correct Latin-1 bytes for the row: `42 6a f6 72 6b 20 c4 72 67 65 72 2e 6d 6f 64`,
identical to the on-disk name. None of the four candidate causes holds. One box glyph per letter is what the
console draws for a single byte the window's font has no glyph for, so the remaining layer is the console font of
the Shell window on the Replay (or a binary on the Replay older than I218d's merge). This could not be measured from
here (no rig access). No code fix was made because no Selecta layer emits a wrong byte.

## Evidence

1. Cells (candidates a, d). The Amiga `Selecta` under vamos (vamos gives the 68k the Latin-1 name bytes, as an
   AmigaDOS volume holds them), `--start-dir work:songs --screen-dump`: the list row cells are U+00F6 and U+00C4
   (dump `20 20 42 6a c3 b6 72 6b 20 c3 84 72 67 65 72 2e 6d 6f 64`, the dump writes cells as UTF-8). So the list
   comes through `sel_dir_each` conversion; the only directory iterators in src are `src/library/local.c:245` and
   `src/library/cache.c:381`, both call `sel_dir_each`. A raw Latin-1 name would give U+FFFD and draw `?` (the
   pre-I218d symptom), not a box.
2. Console bytes (candidates b, d). New `--frame-dump <file>` option on the text UI appends every flushed frame's
   bytes exactly as handed to `txt_term_write`. Under vamos the first frame holds
   `1b 5b 35 3b 31 48 1b 5b 30 3b 33 37 6d 20 20 20 42 6a f6 72 6b 20 c4 72 67 65 72 2e 6d 6f 64 20 ...`
   (CSI 5;1H, CSI 0;37m, then the name as one byte per letter). `grid.latin1` is set (`text_ui.c`
   `u->grid.latin1 = txt_term_amiga_console`), `put_cp` writes `cp` as one byte, the console SGR path (I218g) only
   writes `CSI 0;3x;4x m`, and no UTF-8 lead byte `c3` appears in the row.
3. No second conversion (candidate c). `txt_term_write` (`plugins/ui/text/amiga/term.c:123`) is a direct
   dos.library `Write(Output(), ...)`; `src/core/amiga/stdio_latin1.c` wraps only path calls (fopen, open, remove,
   rename, unlink, mkdir, rmdir, stat, access, lstat, utimes, readlink, getcwd), no output call.
4. Real hardware differs from vamos only in `txt_term_open` / the size probe / `tui_resize`; `txt_grid_resize`
   keeps the `latin1` and `console` flags (only `txt_grid_init` clears them, called once before the flags are set).

## Guard added

- `plugins/ui/text/text_ui.c`: `--frame-dump <file>` (test hook, appends raw frame bytes).
- `tests/m2/latin1_row_bytes.cmake`, ctest `amiga-console-latin1-name-box-glyph-vamos`: the Amiga binary under
  vamos must send `42 6a f6 72 6b 20 c4 72 67`. Checked red: with `u->grid.latin1 = 0` (candidate d) the test fails;
  restored, it passes.
- Ran: amiga-console-latin1-name-box-glyph-vamos, amiga-latin1-name, grid-unit, amiga-console-first-frame-vamos,
  amiga-console-keys-vamos: all pass. Host and Amiga builds: no new warnings (only text_ui.c recompiled, 0 warning
  lines).

## Open: hardware check (Johan, on the Replay)

In the same Shell window Selecta runs in:
1. `List RAM:selecta` (or `Dir`). If the name shows boxes there too, the window font has no Latin-1 glyphs:
   set Prefs/Font "System Default Text" (the Shell font) to topaz 8 or another ISO-8859-1 font and retry Selecta.
   PASS: Selecta then draws "Björk Ärger.mod".
2. If `List` shows o-umlaut correctly but Selecta shows boxes, check the Selecta binary on the Replay is from
   b9c3dfc or later (`Version RAM:selecta/Selecta` or the file date), then run
   `RAM:selecta/Selecta --start-dir RAM:selecta --frame-dump RAM:f.bin`, quit, and send `RAM:f.bin`; it holds the
   exact bytes the console got.
