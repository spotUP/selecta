---
date: 2026-10-24
topic: Song end and seek for hardware-exclusive (HW) replayers on the Amiga (ledger I219b)
tags: [hw-kind, kplayer, song-end, seek, eagleplayer, I219b]
status: final
---

# HW song end and seek

Read-only; nothing built or run. Design doc: `thoughts/shared/plans/2026-10-22-architecture-design.md:22,220,265` (HW kind). Report: `thoughts/shared/reports/m3-I219-report.md` "Known gaps".

## 1. Plugin and engine paths

- KPlayer plugin: `plugins/replayers/kplayer/adapter.c` (177 lines). Hooks at 157-168: OPEN/CLOSE/START, `SEL_RP_HW_PAUSE`, `SEL_RP_HW_RELEASE`; no TICK, no RENDER, no POLL. `rp_start` (116-132) memsets the 2000-byte work area `s->work` (SEL_MEM_SAMPLE) and calls `kp_call(KP_INIT, 0,1,1, work, module)`. The work area is plain memory owned by the plugin, so the C side can read it directly.
- Hook ABI: `include/selecta/replayer.h:121-126`, tags `include/selecta/tags.def:67-71`.
- Engine HW start branch: `src/engine/engine.c:1228-1240`; HW flag/hook lookup 1795-1797. Render of a HW slot is silence only: engine.c:5220-5225 (`while (sl->hw ...)` memset, `song_frames += got`, `policy_chunk`). `hw_clock` (engine.c:4731-4747, called from `sel_engine_pump` at 4756 when `handed_over`) runs `sel_engine_render` by wall clock.
- Normal replayers: the engine calls `rp_tick` and reads `r.flags` (`SEL_TICK_LOOPED=1`, `SEL_TICK_ENDED=2`, replayer.h:18). Render path: engine.c:5244-5265: ENDED sets `last_tick` and `pol.reason = SEL_END_END`; LOOPED calls `policy_loop` (5148-5160: counts passes up to `e->loops`, then fade or `last_tick`). When `frames_left` is 0 and `last_tick`, `sl->ended = 1` (5268-5272). nuade sets the flags at `plugins/replayers/nuade/adapter.c:226`.
- HW slots never tick, so nothing sets ENDED/LOOPED: only the `max_song_ms` cap in `policy_chunk` (engine.c:5107-5123) ends them. Also `prepare early` is skipped for HW (engine.c:2373).

## 2. KPlayer song-end flag

- Symbol `k_songover` (byte), `third_party/kplayer/kpl14.s:182`, also `kpl_offsets.S:74`. Offset into the work area (rs layout, kpl14.s:158-209): counter 0, speed 1, posjumpflag 2, pbreakflag 3, pattdeltime 4, pattdeltime2 5, songpos 6 (word), pbreakpos 8, patternpos 10, dma 12, mastervolume 14, songdataptr 16 (long), fast 20, timingmode 21, filterstore 22, trig 23, tempo 24, usetempo 25, usertrig 26, sysint 27, playmusic 28, **songover 29**. Derived by hand from the rs list: verify against the vasm listing (`-L`) or a one-line test that reads work[29] after forcing a wrap before relying on it.
- Written by the CIA-interrupt music routine (68k), `addq.b #1,k_songover(a5)`, in three places:
  - kpl14.s:1539-1545: advancing `k_songpos`; when `songpos >= song length byte (-2(songdata))` it wraps to 0 and increments `k_songover` ("Kappale loppui!"). This is the normal end, and it is a wrap to position 0 with the song looping on.
  - kpl14.s:2035-2040 (Bxx jumping to the same position, "this is a songend") and 2043-2050 (Bxx/jump from the last position).
- Semantics: a counter ("played n times"), not a latched flag; it is incremented at the moment the last position is left, the song keeps playing from position 0. Cleared only by the work-area memset in `rp_start`. A byte read is atomic against the interrupt. Equivalent to `SEL_TICK_LOOPED` (a loop), not an explicit stop. `k_songpos` (word at 6) also gives the order for a POSITION event.
- The plugin has the pointer: `s->work`. Reading `s->work[29]` needs no new 68k code and no change to the unmodified third_party file.

## 3. Other HW-exclusive players

