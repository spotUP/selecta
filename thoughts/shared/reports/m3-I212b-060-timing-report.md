# I212b decode timing on real 68060 hardware (minimp3, stb_vorbis)

Date 2026-10-23. Product code unchanged. Same method and RTF definition as m3-I194-rig-timing-report.md.

## Result

Real-time factor (RTF) = wall time / audio duration; below 1.0 plays in real time.
Command: `selecta-cli render <clip> --output RAM:selecta-i212b/x.wav --seconds 2 --rate <hz>`, timed with AmigaDOS `Date` before and after
(1 s resolution, includes program load). 3 runs each, run in a visible shell window via `ami.py long`.
Clips regenerated with the I194 recipe (2.0 s, 44.1 kHz stereo, sine 440 Hz + pink noise; MP3 128 kbps, 33061 bytes; Ogg ffmpeg native `vorbis`, 55384 bytes).
Build: build/amiga from this worktree (68020 toolchain file, minimp3 with -m68881 per I213, stb_vorbis without, I213a open). Plugins are linked into selecta-cli (12.5 MB), nothing else copied.

| Format | CPU tier | Output rate | Wall time (3 runs) | RTF | 020 RTF (I194) |
|---|---|---|---|---|---|
| MP3 (minimp3) | 68060 | 22050 | 30, 29, 29 s | about 14.7 | about 69.5 |
| MP3 (minimp3) | 68060 | 44100 | 29, 29, 29 s | about 14.5 | about 74 |
| Ogg (stb_vorbis) | 68060 | 22050 | 35, 35, 34 s | about 17.4 | about 62 |
| Ogg (stb_vorbis) | 68060 | 44100 | 35, 35, 35 s | about 17.5 | about 61.5 |

Both decoders are still about 15 to 17 times too slow for real time on the 060. Speedup over the 020 emulator numbers: MP3 about 4.7x to 5x, Ogg about 3.5x.
Output rate barely matters (as on the 020). Runs are very stable (within 1 s).
The probe ratio is far higher (CPU_SPEED 4356 vs 183 = 24x), so decode does not scale with the probe.
Likely cause (inferred, not profiled): float work that the 060 FPU does not run natively (68060.library emulation traps) and/or soft-float paths
(stb_vorbis has no -m68881; minimp3 with -m68881 gained little). Unverified; no profile taken.
Note: render output reports `rendered_frames=44100` for the 22050 Hz runs (render_calls=87) and 88200 at 44100 Hz (render_calls=173); frames appear counted at 44.1 kHz in the 22050 case. Not investigated.

## Machine

`selecta-cli machine` on the A1200:

    probe_calls=1  clock.source=eclock  CPU=68060  CPU_SPEED=4356  FPU=1  FPU_SPEED=4356
    FAST_RAM=192937984  CHIP_RAM=2080768  GFX=AGA+RTG  AUDIO=paula  NET=unknown  DISK=unknown  TIER=rtg

`selecta-cli plugins`: null, voices-test, paula-shadow, paula, paula14-shadow, paula14, wav, crossfeed, nuade, minimp3, stb_vorbis, text
(float replayers minimp3 and stb_vorbis are visible; FPU=1 under 68060.library). Clock speed in MHz not reported by the probe.

## Blockers and notes

- None for the measurement. ENV: and ENVARC: were assigned (ENVARC -> System:Prefs/Env-Archive); no render hung.
- 1 s timer resolution: error about 3 percent on a 30 s run.
- ami.py has no plain screen-click verb (only gclick on gadgets); no pointer click was sent before input.
- Cleanup incomplete: the visible "i212b" shell window opened by `ami.py long` is still open on the Amiga and holds RAM:selecta-i212b as its current directory,
  so the (now empty) directory could not be deleted. Files inside are deleted. Close the window by hand, then `Delete RAM:selecta-i212b`.
  `ami.py gclick` on its close gadget did not close it, and typing EndCLI had no visible effect, so I stopped rather than guess.
