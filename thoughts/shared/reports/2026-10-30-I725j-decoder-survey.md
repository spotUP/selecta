# I725j: decoder survey (phase A, no rig)

Date 2026-10-30. Branch worktree-agent-a8d11d51dedfcd548. Plan row 9 of 2026-10-30-open-streaming-sources-plan.md. Rig numbers PENDING (phase B).
Harness: tools/decoder-survey/ (bench.c, make_clips.sh, build_bench.sh, build_libs_mac.sh, build_libs_amiga.sh, run_mac.sh).
No decode-only bench existed in tools/ or tests/ (I194/I212 timed `selecta-cli render` end to end), so bench.c is new: it times only the decode of an in-memory file.

## 1. Candidates

Upstream sources fetched shallow into isolated scratchpad dirs (HEAD of the default branch on 2026-10-30, commit not pinned; Tremolo = Tremolo008.zip from wss.co.uk/pinknoise/tremolo).
"Static-link OK" means permissive, no copyleft, no patent or field-of-use restriction (same bar as the Helix rejection 2026-10-23).

| Codec | Candidate | Licence (file) | Fixed point | Static-link fit | 68k bench exe (-O2 -m68020) | 68k port |
|---|---|---|---|---|---|---|
| Vorbis | stb_vorbis 1.22 (current) | public domain / MIT | no, float | yes | 68 KB | in product |
| Vorbis | Tremor (xiph, needs libogg) | BSD-3-Clause (COPYING) | yes, 64-bit MULT | yes | 140 KB incl. libogg | none known; asm only ARM |
| Vorbis | Tremolo 008 | BSD-style (COPYING; page says re-released from GPL) | yes, ARM-tuned, has _LOW_ACCURACY_ | yes | 119 KB (ONLY_C) | none; ARM asm only; C path built |
| Opus | libopus FIXED_POINT | BSD-3-Clause (COPYING) | yes | yes | 202 KB | none known; built with repo 68020 toolchain |
| AAC | Helix AAC | RPSL/RCSL | yes | REJECTED 2026-10-23 | not built | n/a |
| AAC | FAAD2 (knik0) | GPL-2.0, "non-GPL usage forbidden", commercial via Nero | yes | NO (GPL) | 260 KB | speed reference only |
| AAC | fdk-aac (mstorsjo) | Fraunhofer FDK licence (NOTICE): no patent grant, GPL-incompatible | integer, C++ | NO (same class as Helix) | not built | none known |
| AAC | opencore-aacdec (Apache-2.0), ffmpeg aac (LGPL) | not fetched; UNVERIFIED | opencore fixed | opencore probably yes; LGPL needs relink terms | not built | none known |
| FLAC | dr_flac 0.13.4 (current) | Unlicense / MIT-0 | integer | yes | 112 KB | in product |
| FLAC | libFLAC (xiph) | BSD-3 library (COPYING.Xiph); programs GPL, not used | integer; x86/PPC asm only | yes | 144 KB | none known; plain C built |
| Players | Aminet (AmigaAMP, HippoPlayer, ...) | NOT surveyed (read-only item, out of tool budget) | | | | PENDING |

AAC finding: no candidate with clean licence AND fixed point AND proven speed. FAAD2 is the fast fixed-point reference but GPL; fdk-aac and Helix carry patent/RPSL terms. Open leads: opencore-aacdec (Apache-2.0, upstream not located), ffmpeg LGPL aac. Johan decides whether LGPL is acceptable; else AAC needs an opencore port.

## 2. Mac real-time factor (Apple M1 Pro, clang -O2, nice 19, 20 reps)

Clips (make_clips.sh, I194 recipe, 10 s stereo sine 440 Hz + pink noise): t.ogg ffmpeg native vorbis 268188 B; t.opus libopus 96 kbps 48 kHz 85564 B; t.aac ffmpeg aac-lc 128 kbps ADTS 163558 B; t.flac 1076773 B. No HE-AAC clip (no HE encoder in this ffmpeg).
RTF = wall / audio duration, lower is faster. Vorbis and FLAC include demux; Opus packets pre-demuxed; AAC includes ADTS parse.

| Codec | Decoder | Wall s | RTF | x real time |
|---|---|---|---|---|
| Vorbis | stb_vorbis (float) | 0.239 | 0.00120 | 833 |
| Vorbis | Tremor (fixed) | 0.487 | 0.00243 | 411 |
| Vorbis | Tremolo (ONLY_C) | 0.783 | 0.00391 | 256 |
| Opus | libopus float | 0.299 | 0.00149 | 671 |
| Opus | libopus FIXED_POINT | 0.333 | 0.00166 | 602 |
| AAC | FAAD2 fixed (GPL, reference) | 0.271 | 0.00135 | 741 |
| AAC | fdk-aac (reference) | 0.091 | 0.00229 | 437 (first 2 s of clip only: harness fed one Fill; RTF still valid) |
| FLAC | dr_flac | 0.088 | 0.00044 | 2270 |
| FLAC | libFLAC (cmake default; SIMD not confirmed off) | 0.093 | 0.00047 | 2130 |

