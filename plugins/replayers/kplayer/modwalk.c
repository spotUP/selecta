/* ProTracker timing walker (I219g), see modwalk.h. Mirrors KPlayer v33, not ProTracker in general: F00 sets
 * speed 31, Fxx above 31 sets the tempo, a song wrap resets the tempo to 125 and keeps the speed, and a Bxx to
 * the current position without a Dxx on the same row, or a Bxx from the last position, is a song end. */
#include "modwalk.h"

enum {
    MW_LENGTH_AT = 950,   /* song length byte; KPlayer reads it as -2(k_songdataptr) */
    MW_ORDERS_AT = 952,   /* 128 order entries */
    MW_PATTERNS_AT = 1084,
    MW_ROW_BYTES = 16,    /* 4 channels x 4 bytes */
    MW_PATTERN_BYTES = 1024,
    NO_JUMP = 0x10000u,
    MW_MAX_ROWS = 1u << 20 /* bound on the walk; every row takes at least one tick, so a real target ends first */
};

typedef struct mw_channel {
    uint8_t loop_row;   /* n_pattpos */
    uint8_t loop_count; /* n_loopcount */
} mw_channel;

/* One walker: finds the row at `ms` (find) or, with end_ms != NULL, the time the song ends (length). The song ends
 * where KPlayer's k_songover would count. */
static int walk(const uint8_t* mod, size_t size, uint32_t ms, kp_walk_pos* out, uint32_t* end_ms) {
    mw_channel ch[4] = {{0, 0}, {0, 0}, {0, 0}, {0, 0}};
    uint64_t target_us = (uint64_t)ms * 1000u;
    uint64_t now_us = 0;
    uint32_t songpos = 0, row = 0, speed = 6, tempo = 125, length, rows;
    if (mod == NULL || (out == NULL && end_ms == NULL) || size < MW_PATTERNS_AT) {
        return 0;
    }
    length = mod[MW_LENGTH_AT];
    for (rows = 0; rows < MW_MAX_ROWS; rows++) {
        const uint8_t* cell;
        size_t at;
        uint32_t c, pbreakpos = 0, posjump = 0, pbreakflag = 0, delay = 0, has_d = 0, ended = 0;
        uint32_t next_pos = NO_JUMP, start_speed, start_tempo;
        uint64_t row_us;
        if (songpos > 127u) {
            return 0;
        }
        at = MW_PATTERNS_AT + (size_t)mod[MW_ORDERS_AT + songpos] * MW_PATTERN_BYTES + (size_t)row * MW_ROW_BYTES;
        if (at + MW_ROW_BYTES > size) {
            return 0;
        }
        cell = mod + at;
        start_speed = speed;
        start_tempo = tempo;
        for (c = 0; c < 4u; c++) {
            if ((cell[c * 4u + 2u] & 0x0fu) == 0x0du) {
                has_d = 1;
            }
        }
        /* Tick 0 effects, channel 1 to 4 (k_checkmoreefx). */
        for (c = 0; c < 4u; c++) {
            uint32_t cmd = cell[c * 4u + 2u] & 0x0fu, p = cell[c * 4u + 3u];
            if (cmd == 0x0fu) { /* k_setspeed */
                if (p == 0u) {
                    speed = 0x1fu;
                } else if (p <= 0x1fu) {
                    speed = p;
                } else {
                    tempo = p;
                }
            } else if (cmd == 0x0bu) { /* k_positionjump */
                if (!has_d && p == songpos) {
                    ended = 1; /* no jump: k_songover counts and play goes on */
                    continue;
                }
                if (!has_d && songpos + 1u == length) {
                    ended = 1;
                }
                next_pos = p;
                posjump = 1;
                pbreakpos = 0;
            } else if (cmd == 0x0du) { /* k_patternbreak: BCD, which KPlayer's init converts to decimal */
                uint32_t v = (p >> 4) * 10u + (p & 0x0fu);
                posjump = 1;
                pbreakpos = v > 63u ? 0u : v;
            } else if (cmd == 0x0eu && (p >> 4) == 0x6u) { /* k_jumploop */
                uint32_t x = p & 0x0fu;
                if (x == 0u) {
                    ch[c].loop_row = (uint8_t)row;
                } else if (ch[c].loop_count == 0u) {
                    ch[c].loop_count = (uint8_t)x;
                    pbreakpos = ch[c].loop_row;
                    pbreakflag = 1;
                } else if (--ch[c].loop_count != 0u) {
                    pbreakpos = ch[c].loop_row;
                    pbreakflag = 1;
                }
            } else if (cmd == 0x0eu && (p >> 4) == 0xeu) { /* k_patterndelay: the last channel wins */
                delay = (p & 0x0fu) + 1u;
            }
        }
        row_us = (uint64_t)speed * (delay != 0u ? delay : 1u) * 2500000u / tempo;
        if (end_ms == NULL && target_us < now_us + row_us) {
            /* KPlayer replays this row's effects itself, so the state is the one in force before them. */
            out->songpos = (uint8_t)songpos;
            out->row = (uint8_t)row;
            out->speed = (uint8_t)start_speed;
            out->tempo = (uint8_t)start_tempo;
            out->at_ms = (uint32_t)(now_us / 1000u);
            return 1;
        }
        if (ended) {
            if (end_ms != NULL) {
                *end_ms = (uint32_t)((now_us + row_us) / 1000u);
                return 1;
            }
            return 0; /* the song ends as this row finishes, at or before the target */
        }
        now_us += row_us;
        /* k_dskip and k_nextposition. */
        row++;
        if (pbreakflag) {
            row = pbreakpos;
            pbreakpos = 0;
        }
        if (row >= 64u || posjump) {
            row = pbreakpos;
            songpos = posjump && next_pos != NO_JUMP ? next_pos : songpos + 1u;
            if (songpos >= length) {
                if (end_ms != NULL) {
                    *end_ms = (uint32_t)(now_us / 1000u);
                    return 1;
                }
                return 0; /* k_songover: the song wraps to position 0 before the target */
            }
        }
    }
    return 0;
}

