# I213: FPU build for the float stream replayers

Date 2026-10-23. Status: PARTIAL. Build change landed for minimp3 only; no after-timing obtained.

## Changed

- plugins/replayers/minimp3/CMakeLists.txt: in the Amiga build, `selecta_minimp3` and `sel_plugin_minimp3` get `-m68881 -ffast-math`.
  Not global. The registry still hides NEEDS_FLOAT replayers when the machine reports no FPU (src/engine/registry.c:45), so no FPU code runs on a no-FPU machine.
- `nm -u` on the minimp3 objects shows no libm references (all math inlined as FPU instructions).
- stb_vorbis is NOT changed. With `-m68881` it still calls libm `floor`, `ldexp`, `pow`, `sin`, `cos`, `exp`, `log` even with `-ffast-math`
  (`floor`, `ldexp`, `pow` remain). GCC m68k returns doubles in fp0 under `-m68881`, libnix's libm (soft, linked by nuade via `-lm`) returns in d0:d1,
  so FPU-compiled callers would read garbage. The first attempt (both plugins, no `-ffast-math`) had exactly this ABI mismatch; it was reverted for stb_vorbis.
  Options for stb_vorbis (open): link libm881 for the whole exe (breaks nuade's soft callers, and libc881 contains FPU code), or wrap/replace the three libm calls in stb_vorbis with FPU-ABI shims.
- Host build: unchanged (the option is inside `if(SEL_AMIGA)`).

## Tests

- Host ctest `stream|no-float` (8 tests): all pass. `amiga|no-float` vamos tests pass (no-float check green).
- Pre-existing failures, unrelated to this change (plugin count expectation 13 vs actual 15/16): registry-tags, registry-tags-negative, registry-machine-filter, registry-machine-filter-vamos.
  Host plugin registration is not touched by this change; not verified against main.

## Emulator timing: NOT obtained

Own FS-UAE config under the scratchpad (A1200, 68020, 68882, 8 MB fast, cycle exact, no JIT, same as the I194 report; DH0 = rig4 sys.hdf read-only,
own BOOTX startup-sequence assigning C: LIBS: DEVS: L: S: FONTS: ENVARC: ENV:). The new `selecta-cli` (minimp3 with -m68881) printed
`mathieeedoubbas/mathieeedoubtrans/locale.library failed to load` when LIBS: was unassigned, and with LIBS: assigned `selecta-cli machine`
produced no output (the process ended or stalled with an empty stdout file) in 3 attempts, so no render could be timed. Cause not found:
I did not run a baseline binary in the same rig to tell a rig problem from a regression of the -m68881 link. Next step: build the baseline (git stash of the
CMake change is not needed; checkout HEAD~1 into another build dir), run `machine` in the same rig; if it works, bisect the -m68881 link (suspect: libnix startup/FPU init).
Emulator stopped; rig6 and UP-Term untouched.

## RTF table

| Format | Tier | Rate | Before (I194) | After |
|---|---|---|---|---|
| MP3 | 68020 + 68882 | 22050 | about 69.5 | not measured |
| MP3 | 68020 + 68882 | 44100 | about 74 | not measured |
| Ogg | 68020 + 68882 | 22050 | about 62 | unchanged build, not measured |
| Ogg | 68020 + 68882 | 44100 | about 61.5 | unchanged build, not measured |