Reading: all Mac figures are 400 to 2000 times real time on a 64-bit out-of-order CPU with hardware FPU. They rank code on that CPU only. On a 68020/060 the order can flip: stb_vorbis is float, Tremor and libopus-fixed need 32x32->64 multiplies. Baselines I194/I212b: Ogg stb_vorbis RTF 62 (020), 17.4 (060). The rig decides.

## 3. Provisional pick per codec (Mac only; rig pending)

- Vorbis: Tremor (BSD-3, integer) is the one to beat stb_vorbis on 68k; keep stb_vorbis if the rig shows no gain. Tremolo has no 68k asm and its C path is slowest on Mac; third data point only.
- Opus: libopus FIXED_POINT (only real option; 11 percent slower than float on M1, expected to win on 68k without FPU).
- AAC: no licence-clean pick. FAAD2 is the speed reference, not linkable.
- FLAC: dr_flac stays (same speed as libFLAC on Mac, public domain, already in product, 32 KB smaller). Rig run confirms.

## 4. Asm hot paths (from reading sources, not profiled; UNVERIFIED hypotheses)

- Tremor/Tremolo: MULT32/MULT31/MULT31_SHIFT15 (misc.h; asm_arm.h is the model) in mdct.c butterflies, floor1 render_line, res012. 68020 has a 64-bit `muls.l`. The 68060 does not implement it and traps to 68060.library software, so Tremor and libopus-fixed may be disproportionately slow on the 060. Check in phase B.
- libopus fixed: MULT16_32_Q15, MAC16_32_Q15, MULT32_32_Q31 (arch.h), celt_inner_prod, xcorr_kernel, kf_bfly/opus_fft_impl, clt_mdct_backward, silk_SMULWB family in LPC and NLSF. Same 64-bit caveat.
- stb_vorbis: inverse_mdct, imdct_step3, decode_residue, window overlap (float; 060 uses FPU, 020 soft float unless FPU).
- dr_flac: rice residual decode, drflac__calculate_prediction_32 (32-bit multiplies, native on 060).

## 5. Phase B: rig commands (rig not touched in phase A)

Binaries (68020 objects, same files run on 020 and 060 configs) in the scratchpad: /private/tmp/claude-501/-Users-spot-Code-MusicPlayer/f612380c-af14-45db-8b41-4dc41cce702e/scratchpad/ami/bin/bench_{stb_vorbis,tremor,tremolo,opus,faad2,drflac,libflac}. Clips in .../scratchpad/media. If the scratchpad is gone, rebuild: `build_libs_amiga.sh <repo> <dl-dir> <out>`, then `build_bench.sh amiga <dl-dir> <out> stb_vorbis tremor tremolo opus faad2 drflac libflac` (dl-dir layout in the build_bench.sh header; re-fetch upstream shallow into empty dirs).
Clips: use short clips, `DUR=2 tools/decoder-survey/make_clips.sh <dir>` (stb_vorbis was RTF 62 on 020, so a 10 s clip costs about 10 min per run). Same recipe as I194, so baselines are comparable (m3-I194-rig-timing-report.md: MP3 69.5/74, Ogg 62/61.5 at 22050/44100 on 020; I212b 060: MP3 14.5, Ogg 17.5).
Per rig, 020 config then 060 config: rig 2 with `rig.py start --max --ram 256 --purpose "I725j decoder survey: bench decode timing"`; copy bench_* and clips to RAM:; per row `Date`, `RAM:bench_<dec> RAM:t.<ext> 1`, `Date`, 3 runs via `ami.py long` as in I212b. The bench prints `sec=`/`rtf=` from time() (1 s resolution): use reps so a run is at least 20 s, or rely on the Date pair. `sum=` must match the Mac value of the same decoder on the same clip (recompute the Mac sum for the 2 s clips) as the correctness check. Stop the rig 5 s after the last command; never stop rig 1.
Rows: Vorbis {stb_vorbis, tremor, tremolo} on t.ogg; Opus {opus} on t.opus; AAC {faad2} on t.aac (reference only); FLAC {drflac, libflac} on t.flac.
Not done: Aminet player survey, HE-AAC clip, opencore/LGPL AAC fetch, 68k link sanity run (no run done; binaries link cleanly), fdk-aac and Helix 68k builds (licence-rejected).
