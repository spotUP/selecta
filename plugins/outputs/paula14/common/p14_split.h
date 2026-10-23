/* paula14 split tables (plan M2 phase 4, I122, D-M2-4). A 14-bit sample is carried by two Paula channels per
 * side: the high channel plays the top 8 bits at volume 64, the low channel plays the bottom 6 bits at volume 1.
 * Reconstruction in 14-bit LSB units is hi * vol_hi + lo * vol_lo. The default is the Eagleplayer 14 Bit
 * Amplifier linear table; a per-machine calibration replaces `lo`. Portable, integer only (audio task). */
#ifndef SELECTA_P14_SPLIT_H
#define SELECTA_P14_SPLIT_H

#include <stdint.h>

typedef struct sel_p14_table {
    uint8_t vol_hi; /* AUDxVOL of the high channel, 0..64 */
    uint8_t vol_lo; /* AUDxVOL of the low channel, 0..64 */
    int8_t lo[64];  /* sample byte the low channel plays for a low part 0..63 */
} sel_p14_table;

/* Fills `t` with the linear default (vol 64 / 1, lo[i] = i). */
void sel_p14_table_default(sel_p14_table* t);
/* s16 -> the two channel bytes. */
void sel_p14_split(const sel_p14_table* t, int16_t s, int8_t* hi, int8_t* lo);
/* The two channel bytes -> value in 14-bit LSB units, as the analogue sum reconstructs it. */
int32_t sel_p14_merge(const sel_p14_table* t, int8_t hi, int8_t lo);
/* Largest |merge(split(s)) - (s >> 2)| over every s16 input, in 14-bit LSB. */
uint32_t sel_p14_max_error(const sel_p14_table* t);

#endif
