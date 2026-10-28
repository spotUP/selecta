# Song length database (I605, feature F1)

Problem: the seek bar never fills and the length text stays empty for ProTracker and other UADE formats, because
`song.duration_ms` (used by `progress_q16` in `src/uimodel/uimodel.c`) came only from player metadata
`SEL_META_DURATION_MS`, and kplayer and nuade publish none. Decision (Johan): every UADE format takes its length
from the audacious song length database; kplayer additionally computes its own.

## Order of sources (engine, `refresh_snapshot`)

1. Player metadata `SEL_META_DURATION_MS` (kplayer now publishes one, see below; libopenmpt and others already do).
2. The song length database, `songlen_db_ms()` in `src/engine/engine.c`: key read once per song with
   `sel_uade_key_file(path)`, lookup once per subsong, result cached in the slot.
3. 0 = unknown (as before).

## kplayer length

`kp_walk_length()` in `plugins/replayers/kplayer/modwalk.c` reuses the order/row walker that already finds a
position for seeking (`kp_walk_find`), runs it to the song end (last order reached, or a Bxx jump back, which ends
the song) and returns the end time in ms, honouring Fxx speed and tempo. The adapter calls it in `rp_open` and
publishes the value through a new `SEL_RP_METADATA` hook. Test: `kplayer-modwalk` (plain orders 15360 ms, Bxx
case 9000 ms, F03 case 7680 ms, malformed module = not found). Only the AGA build compiles the adapter; its published
value on the rig is unverified.

## File format `songlengths.bin` (big-endian)

    header   16 bytes: "SLDB", u32 version = 1, u32 entry count, u32 byte offset of the length array
    entries  12 bytes each, sorted by key: 6 key bytes (the 12 hex digits of sel_uade_key), u8 first subsong,
             u8 subsong count, u32 index of the entry's first length in the length array
    lengths  u32 each, milliseconds, 0 = unknown

Built by `tools/songlen_bake.c` (`sel_songlen_bake` in `src/library/songlen_bake.c`) from the audacious-uade-tools
file `tsv/pretty/xxh32/songlengths.tsv` (row: `key TAB first_subsong TAB "ms,flags" ...`). Flags are dropped.
Duplicate keys: first row wins. Read by `src/library/songlen.c` with a binary search over fseek/fread; the file is
never loaded whole.

## Measured (real TSV, 476328 rows)

- Bake: 0.44 s, 476328 entries, 7 802 356 bytes (about 5.7 MB entries plus 2.1 MB lengths).
- Lookup: 20 fread calls per lookup (header + about 19 probes + the length), 27 us warm cache on the Mac.
  On the Amiga each fread is a small seek+read; about 20 small reads once per subsong, RAM cost a few hundred bytes.
- Disk: 7.8 MB. Not committed (generated); the TSV is not in the repo either.

## Build and install

- Mac/host: `-DSELECTA_SONGLENGTHS_TSV=<.../songlengths.tsv>` makes the `songlengths` target write
  `<build>/songlengths.bin`. Without it only a status message is printed and every lookup finds nothing.
- Install: copy the file to `<config dir>/songlengths.bin` (the same directory as `prefs.conf`; on the Amiga
  `ENVARC:Selecta` when the ENVARC: assign exists, else `PROGDIR:Selecta`, as `sel_config_dir` resolves). The Amiga cross build does not bake;
  bake on the host and copy the file to the Amiga.
- The engine reads `cfg.config_dir`; with no file, no key or no hit the duration stays 0.

## Known limits

- The key comes from `sel_uade_key_file(sl->path)`, so only plain local files match. Packed files, archive members
  and streams get no database length.
- The database has per-subsong lengths; the slot subsong index is matched against the row's first subsong.

## Tests

- `songlen-bake-lookup` (`tests/songlen_unit_test.c`): the real `songlen_bake` tool on a TSV fixture; hit, miss,
  subsong numbering, unknown length, missing file, read count, duplicates.
- `engine-song-length-from-database` (`tests/engine/songlen_engine_test.c`): reachability through QUEUE_ADD/PLAY;
  `sel_engine_state()->song.duration_ms` equals the database value for a fake replayer without a duration.
  Fails with the engine hunk disabled (`duration_of(...) == 187654u`), passes with it.
- `kplayer-modwalk`: `kp_walk_length` cases.
