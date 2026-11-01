/* playsid.library LVO layer (I434b), Amiga only. Offsets count from playsid_lib.i: LIBINIT starts at -30, six bytes
 * per entry. Register contracts from HippoPlayer-68k puu016.s (the callers) and playsid.library/playsid.asm. */
#ifdef __m68k__
#include "playsid_api.h"

#include <exec/libraries.h>
#include <string.h>
#include <proto/exec.h>

static struct Library* g_base;
static int g_refs;

enum {
    LVO_ALLOC = -30,
    LVO_FREE = -36,
    LVO_SETMODULE = -54,
    LVO_STARTSONG = -60,
    LVO_STOPSONG = -66,
    LVO_PAUSE = -72,
    LVO_CONTINUE = -78,
    LVO_FORWARD = -84,
    LVO_SETVERTFREQ = -96,
    LVO_SETOPMODE = -138,
    LVO_GETOPMODE = -144,
    LVO_SETRESIDMODE = -150,
    LVO_SETRESIDFILTER = -168,
    LVO_GETRESIDBUFFER = -174,
    LVO_MEASURERESID = -180,
    LVO_SETAHIMODE = -192,
    LVO_SETRESIDBOOST = -204
};

/* One tiny function per library vector: the offset is an asm immediate, the call a plain jsr through A6.
 * A function (not a statement expression) keeps -pedantic quiet. */
#define PS_LVO_FN(fn, off)                                                                           \
    static uint32_t fn(uint32_t d0v, const void* a0v, const void* a1v) {                              \
        register struct Library* r_a6 __asm__("a6") = g_base;                                        \
        register uint32_t r_d0 __asm__("d0") = d0v;                                                  \
        register const void* r_a0 __asm__("a0") = a0v;                                               \
        register const void* r_a1 __asm__("a1") = a1v;                                               \
        __asm__ volatile("jsr %c4(%%a6)"                                                             \
                         : "+d"(r_d0), "+a"(r_a0), "+a"(r_a1), "+a"(r_a6)                            \
                         : "i"(off)                                                                  \
                         : "d1", "memory", "cc");                                                    \
        return r_d0;                                                                                 \
    }

PS_LVO_FN(lvo_alloc, LVO_ALLOC)
PS_LVO_FN(lvo_free, LVO_FREE)
PS_LVO_FN(lvo_setmodule, LVO_SETMODULE)
PS_LVO_FN(lvo_start, LVO_STARTSONG)
PS_LVO_FN(lvo_stop, LVO_STOPSONG)
PS_LVO_FN(lvo_pause, LVO_PAUSE)
PS_LVO_FN(lvo_continue, LVO_CONTINUE)
PS_LVO_FN(lvo_forward, LVO_FORWARD)
PS_LVO_FN(lvo_vert, LVO_SETVERTFREQ)
PS_LVO_FN(lvo_opmode, LVO_SETOPMODE)
PS_LVO_FN(lvo_getopmode, LVO_GETOPMODE)
PS_LVO_FN(lvo_ahi, LVO_SETAHIMODE)
PS_LVO_FN(lvo_residmode, LVO_SETRESIDMODE)
/* SetRESIDFilter takes d0 and d1 */
static void lvo_residfilter(uint32_t d0v, uint32_t d1v) {
    register struct Library* r_a6 __asm__("a6") = g_base;
    register uint32_t r_d0 __asm__("d0") = d0v;
    register uint32_t r_d1 __asm__("d1") = d1v;
    __asm__ volatile("jsr %c3(%%a6)" : "+d"(r_d0), "+d"(r_d1), "+a"(r_a6) : "i"(LVO_SETRESIDFILTER) : "a0", "a1", "memory", "cc");
}
PS_LVO_FN(lvo_residboost, LVO_SETRESIDBOOST)

static int a_open(void) {
    if (g_base == NULL) {
        g_base = OpenLibrary((CONST_STRPTR) "playsid.library", 0);
        if (g_base == NULL) {
            return 0;
        }
    }
    g_refs++;
    return 1;
}

static void a_close(void) {
    if (g_refs > 0 && --g_refs == 0 && g_base != NULL) {
        CloseLibrary(g_base);
        g_base = NULL;
    }
}

