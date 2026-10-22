/* Hand-computed vectors for T12 (mixer-unit). Every number below was worked
 * out on paper from the formulas in the comments, not copied from output.
 *
 * Gain of one voice at volume 65536, pan 0: vol * (32768 - pan) >> 18 = 8192
 * (0.25 in Q15), so a mixed sample is (v * 8192) >> 15 = floor(v / 4).
 * Step 1.0 is hz_q8 = rate * 256, step 0.5 is hz_q8 = rate * 128. */
#ifndef T12_VECTORS_H
#define T12_VECTORS_H

#define VEC_RATE 44100u

/* Source for the resampler vectors (S16). */
#define VEC_SRC_LEN 8u
#define VEC_SRC { 0, 1000, 2000, 3000, 4000, 8000, 0, 0 }

/* Output frames at source positions 0, 0.5, 1, 1.5, 2, 2.5, 3, 3.5 (step 0.5). */
#define VEC_FRAMES 8u

/* NONE: floor(pos) -> 0 0 1000 1000 2000 2000 3000 3000, each / 4. */
#define VEC_NONE { 0, 0, 250, 250, 500, 500, 750, 750 }

/* LINEAR: a + ((b - a) * 16384 >> 15) -> 0 500 1000 1500 2000 2500 3000 3500, each / 4. */
#define VEC_LINEAR { 0, 125, 250, 375, 500, 625, 750, 875 }

/* CUBIC at t = 0.5: Q15 weights -2048 18432 18432 -2048 (sum 32768).
 * pos 0.5 clamps s[-1] to s[0]: (18432*1000 - 2048*2000) >> 15 = 437.
 * pos 1.5, 2.5 on the straight line: 1500, 2500 exactly.
 * pos 3.5: (-2048*2000 + 18432*3000 + 18432*4000 - 2048*8000) >> 15 = 3312.
 * Whole positions: weights 0 32768 0 0 -> the sample. Each / 4 (437 -> 109). */
#define VEC_CUBIC { 0, 109, 250, 375, 500, 625, 750, 828 }

/* CUBIC at t = 0.25 (frac 0x4000): weights -2304 28416 7424 -768;
 * on taps 0 1000 2000 3000: (28416*1000 + 7424*2000 - 768*3000) >> 15 = 1250. */
#define VEC_CUBIC_Q 1250

#endif
