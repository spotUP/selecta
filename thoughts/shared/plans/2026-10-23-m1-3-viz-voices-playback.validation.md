---
date: 2026-10-24
topic: M1.3 validation (T39 to T48 and T8)
tags: [validation, m1.3, viz, voices, playback, export, crossfeed]
status: in-progress
plan: thoughts/shared/plans/2026-10-23-m1-3-viz-voices-playback.md
---

# M1.3 validation

## Full run (landing M1.3)

Reason for the full run: landing the milestone (plan Verification). Not re-run for this file; evidence is the log of the run at HEAD 0cf158d plus doc-only commits: `100% tests passed out of 85`, 44.63 s (scratchpad `ctest103.log`, grep-checked per test below). Amiga build green at I103 (ledger, merge 0cf158d); not rebuilt for this file.

## Per test

"Sentinel" is the counter or line that proves the new code ran. Numbers are what the test asserts per the plan table, as built.

| Test | ctest name | Result (log) | Sentinel | Asserted numbers |
|------|-----------|--------------|----------|------------------|
| T39 | selecta-m13-reach (#64) | Passed | exit line `viz_frames= scope_reads= pattern_rows= mutes= dsp=crossfeed:on fades= sleep_armed=1`; V, S, P > 0, M = 1 | last screen equals `tests/m13/reach.screen`. DEVIATION: scope view with voice 1 blank, not scope + pattern on one screen (see Deviations); `fades=` parsed, not asserted |
| T40 | viz-taps (#68) | Passed | `viz_blocks=N` | 100 blocks for 2 s; dump equals `tests/m13/viz.expected` byte for byte; VU decay 1/8 per block. Extra: viz-vu (#66), viz-spectrum (#67) Passed |
| T41 | pattern-cells (#69) | Passed | `@cells` lines in `tests/m13/pattern.expected` | channel 0 rows 0..3 equal gen_testmod notes; nuade on `selecta-test.mod`, openmpt on `selecta-test-mk.mod`; both on one note scale (I89a) |
| T42 | voice-control (#70) | Passed | `mute_mask=0x...`, `pitch_factor_q16=131072` | MUTE 1 equals golden `tests/golden/nuade/selecta-test-mute1.sha256`; SOLO 2 mask corrected to 0x0d (SOLO 3 is 0x0b, plan typo); SOLO 0 clears; PITCH 1200 doubles step / openmpt factor 2.0; TEMPO scales ticks; PITCH 2000 gives `ERROR range PITCH`. openmpt mute is checked as "differs from unmuted", not a golden |
| T43 | loop-fade (#73) | Passed | `SONG_ENDED reason=` in the `.expected` output | reasons end, loop, max_length, silence; REPEAT one, all; stop-after-current and stop-after-queue (`loop_stop_*.sel`); fade midpoint gain 32768. `loop_end` uses `selecta-test-mk.mod` (libopenmpt), not `selecta-short.mod` |
| T44 | crossfade (#74) | Passed | `STATS xfade_frames= slots_mixed= mixed_path=` | overlap frames 88200, slots_mixed 2; xfade table gain; gapless total frames equal sum; `crossfade_unsupported.sel`: `ERROR unsupported_path OPTION` on voices-test |
| T45 | sleep-alarm (#75) | Passed | `.expected` lines; `timer_test.c` ramp unit | sleep ramp 10000 ms then `STATE paused`, volume restored; cancel; ALARM 07:30 fake clock then playing, ramp 0 to volume over 30000 ms; `ALARM off`; `ERROR range ALARM` |
| T46 | sample-export (#77) | Passed | `SAMPLE n len loop_start loop_len rate name` lines | openmpt path only: counts, lengths, loop points equal gen_testmod; WAV mono 8-bit 8287 Hz; 8SVX FORM/VHDR/BODY; SHA-256 goldens `tests/golden/samples/samples-1.wav, samples-1.8svx, samples-4.8svx` |
| T47 | crossfeed-unit (#71), crossfeed-render (#72) | Both Passed | render prints `dsp=crossfeed blocks=N` | 14-frame delay, 700 Hz one-pole, mono sum preserved, `ERROR unsupported_path DSP`. GAP: `tests/golden/dsp/crossfeed.sha256` does not exist; render_check.cmake only prints the hash when the file is absent, so the golden assertion is NOT enforced |
| T48 | phase-tap (#65) | Passed | unit test, no CLI sentinel | 1024-frame ring; diagonal / anti-diagonal; correlation +32767 / -32767 / |c| < 2000 |
| T8 | ratchet (#10) | Passed | `commands --unimplemented` count equals `tests/ratchet/unimplemented.txt` | count 21 (plan <= 21) |
| T1..T38 | rest of the 85 | Passed | n/a | 85 of 85 passed |
| T26 | no-alloc, no-alloc-render, no-alloc-render-queue, no-alloc-render-openmpt (#78..#81) | Passed | none found for DSP on | Plan asks "T26 holds with the DSP on"; I found no no-alloc test that enables the DSP (grep of tests/CMakeLists.txt). Unverified |

## Plan items

M1.3 plan items counted: T39 to T48 (10), T8 ratchet, T1..T38 still green, amiga build green = 13.

| Item | Status |
|------|--------|
| T39, T40, T41, T42, T43, T44, T45, T48 | verified (T39 with deviation) |
| T8 ratchet 21 | verified |
| T1..T38 green (85 of 85) | verified |
| build/amiga green | verified at I103 (ledger), not rebuilt now |
| T46 sample-export | OPEN in part: nuade sample scan (RP_SAMPLE_GET on a nuade-played module) has no test (phase 5a report) |
| T47 crossfeed | OPEN in part: golden hash not pinned, T26 with DSP on has no test |

Verdict items: 11 verified, 2 partial (not counted).

## Knobs

| Knob | Moved by a test? |
|------|------------------|
| MUTE, SOLO | yes, T42 (nuade golden; openmpt differs-from-unmuted) |
| PITCH, TEMPO | yes, T42 |
| REPEAT off / one / all | yes, T43 (`loop_*.sel`) |
| OPTION loops, fade_ms, max_song_ms, silence_ms | yes, T43 |
| stop-after-current, stop-after-queue | yes, T43 |
| OPTION crossfade (0 and 2000) | yes, T44 |
| SLEEP_TIMER, ALARM | yes, T45 |
| SAMPLE_LIST, SAMPLE_EXPORT (wav, 8svx) | yes, T46 |
| DSP crossfeed on/off | yes, T47 render differs from plain; T39 sets it |
| viz mode (V cycles), pattern panel (P) | yes, panel screen fixtures plus T39 |
| Pitch keys `}` `{`, tempo keys `>` `<`, `S` solo, `0` clear | no key-level test found; verbs are covered by T42. No written exemption: OPEN |
| Note dots, phase view panel | not implemented in the UI (phase ring and correlation exist, T48 unit only). No written exemption: OPEN |
| Spectrum panel golden | none (phase 6a report); data covered by viz-spectrum, viz-taps |

## Deviations (from plan Deviations section and phase reports)

- T39: panels are mutually exclusive; final screen is scope view, pattern rows proven by counter only; `Muted` drawn in levels mode only; `fades=` not asserted.
- T41: nuade on `selecta-test.mod`, openmpt on `selecta-test-mk.mod`.
- T42: SOLO 2 mask 0x0d.
- T43: end test uses libopenmpt module; nuade Protracker never reports ENDED; no duration used (Q20).
- T8 ratchet stays 21 (SLEEP_TIMER and ALARM already implemented).
- Phase 1: `--viz-dump` needs `--output`; viz blocks close per render call; VU release `level -= max(1, level >> 3)`.
- Phase 4c defaults: alarm inside its own minute fires at once (Q32); sleep expiry while not playing dropped; alarm ramp from 0.
- Phase 5b: DSP verb emits no ack; hash golden unpinned.
- libopenmpt: `sample_table.cpp` mirrors `struct openmpt_module` of 0.8.4; re-check on upgrade.

## Manual checks open for Johan

1. Listening, voice mute golden (LOCAL): `cd ~/Code/MusicPlayer && build/host/apps/selecta/selecta --start-dir tests/songs`, play `selecta-test.mod`, press `1`. PASS: voice 1 audibly drops out, the rest keeps playing, the voice is marked muted; press `1` again, it returns.
2. Listening, crossfeed (LOCAL, headphones): play a hard-panned 4-channel .mod, `:DSP crossfeed on`. PASS: channels sound less separated, no clicks, no volume jump. If good, pin it: `build/host/tools/../../apps/selecta/selecta-cli render tests/songs/selecta-test.mod --dsp crossfeed --seconds 2 --output /tmp/x.wav` is what the test runs; copy the hash printed by `ctest --test-dir build/host -R crossfeed-render -V` into `tests/golden/dsp/crossfeed.sha256`.
3. Viz in a real terminal (LOCAL, macOS Terminal): `build/host/apps/selecta/selecta --start-dir ~/Music/mods`, play a 4-channel .mod. PASS: `V` cycles Levels, Scope, Spectrum, bars and scopes move with the music; `P` scrolls the pattern in time; `}` raises pitch 100 cents, `{` lowers; `>` `<` change tempo.
4. Crossfade: `:OPTION crossfade 3000`, queue two songs, press `n`. PASS: a 3 s blend, no gap, no click.
5. Alarm fade in: pause, `:ALARM HH:MM` for one minute ahead. PASS: playback starts at the minute and fades from silence to the set volume over 30 s.
6. Sleep timer fade out: `:SLEEP_TIMER 1`. PASS: after one minute the volume ramps down over 10 s, state paused, volume restored on next play.
7. macOS Control Center (carried from M1.2): play and pause from Control Center; the shell follows.

## Open

- T46 nuade sample scan has no test; T47 golden unpinned; T26 with DSP on has no test.
- Linux MPRIS (M1.2, I81) still unverified.
- Note dots and phase view are not in the text UI.

## Verdict

M1.3: 11 of 13 plan items verified, 7 manual open.
