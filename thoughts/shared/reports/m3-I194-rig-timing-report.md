# I194 decode timing on an emulated Amiga (minimp3, stb_vorbis)

Date 2026-10-23. Product code unchanged.

## Result

Only the 68020 tier was measured. The 68060 tier is blocked (below).

Real-time factor (RTF) = wall time / audio duration; below 1.0 plays in real time.
Command: `selecta-cli render <file> --output RAM:x.wav --seconds 2 --rate <hz>` (decode, resample, WAV write to RAM:),
timed with AmigaDOS `Date` before and after (1 s resolution, includes about 1 s of program load).
Clip: 2.0 s of 44.1 kHz stereo (sine plus pink noise, ffmpeg). MP3 = 128 kbps (ffmpeg default mp3 encoder).
Ogg = ffmpeg native experimental `vorbis` encoder (libvorbis not available), about 220 kbps, so the Ogg bitrate is higher than the MP3's.

| Format | CPU tier | Output rate | Wall time | RTF |
|---|---|---|---|---|
| MP3 (minimp3) | 68020 14 MHz + 68882 | 22050 | about 139 s | about 69.5 |
| MP3 (minimp3) | 68020 14 MHz + 68882 | 44100 | about 148 s | about 74 |
| Ogg (stb_vorbis) | 68020 14 MHz + 68882 | 22050 | about 124 s | about 62 |
| Ogg (stb_vorbis) | 68020 14 MHz + 68882 | 44100 | about 123 s | about 61.5 |
| any | 68060 50 MHz | any | not measured | blocked |

Both decoders are 60 to 75 times too slow for real time on a 68020. Decoding dominates; output rate barely matters.
Earlier point: the 0.34 s repo fixture sine.mp3 at 22050 took about 36 s (RTF about 106, includes fixed overhead).

Likely cause (inferred, not profiled): the Amiga build is `-m68020 -mcrt=nix20`, soft-float, with no `-m68881`.
Both decoders use float, so they run through soft-float even though an FPU is present. Unverified: no FPU-enabled build was tried.

## Emulator settings (68020)

FS-UAE, own config (not UP-Term rig scripts; nothing under UP-Term edited):
`amiga_model=A1200`, `cpu=68020`, `fpu=68882`, `fast_memory=8192`, `jit_compiler=0`, `uae_cpu_speed=real`,
`uae_cpu_cycle_exact=true`, `uae_cpu_compatible=true`, Kickstart 40.068 (kick40068-a1200.rom).
DH0 = UP-Term rig4 sys.hdf mounted read-only (rig4 not running). Work drive and a boot drive (own startup-sequence) are host folders.
`selecta-cli machine` reported: CPU=68020, CPU_SPEED=183, FPU=1, FAST_RAM=8388608, CHIP_RAM=2096128, GFX=ECS, TIER=planar.
The AmigaDOS clock tracked the host clock, so emulation ran at about 1x.
GFX=ECS (not AGA) is irrelevant to decode speed.

## 68060 tier: blocked

- `cpu=68060-NOMMU`, `fpu=68060`, real speed, no JIT, not cycle-exact: probe reports CPU=68040, CPU_SPEED=720 (3.9x the 020), FPU=0.
  Without 68060.library the OS sees a 68040 and no FPU. `fpu=68882` on the 060 still gave FPU=0.
- With FPU=0 the registry hides the NEEDS_FLOAT replayers (src/engine/registry.c), and `render` of the MP3 did not finish in 10 minutes.
  The product gives no error in that case. Cause not investigated.
- A `cpu=68040`, `fpu=68040` proxy config produced no output within about 3 minutes (boot or hang unresolved); abandoned.
- FS-UAE has no cycle-exact mode for the 060, so a 50 MHz timing would not be exact anyway.
- Unmeasured extrapolation only: if the probe ratio (3.9x) held, RTF would still be about 16 to 19 on the 060.
- To measure: add 68060.library to the rig image so the OS reports an FPU, and build with FPU code generation.

## Found

- `selecta-cli plugins` and `render` hang silently when `ENVARC:` is unassigned (src/core/config_dir.c uses `ENVARC:Selecta`):
  AmigaDOS opens an invisible requester. `machine` does not hit it. The rig assigns `ENVARC:` and `ENV:` to fix this.
- rig6 (fs-uae pid 77717) ran concurrently and was not touched. The cycle-exact emulated clock does not depend on host load.
