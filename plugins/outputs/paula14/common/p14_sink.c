#include "p14_sink.h"

uint32_t sel_p14_period_for_rate(uint32_t rate) {
    uint32_t period;
    if (rate == 0u) {
        return 0u;
    }
    period = (SEL_PAULA_CLOCK + rate / 2u) / rate;
    return period < SEL_PAULA_PERIOD_MIN || period > SEL_PAULA_PERIOD_MAX ? 0u : period;
}

static void reg(sel_p14* p, uint32_t ch, uint32_t off, uint32_t value) {
    p->bus.write(p->bus.ctx, off + ch * SEL_PAULA_CHANNEL_STRIDE, value);
}

static void point(sel_p14* p, uint32_t half) {
    uint32_t c;
    for (c = 0; c < SEL_P14_CHANNELS; c++) {
        p->bus.set_lc(p->bus.ctx, c, p->half[c][half], 0, p->bufs * 2u + half);
    }
}

void sel_p14_close(sel_p14* p) {
    uint32_t c, h;
    p->bus.write(p->bus.ctx, SEL_PAULA_DMACON, 0x000Fu);
    for (c = 0; c < SEL_P14_CHANNELS; c++) {
        for (h = 0; h < 2u; h++) {
            if (p->half[c][h] != NULL) {
                p->host->mem_free(p->host, p->half[c][h]);
                p->half[c][h] = NULL;
            }
        }
    }
}

int sel_p14_open(sel_p14* p, const sel_host* host, const sel_paula_bus* bus, uint32_t rate, uint32_t buf_frames, int strict) {
    uint32_t c, h, i;
    p->host = host;
    p->bus = *bus;
    p->buf_frames = (buf_frames + 1u) & ~1u;
    p->period = sel_p14_period_for_rate(rate);
    if (p->period == 0u && !strict && rate > 0u && SEL_PAULA_CLOCK / rate < SEL_PAULA_PERIOD_MIN) {
        p->period = SEL_PAULA_PERIOD_MIN;
    }
    p->scheduled = 0;
    p->frames = 0;
    p->bufs = 0;
    p->hash = 2166136261u;
    sel_p14_table_default(&p->table);
    for (c = 0; c < SEL_P14_CHANNELS; c++) {
        p->half[c][0] = p->half[c][1] = NULL;
    }
    if (p->period == 0u || buf_frames == 0u) {
        return -1;
    }
    for (c = 0; c < SEL_P14_CHANNELS; c++) {
        for (h = 0; h < 2u; h++) {
            p->half[c][h] = (int8_t*)host->mem_alloc(host, p->buf_frames, SEL_MEM_SAMPLE);
            if (p->half[c][h] == NULL) {
                sel_p14_close(p);
                return -1;
            }
            for (i = 0; i < p->buf_frames; i++) {
                p->half[c][h][i] = 0;
            }
        }
    }
    point(p, 0);
    for (c = 0; c < SEL_P14_CHANNELS; c++) {
        reg(p, c, SEL_PAULA_AUD_LEN, p->buf_frames / 2u);
        reg(p, c, SEL_PAULA_AUD_PER, p->period);
        reg(p, c, SEL_PAULA_AUD_VOL, c < 2u ? p->table.vol_hi : p->table.vol_lo);
    }
    p->bus.write(p->bus.ctx, SEL_PAULA_DMACON, SEL_PAULA_DMA_SETCLR | SEL_PAULA_DMA_MASTER | 0x000Fu);
    return 0;
}

static void hash_byte(sel_p14* p, int8_t b) {
    p->hash = (p->hash ^ (uint8_t)b) * 16777619u;
}

void sel_p14_submit_n(sel_p14* p, const int16_t* pcm, uint32_t n) {
    uint32_t idle = p->scheduled ^ 1u, i;
    for (i = 0; i < p->buf_frames; i++) {
        int8_t hi, lo;
        sel_p14_split(&p->table, pcm[i * 2u], &hi, &lo);
        p->half[0][idle][i] = hi;
        p->half[3][idle][i] = lo;
        sel_p14_split(&p->table, pcm[i * 2u + 1u], &hi, &lo);
        p->half[1][idle][i] = hi;
        p->half[2][idle][i] = lo;
        hash_byte(p, p->half[0][idle][i]);
        hash_byte(p, p->half[3][idle][i]);
        hash_byte(p, p->half[1][idle][i]);
        hash_byte(p, p->half[2][idle][i]);
    }
    point(p, idle);
    p->scheduled = idle;
    p->frames += n;
    p->bufs++;
}

void sel_p14_submit(sel_p14* p, const int16_t* pcm) {
    sel_p14_submit_n(p, pcm, p->buf_frames);
}