- None exists yet in the tree: HW kind is used only by kplayer (`grep SEL_RP_KIND_HW plugins src include`: kplayer/adapter.c, engine.c, replayer.h). The "native UADE glue as HW plugins" (design doc line 22) is M4 and unwritten. nuade players are VOICE kind and report end through ticks.
- Eagleplayer/DeliTracker has a common callback: `dtg_SongEnd` (function pointer in `DeliTrackerGlobals`, `thoughts/shared/research/2026-10-22_eagleplayer-architecture.md:25`; players call it via `EP_SongEnd`-style hook; HippoPlayer-68k examples: `~/Code/HippoPlayer-68k/code/pl/Benn Daglishv3.s:1091`, `future_player.s:223-554`). A native-player glue can install its own `dtg_SongEnd` routine that sets a flag byte. KPlayer is not a DeliTracker player, so it has no such callback; it uses `k_songover`.
- Common abstraction that fits both: the plugin owns a byte/counter, the engine polls it (below).

## 4. Seek

- Today: `cmd_SEEK` (engine.c:1974-2009) -> `reopen_current(SEL_BEGIN_SEEK)` -> `prepare_slot(..., skip_ms)`; engine.c:1836-1849 tries `rp_seek` (not set for kplayer) then `skip_frames`, which returns 0 for HW (engine.c:1443-1445). Net effect: the HW song restarts from the start (kp_init) and SEEKED reports the target while the hardware plays from 0 (unverified in a test; read from code only).
- KPlayer can reposition: `k_songpos` (word at 6) and `k_patternpos` (word at 10) are plain variables the routine re-reads each row (kpl14.s:1199-1206 `k_read` indexes by songpos/patternpos; 1525-1545 handles the row advance and `k_posjumpflag`). The routine itself uses the same writes for Bxx/Dxx (2055-2065). But there is no ms-to-order map: the routine has no time model. A ms seek needs the engine/plugin to walk the pattern table with speed/tempo (Fxx) simulation, or to accept seek-by-order only. Writing while the CIA interrupt runs is racy (word writes are atomic, but a pattern-break flag in flight can clash): do it with the clock stopped (`kp_playstop`, entry 20) and silence via `kp_clear`/`kp_playstop`, then resume.

## 5. Recommended minimal design

Song end (cost: about 25 lines, no 68k changes):
1. New optional hook `SEL_RP_HW_POLL` (`uint32_t poll(void* state)` returning `SEL_TICK_*` flags, 0 if none), in tags.def/replayer.h next to HW_PAUSE.
2. kplayer implements it: remember `last = work[29]` at start; each poll returns `SEL_TICK_LOOPED` when `work[29] != last` (update `last`). Plugin level, polled from the engine; no interrupt flag needed, the 68k routine already writes the counter.
3. Engine: in the HW silence loop (engine.c:5220), call the hook once per render chunk and feed the flags into the existing path: LOOPED -> `policy_loop`, ENDED -> `last_tick`/`SEL_END_END` (same code as 5261-5264, factor it into one helper used by both loops). Then loops/fade/REPEAT/queue advance work as for every replayer, and `max_song_ms` stays only the safety cap. Poll granularity = the render chunk of `hw_clock` (256 frames) is far finer than needed.
4. Gapless early prepare for HW stays disabled (engine.c:2373); I219d "cut short" gap is a separate item.
5. Future glue players: install `dtg_SongEnd` writing a flag byte and return it from the same poll hook.

Seek (cost: larger, separate row): implement `RP_SEEK`-like HW hook only after the poll exists; order-granular seek first (set `k_songpos`, `k_patternpos`=0 with the clock paused), ms-to-order via a pattern walk that honours Fxx. Engine already tries `rp_seek` before `skip_frames` for any kind (engine.c:1843), so giving kplayer `SEL_RP_SEEK` needs no engine change except making `prepare_slot` allow it for the HW start branch (the HW branch at 1230 returns before the skip code at 1839: check that ordering, unverified). Needs a rig run; vamos cannot prove it (no CIA resource, per the I219 report).

Test: song-end reachability test must drive `selecta` top level with a short synthetic M.K. module and a stubbed work area where the 68k routine cannot run under vamos: use a host-side fake `HW_POLL` plugin (kind HW) returning LOOPED, assert song ends before `max_song_ms` and the queue advances; real KPlayer counter proof needs the rig.
