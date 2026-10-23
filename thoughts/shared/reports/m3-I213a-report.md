# I213a: stb_vorbis on the FPU tier without libm

## Problem
`-m68881` returns doubles in fp0; libnix soft libm (floor, ldexp, pow, and sin/cos/exp/log) returns them in d0:d1.
stb_vorbis calls all seven, so it could not take `-m68881` (minimp3 could, it calls no libm).

## Fix
- `plugins/replayers/stb_vorbis/fpu_math.h`, force-included (`-include`) into the stb_vorbis objects only
  (selecta_stb_vorbis and sel_plugin_stb_vorbis, Amiga builds): function-like macros `floor`, `ldexp`, `pow` map to
  inline helpers (int-cast floor, ldexp via IEEE exponent bits, integer-exponent pow). All three run once per stream at
  setup (float32_unpack, lookup1_values). The vendored source is untouched.
- Flags: `-O2 -m68881 -ffast-math`. `-ffast-math` makes gcc inline sin/cos/exp/log as 68881 instructions, but only
  when optimising; the default Amiga build is `-O0` (CMAKE_C_FLAGS_INIT has no -O), where the libm calls stayed
  (first attempt failed the nm check for that reason). So these two targets get `-O2`.
- Not used: `fscale` for ldexp. vamos' 68040 core does not implement fscale (opmode 0x26), flogn or the other
  transcendental ops, so exponent-bit construction is both emulator-checkable in principle and portable.

## Tests
- `stb-vorbis-fpu-symbols` (new, `tests/m3/stb_vorbis_fpu.cmake`): `nm -u` on both stb_vorbis 68k objects fails on
  floor/ldexp/pow/sin/cos/exp/log/sqrt/fabs/frexp or any soft-float helper. Fail-before: the same script on the
  unchanged objects of the main tree build listed `cos exp floor ldexp log pow sin` (soft-float objects). Pass after.
- `stb-vorbis-fpu-math` (new, host): the three helpers equal libm over the inputs stb_vorbis feeds them (floor on
  8001 values incl. negatives, ldexp for every exp 0..1023 with 4 mantissas, pow for bases 1..40 and dims 1..16
  while the result is below 2^53). Passes.
- `no-float-engine` stays green.

## Not verified (open)
- Decode equality of the Amiga stb_vorbis against host: not possible under vamos. `-C 68020` has no FPU (line-F crash
  at the first FPU instruction); `-C 68040` stops with `fpgen_rm_reg: unimplemented opmode 14` (flogn), then fscale
  before the ldexp change. This also holds for minimp3 (I213). It needs the FS-UAE rig with an FPU config (I213b), not
  touched here. The host test above covers the replacement math; the inlined sin/cos/exp/log are gcc's 68881 code.
- RTF effect of `-O2` plus FPU on stb_vorbis: belongs to I213b.
