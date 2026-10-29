---
date: 2026-10-29
row: I695
status: findings, no fix committed
---

# I695: online search results play silent (Johan, Mac, 2026-10-29)

## Summary

No single engine bug found inside the agent budget. The evidence points at two
separate causes, both source-specific, plus one UI gap:

1. **SNDH has no replayer.** `plugins/replayers/` holds dr_flac, gme, kplayer,
   minimp3, nuade, openmpt, stb_vorbis and ym. None of them probes SNDH. Ledger
   row I427 (SNDH replayer, sc68 core) is still open. Every `sndh:` search
   result therefore ends in `open_failed`. Johan's cache shows he played SNDH
   rows in the I695 session: `~/.cache/selecta/files/sndh/Dubmood/*.sndh`
   fetched 22:51-22:52 (ICE!-packed, valid downloads), so "silent" for those
   rows is "no player", not a fetch or audio bug.
2. **Some fetches leave an empty directory and no file.** Johan's cache holds
   empty `sndh/Dubmood/DMA/` (22:53) and `modland/Protracker/Jester/` (22:21).
   Headless repro with real network reproduces the DMA case:
   `ERROR open_failed sndh:Dubmood/DMA/Too_Drunk_To_Fuck.sndh` and the DMA dir
   is created empty. SNDH fetches go through the year zip by HTTP range
   (`plugins/sources/sndh/sndh_net.c:57-79`: download.php page, then
   `files/sndh<year>_lf.zip` tail + central directory). Suspect: the central
   directory lookup for a file in a composer subfolder (`Dubmood/DMA/...`)
   misses. Not confirmed.
3. **Open failure is silent in the desktop UI.** `read_song` (src/engine/engine.c
   ~1597) returns NULL when `sel_library_fetch` (src/library/db/fetch.c:459)
   returns <= 0; the engine emits `open_failed`, but Johan reports silence, not
   an error. Check whether desktop_ui.c surfaces `open_failed` as a toast.

## What works (headless, real network, fresh db)

- `asma:Composers/Aki/Atari_Style.sap` fetches and opens (gme, "Atari XL").
- `modland:Protracker/Jester/elysium.mod` fetches (129430 bytes) and opens
  (openmpt), position moves.

So the modland empty `Jester/` dir in Johan's cache at 22:21 is not reproduced
headless. It may be a desktop-only path (Johan's library.db is user_version 12;
the worktree used here only migrates to V9 and refused his db, so the exact
desktop run could not be replayed).

## Repro

`selecta-cli script <file> --db <fresh.db>` with
`SELECTA_CACHE_DIR=<scratch>/cache`, script:

```
OPEN "sndh:Dubmood/DMA/Too_Drunk_To_Fuck.sndh"
PLAY
@render 44100
@position
OPEN "modland:Protracker/Jester/elysium.mod"
PLAY
@render 44100
@position
```

## Next steps (proposed rows)

- I695a: debug the SNDH zip central-directory lookup for subfolder paths with
  the net fixture server (tests/net/fixture_server.py), regression test on the
  `Dubmood/DMA` shape.
- I695b: desktop shows `open_failed` (toast or row state) instead of silence;
  SNDH rows keep the "no player yet" mark until I427 lands.
- I695c: Johan, Mac: which rows played silent? If any modland/asma row is
  silent with a file in cache, that is a separate output-path bug; capture the
  row key.
- `@render` prints no output level; a level/peak readout in selecta-cli would
  let the I695 regression test assert nonzero samples.
