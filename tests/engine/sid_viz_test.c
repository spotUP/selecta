/* I1226: the libsidplayfp SID replayer (a RENDER-kind module) answers the register feed (SEL_RP_VIZ_SID). A real SID is
 * opened and played through the engine's own entry points (QUEUE_ADD, PLAY, render, pump) with the module loaded from
 * $SELECTA_PLUGIN_PATH; the viz frame must then carry sid_chips >= 1 and registers that change from block to block.
 * Sentinel: the printed "sid_viz_ok chips N changed M" line only appears when the hook ran. */
#include <selecta/engine.h>
#include <selecta/host.h>
#include <selecta/protocol.h>
#include <selecta/registry.h>
#include <selecta/viz.h>
#include <stdio.h>
#include <string.h>

static int g_fail;
#define CHECK(c)                                        \
    do {                                                \
        if (!(c)) {                                     \
            printf("FAIL line %d: %s\n", __LINE__, #c); \
            g_fail++;                                   \
        }                                               \
    } while (0)

static sel_engine* eng;

static void run(const char* line) {
    sel_message m;
    char pool[1536];
    if (sel_codec_decode(line, SEL_MSG_CMD, &m, pool, sizeof pool) != SEL_CODEC_OK) {
        CHECK(0);
        return;
    }
    sel_engine_send(eng, &m);
    sel_engine_pump(eng, NULL, NULL);
}

int main(int argc, char** argv) {
    const sel_host* host = sel_host_default();
    sel_engine_config cfg;
    char line[1200];
    static int16_t buf[441 * 2];
    uint8_t prev[SEL_VIZ_SID_REGS];
    uint32_t n, changed = 0, seen = 0, last_seq = 0, chips = 0, nonzero = 0;
    if (argc < 2) return 2;
    CHECK(sel_registry_init(host) > 0);
    memset(&cfg, 0, sizeof cfg);
    cfg.registry_count = sel_registry_count;
    cfg.registry_get = sel_registry_get;
    eng = sel_engine_create(host, &cfg);
    CHECK(eng != NULL);
    if (eng == NULL) return 1;
    snprintf(line, sizeof line, "QUEUE_ADD %s", argv[1]);
    run(line);
    run("PLAY");
    memset(prev, 0, sizeof prev);
    for (n = 0; n < 300u; n++) { /* 3 s of song at 20 ms per viz block */
        const sel_viz_frame* vf;
        sel_engine_render(eng, buf, 441u);
        sel_engine_pump(eng, NULL, NULL);
        vf = sel_engine_state(eng)->viz;
        if (vf == NULL || vf->seq == last_seq) continue;
        last_seq = vf->seq;
        if (vf->sid_chips > chips) chips = vf->sid_chips;
        if (vf->sid_chips >= 1u) {
            uint32_t i;
            seen++;
            for (i = 0; i < SEL_VIZ_SID_REGS; i++) if (vf->sid[0].reg[i] != 0u) nonzero++;
            if (seen > 1u && memcmp(prev, vf->sid[0].reg, sizeof prev) != 0) changed++;
            memcpy(prev, vf->sid[0].reg, sizeof prev);
        }
    }
    CHECK(chips >= 1u);
    CHECK(seen > 10u);
    CHECK(nonzero > 0u);
    CHECK(changed >= 2u); /* the registers move across blocks, not a frozen mirror */
    run("STOP");
    sel_engine_destroy(eng);
    if (g_fail == 0) printf("sid_viz_ok chips %u changed %u\n", chips, changed);
    return g_fail != 0;
}
