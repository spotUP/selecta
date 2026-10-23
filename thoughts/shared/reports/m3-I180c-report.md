# I180c report: E6x/EEx in the time walk, seek before play (worktree /Users/spot/Code/devilbox-selecta, commit 4d76f7b5c, not pushed)

## Changes
1. src/lib/tracker/timeToPosition.ts: songPositionAtMs now mirrors TrackerReplayer.processRowForSynths + advanceRow: per-channel E6x loops (E60 start row, E6n loops; shared pBreakPos/pBreakFlag as in the replayer), EEx pattern delay only when xmBreakRows (XM; replayer ignores EEx for MOD), same advance order (row++, delay, E6x, order change). Delay repeats do not re-read row effects (the replayer's "repeat, no new notes" intent; re-reading would loop forever).
   Replayer semantics kept: E6n gives n jumps (n+1 passes); EEx with x=2 repeats the row once (row lasts 2 row-times, pattDelTime2 decrement).
2. src/lib/song/startPlayback.ts (new): seek, then setIsLooping, then transport.play(). openSong gets option startAtMs and result field seeked; uses startPlayback. selectaLink passes startAtMs and only seeks after the start when seeked is false (C64 SID JSIDPlay2: engine exists only once playing). Tracker/libopenmpt start from the replayer songPos (TrackerReplayer.ts:2470/2546 initialSongPos), and a stopped seekTo sets it (TrackerReplayer.ts:~3960), so position 0 is not heard.

## Tests (nice -n 19 vitest, 4 files, 16 pass; npm run type-check 0 errors)
- timeToPosition.test.ts: "honours E6x pattern loops" (E60 row1, E62 row3: 360ms row3, 480ms row1, 1080ms row3, 1200ms row4); "honours EEx pattern delay on XM songs and ignores it for MOD" (359ms row1, 360ms row2; MOD 360ms row3).
- startPlaybackSeeksFirst.test.ts: call order [seek:30000, loop:false, play]; seeked=false passthrough; no seek without offset.
- Revert check: old walker -> both new timeToPosition tests fail; play-then-seek order in startPlayback -> order test fails; restored, all pass.

## Caveats
- Mocked call-order only; not driven against a real replayer/audio (happy-dom has no AudioContext). Manual check: http://localhost:5174/?song=modland%2F...&t=30000 with a real XM/MOD.
- seekTo does not restore speed/BPM in force at the target row (Fxx earlier in the song): existing gap, not in scope. Groove/Furnace speed2 still not simulated.
- SID JSIDPlay2 still seeks after start (audible start possible).

## Native seek/position API per out-of-scope engine (file:line)
- UADE (src/engine/uade/UADEEngine.ts): NO. Outbound messages are play/stop/pause/setSubsong/setLooping etc. (:824-:858); 'position' (:425) is inbound reporting only. (nuade, NuadeEngine.ts:262 seek(frame) -> nuadeDriver.ts:66 _nw_seek: YES, frame-based, not wired to seekPlaybackToMs; follow-up candidate.)
- Hively (src/engine/hively/HivelyEngine.ts): NO. Messages :224-:459 have initSubsong/play/stop, no seek; 'position' (:193) inbound.
- MusicLine (src/engine/musicline/MusicLineEngine.ts): NO. Messages init/load/play/pause/stop/setMuteMask (:168-:235); 'position' (:137) inbound.
- JamCracker (src/engine/jamcracker/JamCrackerEngine.ts): NO. Messages init/loadTune/play/pause/stop/mute (:167-:310); 'position' (:121) inbound.
- Other SID engines (jssid/websid/tinyrsid/websidplay): NO (per I180b report; not re-checked).
