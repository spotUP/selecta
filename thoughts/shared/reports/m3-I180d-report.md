# I180d report: native time seek per family

Branch `feat/selecta-native-seek` (stacked on `feat/selecta-links`), commit 7829d144d in /Users/spot/Code/devilbox-selecta. Not pushed.

## Per family

| Family | Seek mechanism | Status | Evidence |
|---|---|---|---|
| nuade | `_nw_seek` via worklet `seek` message; `NuadeEngine.seekToMs`; `seekNativeEngineToMs('Nuade', ms)`; pending start offset consumed before `instance.play()` in `startNativeEngines` | wired | Nuade.worklet.js:100, NuadeEngine.ts, NativeEngineRouting.ts |
| UADE | none: `UADE.js` exports only `_uade_wasm_*`, no seek/position | gap | export list |
| Hively | none: `_hively_*`, no seek | gap | export list |
| MusicLine | none: only libc `_seek` | gap | export list |
| JamCracker | none | gap | export list |
| SID websid / websidplay | `ScriptNodePlayerEngine.seek` -> `seekPlaybackPosition(ms)` -> `_emu_seek_position` | wired (after start) | engine test |
| SID JSIDPlay2 | `seek` only logged and set `currentTime` (I180c claim was wrong); now reports false | gap | JSIDPlay2Engine.ts:291 |
| SID jssid, tinyrsid | no seek export | gap | |
| Tracker | `seekToMs` now restores speed and BPM from the time walk (`songStateAtMs`) | wired | test |

## Tests
- src/engine/replayer/__tests__/seekPlaybackToMsNativeFamilies.test.ts
- src/engine/replayer/__tests__/nativeSeekEngines.test.ts
- src/engine/replayer/__tests__/seekToMsRestoresSpeedAndBpm.test.ts
- src/lib/tracker/__tests__/timeToPosition.test.ts (songStateAtMs)
Pre-commit hook: 19 files, 121 tests passed.

## Revert checks (each fails without the change, passes with it)
- seekToMs speed/BPM lines removed: speed test fails.
- seekPlaybackToMs nuade branch removed: nuade family test fails.
- ScriptNodePlayerEngine.seek reverted: 4 tests fail.
- NuadeEngine.seekToMs reverted: 1 test fails.
All restored; tree clean.

## Type-check
`tsc -b --force`: 0 errors.

## Open items
- UADE, Hively, MusicLine, JamCracker, jssid, tinyrsid, JSIDPlay2 have no native seek. Needs worklet/WASM work (for example render-and-discard).
- Nuade pending-start path and websid ms units not verified against real audio (no AudioContext in happy-dom).
- Generic `TrackerReplayer.seekTo(songPos,row)` still does not restore speed/BPM; only `seekToMs` does.
- Furnace/groove speed2 not simulated in the walker.
- SID engine exists only once playing, so SID still seeks after start.
- No end-to-end test through `startNativeEngines` for the nuade pending seek.
