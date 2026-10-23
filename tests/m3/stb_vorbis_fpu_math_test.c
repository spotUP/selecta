/* I213a: the libm-free floor, ldexp and pow of plugins/replayers/stb_vorbis/fpu_math.h give libm's results for the
 * inputs stb_vorbis feeds them (float32_unpack and lookup1_values). The 68k FPU build cannot run under vamos (its
 * 68040 core lacks fsin, flogn and fscale), so the substitutes are compared with libm on the host. The call
 * (floor)(x) with parentheses bypasses the header's function-like macro and reaches libm. */
#define SEL_FPU_MATH_TEST 1
#include <math.h>
#include <stdio.h>

#include "../../plugins/replayers/stb_vorbis/fpu_math.h"

static int bad;

static void check(const char *what, double got, double want)
{
  if (got != want) {
    printf("FAIL %s: got %.17g want %.17g\n", what, got, want);
    bad++;
  }
}

int main(void)
{
  int i;
  int e;
  int n;
  for (i = -4000; i <= 4000; i++) {
    double x = i * 0.37;
    check("floor", sel_fpu_floor(x), (floor)(x));
  }
  check("floor int", sel_fpu_floor(5.0), 5.0);
  check("floor neg int", sel_fpu_floor(-5.0), -5.0);
  check("floor neg frac", sel_fpu_floor(-1.5), -2.0);
  /* float32_unpack: a 21 bit mantissa (either sign) times 2^(exp-788), exp 0..1023 */
  for (e = 0; e <= 1023; e++) {
    int m;
    for (m = 0; m < 4; m++) {
      double mant = (m == 0) ? 1.0 : (m == 1) ? -1.0 : (m == 2) ? 2097151.0 : -123457.0;
      check("ldexp", sel_fpu_ldexp(mant, e - 788), (ldexp)(mant, e - 788));
    }
  }
  /* lookup1_values: (r+1)^dim for codebook dimensions 1..16 and small bases */
  for (n = 1; n <= 16; n++) {
    for (i = 0; i < 40; i++) {
      double want = (pow)((double)i + 1.0, n);
      /* the codebook entry count is below 2^32; beyond 2^53 the two roundings may differ in the last bit */
      if (want <= 9007199254740992.0) {
        check("pow", sel_fpu_pow_int((double)i + 1.0, n), want);
      }
    }
  }
  if (bad) {
    return 1;
  }
  printf("stb_vorbis_fpu_math_ok\n");
  return 0;
}
