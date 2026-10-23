/* I213a: libm free floor, ldexp and pow for stb_vorbis built with -m68881.
 *
 * With -m68881 a double comes back from a call in fp0, while the libnix soft libm (floor, ldexp, pow) returns it in
 * d0:d1, so the vendored stb_vorbis.c cannot call libm from FPU code. -ffast-math lets gcc inline sin, cos, exp and
 * log as 68881 instructions; this header, force-included before the vendored source (cmake -include), replaces the
 * three calls gcc cannot inline. Only the call shapes stb_vorbis.c uses are provided: all of them run once per
 * stream at setup (float32_unpack, lookup1_values), never in the decode loop.
 * Only active when the compiler targets the 68881; every other build keeps the real libm.
 * SEL_FPU_MATH_TEST enables it on the host for tests/m3/stb_vorbis_fpu_math_test.c. */
#ifndef SEL_STB_VORBIS_FPU_MATH_H
#define SEL_STB_VORBIS_FPU_MATH_H

#if defined(__mc68881__) || defined(__HAVE_68881__) || defined(SEL_FPU_MATH_TEST)

#include <math.h>
#include <stdint.h>
#include <string.h>

/* floor for |x| < 2^31 (stb_vorbis casts every result to int). */
static inline double sel_fpu_floor(double x)
{
  int i = (int)x;
  return (double)(i - (x < (double)i));
}

/* 2^e for -1022 <= e <= 1023, built from the IEEE exponent field (no libm, no FPU-specific instruction). */
static inline double sel_fpu_pow2(int e)
{
  uint64_t bits = (uint64_t)(e + 1023) << 52;
  double r;
  memcpy(&r, &bits, sizeof r);
  return r;
}

/* x * 2^e, exact. The exponent is applied in two halves so |e| up to 2044 stays inside the pow2 range; the final
 * multiply rounds into a denormal or overflows to infinity exactly as libm ldexp does. */
static inline double sel_fpu_ldexp(double x, int e)
{
  int h = e / 2;
  return x * sel_fpu_pow2(h) * sel_fpu_pow2(e - h);
}

/* x^n for an integer exponent n >= 0 (lookup1_values passes the codebook dimension). */
static inline double sel_fpu_pow_int(double x, int n)
{
  double r = 1.0;
  while (n-- > 0) {
    r *= x;
  }
  return r;
}

#define floor(x) sel_fpu_floor(x)
#define ldexp(x, e) sel_fpu_ldexp((x), (e))
#define pow(x, n) sel_fpu_pow_int((x), (n))

#endif
#endif