int kp_walk_find(const uint8_t* mod, size_t size, uint32_t ms, kp_walk_pos* out) {
    return walk(mod, size, ms, out, NULL);
}

int kp_walk_length(const uint8_t* mod, size_t size, uint32_t* length_ms) {
    return length_ms != NULL && walk(mod, size, 0, NULL, length_ms);
}

/* ---- I732: tracker cells ---- */

enum {
    MW_ROWS = 64,
    MW_CHANNELS = 4,
    MW_PERIODS = 36,
    MW_FIRST_NOTE = 49 /* period 856 = C-1 in ProTracker = C-4 on libopenmpt's scale */
};

/* ProTracker finetune 0 periods, C-1 .. B-3. */
static const uint16_t mw_period[MW_PERIODS] = {856, 808, 762, 720, 678, 640, 604, 570, 538, 508, 480, 453,
                                               428, 404, 381, 360, 339, 320, 302, 285, 269, 254, 240, 226,
                                               214, 202, 190, 180, 170, 160, 151, 143, 135, 127, 120, 113};

static uint8_t note_of_period(uint32_t period) {
    uint32_t i, best = 0, best_d = 0xffffu;
    if (period == 0u) {
        return 0;
    }
    for (i = 0; i < MW_PERIODS; i++) { /* nearest, so a finetuned period lands on its note */
        uint32_t d = period > mw_period[i] ? period - mw_period[i] : mw_period[i] - period;
        if (d < best_d) {
            best_d = d;
            best = i;
        }
    }
    return (uint8_t)(MW_FIRST_NOTE + best);
}

int kp_tracker_info_get(const uint8_t* mod, size_t size, uint32_t order, kp_tracker_info* out) {
    uint32_t length, i, top = 0;
    if (mod == NULL || out == NULL || size < MW_PATTERNS_AT) {
        return 0;
    }
    length = mod[MW_LENGTH_AT];
    if (length > 128u) {
        length = 128u;
    }
    if (order >= length) {
        return 0;
    }
    for (i = 0; i < 128u; i++) { /* the pattern count is the highest pattern in the whole table, as every tracker counts */
        if (mod[MW_ORDERS_AT + i] > top) {
            top = mod[MW_ORDERS_AT + i];
        }
    }
    out->orders = length;
    out->patterns = top + 1u;
    out->rows = MW_ROWS;
    out->channels = MW_CHANNELS;
    return 1;
}

uint32_t kp_tracker_cells_get(const uint8_t* mod, size_t size, uint32_t order, uint32_t channel, uint32_t row_lo,
                              uint32_t row_hi, sel_rp_cell* out, uint32_t cap) {
    kp_tracker_info info;
    uint32_t row, n = 0;
    if (out == NULL || channel >= MW_CHANNELS || !kp_tracker_info_get(mod, size, order, &info)) {
        return 0;
    }
    for (row = row_lo; row < row_hi && n < cap; row++, n++) {
        size_t at = MW_PATTERNS_AT + (size_t)mod[MW_ORDERS_AT + order] * MW_PATTERN_BYTES +
                    (size_t)row * MW_ROW_BYTES + (size_t)channel * 4u;
        out[n].note = out[n].instrument = out[n].volume = out[n].effect = out[n].param = 0;
        if (row < MW_ROWS && at + 4u <= size) {
            const uint8_t* c = mod + at;
            out[n].note = note_of_period(((uint32_t)(c[0] & 0x0fu) << 8) | c[1]);
            out[n].instrument = (uint8_t)((c[0] & 0xf0u) | (c[2] >> 4));
            out[n].effect = c[2] & 0x0fu;
            out[n].param = c[3];
        }
    }
    return n;
}
