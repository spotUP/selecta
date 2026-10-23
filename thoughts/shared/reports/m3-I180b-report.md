# I180b report: Selecta link time seek in DEViLBOX (worktree /Users/spot/Code/devilbox-selecta, commit 824bb6432, not pushed)

Design: D-M3-11. Link `?song=..&sub=..&t=<ms>` now seeks after openSong.

## Entry point
`seekPlaybackToMs(ms): boolean` in src/engine/replayer/seekPlaybackToMs.ts. Called from src/lib/share/selectaLink.ts (after openSong ok and started) via dynamic import.
hvsc links now pass origin 'hvsc' (was always 'modland').

## Seek per replayer family
- Tracker replayer (engine 'tracker' per playingEngineFromStores): YES. TrackerReplayer.seekToMs (src/engine/TrackerReplayer.ts:~1006) converts ms to (songPos, row) with songPositionAtMs (src/lib/tracker/timeToPosition.ts), then the existing seekTo(songPos, pattPos) (TrackerReplayer.ts:~3867). Walker applies Fxx speed/tempo, Bxx, Dxx (BCD for MOD, hex for XM); jump to an order already played ends the walk; past the end holds the last row. Not simulated: E6x pattern loops, pattern delay, groove/Furnace speed2.
- libopenmpt playback: YES via the same path. Its worklet has no set_position_seconds bridge (only setOrderRow, LibopenmptEngine.ts:556), so ms is converted to order/row from the parsed song and seekTo forwards it. No WASM rebuild.
- C64 SID, JSIDPlay2 engine: YES, native C64SIDEngine.seek(seconds) (C64SIDEngine.ts:334). Other SID engines (jssid, websid, tinyrsid, websidplay): NO time seek, return false, play from start.
- UADE, Hively, MusicLine, JamCracker, other WASM engines in WASM_ENGINES: NO time seek exposed; seekPlaybackToMs returns false and playback starts at 0 (documented in the module comment).
- No existing duration/song-length walker existed (tickGridPosition is constant-speed only, no tempo), so the walker is new.

## Tests (nice -n 19 vitest, 2 files, 7 pass; type-check 0 errors)
- src/lib/share/__tests__/selectaLink.test.tsx: AppRouter with t=30000 calls seekPlaybackToMs once with 30000; hvsc link opens with origin 'hvsc' and no seek without t.
- src/lib/tracker/__tests__/timeToPosition.test.ts: speed change at row 2 (240 ms -> row 2, 300 ms -> row 3, 600 ms -> order 1 row 0), tempo change (420 ms -> row 4), past-end hold.

## Caveats
- The reachability test mocks seekPlaybackToMs and usePatternPlayback (AudioContext absent in happy-dom); the real seek chain is unit-tested only at the ms-to-row level, not driven against a live replayer. Needs a manual check at http://localhost:5174/?song=modland%2F...&t=30000 with a real MOD.
- Seek happens after play() starts, so a short audible start at 0 is possible before the jump.
- selectaLink hvsc downloads bytes (kind 'bytes'); origin hvsc start rule comes from START_BY_ORIGIN, start:'song' is still passed explicitly.
