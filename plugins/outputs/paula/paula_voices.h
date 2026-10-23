/* Paula voice sink (plan M2 phase 3, I118/I119): the sel_voices backend that
 * drives the four Paula channels through a register bus. The hardware output
 * (paula.c) and the shadow twin (shadow.c, I120) share this file and differ
 * only in the bus. Integer only: it runs from the audio task (T54). */
#ifndef SELECTA_PAULA_VOICES_H
#define SELECTA_PAULA_VOICES_H

#include <selecta/host.h>
#include <selecta/voices.h>

#define SEL_PAULA_VOICES 4u
#define SEL_PAULA_CLOCK 3546895u /* PAL: period = clock / Hz */
#define SEL_PAULA_PERIOD_MIN 113u /* hardware limit */
#define SEL_PAULA_PERIOD_MAX 65535u

/* Custom chip register offsets from $DFF000. Channel n adds n * 0x10. */
#define SEL_PAULA_DMACON 0x096u
#define SEL_PAULA_AUD_LCH 0x0A0u
#define SEL_PAULA_AUD_LCL 0x0A2u
#define SEL_PAULA_AUD_LEN 0x0A4u
#define SEL_PAULA_AUD_PER 0x0A6u
#define SEL_PAULA_AUD_VOL 0x0A8u
#define SEL_PAULA_CHANNEL_STRIDE 0x10u
#define SEL_PAULA_DMA_SETCLR 0x8000u
#define SEL_PAULA_DMA_MASTER 0x0200u

typedef struct sel_paula_bus {
    /* One 16-bit register write; `reg` is the custom chip offset. */
    void (*write)(void* ctx, uint32_t reg, uint32_t value);
    /* AUDxLC: `bytes + offset` is the chip address. `serial` numbers the chip copy; the shadow
     * logs it instead of the address, so logs are identical on every run and host. */
    void (*set_lc)(void* ctx, uint32_t channel, const void* bytes, uint32_t offset, uint32_t serial);
    void* ctx;
} sel_paula_bus;

typedef struct sel_paula_chan {
    uint8_t* copy[2]; /* chip copy of the playing and of the queued sample */
    uint32_t serial[2];
    uint32_t length[2]; /* bytes copied, even */
    sel_sample view;    /* what get_state hands out */
    uint32_t playing;
    uint32_t pitch_q8;
    uint32_t volume_q16;
    int32_t pan;
    uint32_t last_period; /* 0: none written yet */
    uint32_t last_vol;    /* 0xFFFFFFFF: none written yet */
} sel_paula_chan;

typedef struct sel_paula {
    const sel_host* host;
    sel_paula_bus bus;
    sel_voices api;
    uint32_t serial;
    sel_paula_chan chan[SEL_PAULA_VOICES];
} sel_paula;

void sel_paula_init(sel_paula* p, const sel_host* host, const sel_paula_bus* bus);
/* Stops every channel and frees the chip copies. */
void sel_paula_shutdown(sel_paula* p);
const sel_voices* sel_paula_api(sel_paula* p);

/* round(SEL_PAULA_CLOCK / hz), hz in 24.8, clamped to the hardware range; 0 for hz 0. */
uint32_t sel_paula_period_from_q8(uint32_t hz_q8);

#endif
