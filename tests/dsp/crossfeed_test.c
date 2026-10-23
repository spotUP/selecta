/* T47 (rows I99, I100), unit half: the built-in crossfeed plugin through the DSP
 * ABI (include/selecta/dsp.h). Coefficients come from the engine table that
 * tools/gen_dsp_coef.c generates; the plugin gets them as parameters, exactly as
 * the engine chain hands them over. Host test; the code under test has no float. */
#include <selecta/dsp.h>
#include <selecta/host.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern const sel_tag* sel_plugin_entry_crossfeed(uint32_t host_api);

static int failures;

#define CHECK(cond, ...) \
    do { \
        if (!(cond)) { \
            printf("FAIL %s:%d ", __FILE__, __LINE__); \
            printf(__VA_ARGS__); \
            printf("\n"); \
            failures++; \
        } \
    } while (0)

typedef struct plugin {
    sel_dsp_open_fn open;
    sel_dsp_process_fn process;
    sel_dsp_close_fn close;
    sel_dsp_param_fn param;
    void* state;
} plugin;

static int open_plugin(plugin* p, const sel_host* host, uint32_t rate) {
    const sel_tag* d = sel_plugin_entry_crossfeed(SEL_API_VERSION);
    const sel_tag* hooks;
    sel_dsp_crossfeed_coef c;
    if (d == NULL) return 0;
    CHECK(sel_tag_find(d, SEL_PLUGIN_KIND, 0) == SEL_PLUGIN_KIND_DSP, "kind is not dsp");
    hooks = (const sel_tag*)sel_tag_find(d, SEL_PLUGIN_TAGS, 0);
    CHECK(strcmp((const char*)sel_tag_find(hooks, SEL_DSP_NAME, 0), "Crossfeed") == 0, "DSP_NAME");
    p->open = (sel_dsp_open_fn)sel_tag_find(hooks, SEL_DSP_OPEN, 0);
    p->process = (sel_dsp_process_fn)sel_tag_find(hooks, SEL_DSP_PROCESS, 0);
    p->close = (sel_dsp_close_fn)sel_tag_find(hooks, SEL_DSP_CLOSE, 0);
    p->param = (sel_dsp_param_fn)sel_tag_find(hooks, SEL_DSP_PARAM, 0);
    if (!p->open || !p->process || !p->close || !p->param) return 0;
    if (!sel_dsp_crossfeed_coef_get(rate, &c)) return 0;
    p->state = p->open(host, rate, 2);
    if (p->state == NULL) return 0;
    sel_dsp_crossfeed_apply(p->param, p->state, &c);
    return 1;
}

static void impulse(const sel_host* host) {
    sel_dsp_crossfeed_coef c;
    plugin p;
    int16_t buf[200 * 2];
    int n;
    memset(buf, 0, sizeof buf);
    buf[0] = 32767; /* hard left */
    CHECK(open_plugin(&p, host, 44100), "open at 44100");
    if (p.state == NULL) return;
    CHECK(sel_dsp_crossfeed_coef_get(44100, &c), "coef 44100");
    CHECK(c.delay == 14, "delay %d, want 14 frames (0.3 ms)", (int)c.delay);
    CHECK(c.bleed >= 19512 && c.bleed <= 19528, "bleed %d, want 19520 (-4.5 dB)", (int)c.bleed);
    CHECK(c.direct + c.bleed == 32768, "direct %d + bleed %d != 1.0", (int)c.direct, (int)c.bleed);
    /* one-pole at 700 Hz: a = 1 - exp(-2 pi 700 / 44100) = 0.09492 */
    CHECK(c.lowpass >= 3105 && c.lowpass <= 3117, "lowpass %d, want 3111 (700 Hz)", (int)c.lowpass);
    p.process(p.state, buf, 200);
    for (n = 0; n < 14; n++) CHECK(buf[n * 2 + 1] == 0, "right[%d]=%d before the delay", n, buf[n * 2 + 1]);
    {
        int32_t bleed = ((int32_t)32767 * c.bleed) >> 15;
        int32_t want = (bleed * c.lowpass) >> 15;
        CHECK(abs(buf[14 * 2 + 1] - want) <= 3, "right[14]=%d, want about %d", buf[14 * 2 + 1], (int)want);
        CHECK(buf[15 * 2 + 1] > 0 && buf[15 * 2 + 1] < buf[14 * 2 + 1] + want, "right[15]=%d", buf[15 * 2 + 1]);
    }
    {
        int32_t want = ((int32_t)32767 * c.direct) >> 15;
        CHECK(abs(buf[0] - want) <= 2, "left[0]=%d, want about %d (direct path)", buf[0], (int)want);
    }
    p.close(p.state);
}

static void mono(const sel_host* host) {
    plugin p;
    static int16_t buf[2000 * 2];
    int n;
    for (n = 0; n < 2000; n++) buf[n * 2] = buf[n * 2 + 1] = 10000;
    CHECK(open_plugin(&p, host, 44100), "open at 44100");
    if (p.state == NULL) return;
    p.process(p.state, buf, 2000);
    for (n = 300; n < 2000; n++) {
        CHECK(abs(buf[n * 2] - 10000) <= 1 && abs(buf[n * 2 + 1] - 10000) <= 1, "mono frame %d = %d,%d, want 10000 +-1", n,
              buf[n * 2], buf[n * 2 + 1]);
        if (failures) break;
    }
    p.close(p.state);
}

static void rates(void) {
    static const uint32_t r[] = {11025, 22050, 32000, 44100, 48000, 96000};
    sel_dsp_crossfeed_coef c;
    size_t i;
    for (i = 0; i < sizeof r / sizeof r[0]; i++) {
        CHECK(sel_dsp_crossfeed_coef_get(r[i], &c), "coef %u", (unsigned)r[i]);
        CHECK(c.delay == (int32_t)((14u * r[i] + 22050u) / 44100u), "delay at %u: %d", (unsigned)r[i], (int)c.delay);
        CHECK(c.delay <= SEL_DSP_CROSSFEED_MAX_DELAY, "delay %d above ring", (int)c.delay);
    }
}

int main(void) {
    const sel_host* host = sel_host_default();
    impulse(host);
    mono(host);
    rates();
    if (failures) {
        printf("crossfeed_test: %d failure(s)\n", failures);
        return 1;
    }
    puts("crossfeed_test: ok");
    return 0;
}