static int32_t a_alloc(void) { return (int32_t)lvo_alloc(0, 0, 0); }
static void a_free(void) { (void)lvo_free(0, 0, 0); }
static void a_set_module(const void* h, const void* image, uint32_t len) { (void)lvo_setmodule(len, h, image); }
static int32_t a_start(uint32_t song1) { return (int32_t)lvo_start(song1, 0, 0); }
static void a_stop(void) { (void)lvo_stop(0, 0, 0); }
static void a_pause(void) { (void)lvo_pause(0, 0, 0); }
static void a_continue(void) { (void)lvo_continue(0, 0, 0); }
static void a_forward(void) { (void)lvo_forward(0, 0, 0); }
static void a_vert(uint32_t hz) { (void)lvo_vert(hz, 0, 0); }
static void a_opmode(uint32_t om) { (void)lvo_opmode(om, 0, 0); }
static void a_ahi(uint32_t id) { (void)lvo_ahi(id, 0, 0); }
/* GetResidAudioBuffer returns d0 = length, d1 = period, a0/a1/a2 = buffers */
static uint32_t a_resid_buffer(const int8_t* bufs[3], uint32_t* period) {
    register struct Library* r_a6 __asm__("a6") = g_base;
    register uint32_t r_d0 __asm__("d0");
    register uint32_t r_d1 __asm__("d1");
    register const int8_t* r_a0 __asm__("a0");
    register const int8_t* r_a1 __asm__("a1");
    register const int8_t* r_a2 __asm__("a2");
    __asm__ volatile("jsr %c6(%%a6)"
                     : "=d"(r_d0), "=d"(r_d1), "=a"(r_a0), "=a"(r_a1), "=a"(r_a2), "+a"(r_a6)
                     : "i"(LVO_GETRESIDBUFFER)
                     : "memory", "cc");
    bufs[0] = r_a0;
    bufs[1] = r_a1;
    bufs[2] = r_a2;
    *period = r_d1;
    return r_d0;
}
/* MeasureResidPerformance: d0 mode, d1 internal filter, d2 external filter; returns d0 ms, d1 reference */
static uint32_t a_measure_resid(uint32_t over, uint32_t in_on, uint32_t ext_on, uint32_t* limit) {
    register struct Library* r_a6 __asm__("a6") = g_base;
    register uint32_t r_d0 __asm__("d0") = over;
    register uint32_t r_d1 __asm__("d1") = in_on;
    register uint32_t r_d2 __asm__("d2") = ext_on;
    __asm__ volatile("jsr %c4(%%a6)"
                     : "+d"(r_d0), "+d"(r_d1), "+d"(r_d2), "+a"(r_a6)
                     : "i"(LVO_MEASURERESID)
                     : "a0", "a1", "memory", "cc");
    *limit = r_d1;
    return r_d0;
}
static void a_resid_filter(uint32_t in_on, uint32_t ext_on) { lvo_residfilter(in_on, ext_on); }
static void a_resid_boost(uint32_t boost) { (void)lvo_residboost(boost, 0, 0); }
static void a_resid_mode(uint32_t over) { (void)lvo_residmode(over, 0, 0); }
static uint32_t a_getopmode(void) { return lvo_getopmode(0, 0, 0); }

/* ReadSidRegs (I434j): HippoPlayer's PatternScope reads these private playsid.library fields (puu016.s:40350-40380):
 * psb_C64Mem at base+346, SID n registers at C64Mem+$D400+$20*n, psb_Envelope1..9 words at base+448+2*i. They exist only
 * in the library build Hippo ships, so nothing is read below PS_REGS_MIN_VERSION. */
#define PS_REGS_MIN_VERSION 1
#define PS_OFF_C64MEM 346
#define PS_OFF_ENVELOPE 448
static uint32_t a_read_sid_regs(uint8_t* regs, uint8_t* env, uint32_t chips) {
    const uint8_t* base = (const uint8_t*)g_base;
    const uint8_t* mem;
    uint32_t k, i;
    if (g_base == NULL || g_base->lib_Version < PS_REGS_MIN_VERSION) return 0;
    mem = *(const uint8_t* const*)(base + PS_OFF_C64MEM);
    if (mem == NULL) return 0;
    if (chips > 3u) chips = 3u;
    for (k = 0; k < chips; k++) {
        memcpy(regs + k * 25u, mem + 0xD400u + 0x20u * k, 25u);
        for (i = 0; i < 3u; i++) {
            uint16_t w = *(const uint16_t*)(base + PS_OFF_ENVELOPE + 2u * (3u * k + i));
            env[k * 3u + i] = (uint8_t)(w > 63u ? 63u : w);
        }
    }
    return chips;
}

static const ps_ops g_ops = {a_open, a_close, a_alloc, a_free, a_set_module, a_start, a_stop,
                             a_pause, a_continue, a_forward, a_vert, a_opmode, a_ahi, a_getopmode,
                             a_resid_filter, a_resid_boost, a_resid_mode, a_resid_buffer, a_measure_resid, a_read_sid_regs};

const ps_ops* ps_ops_get(void) {
    return &g_ops;
}
#endif
