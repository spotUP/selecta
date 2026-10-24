/* I118/I121: the hardware output `paula` and the `paula-shadow` twin share one output skeleton and one sink, and
 * the hardware bus writes real register offsets (here into a RAM array). Also the I121 routing table. */
#include <selecta/engine.h>
#include <selecta/output.h>
#include <selecta/plugin.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "paula/paula_hw.h"

const sel_tag* paula_entry(uint32_t host_api);
const sel_tag* shadow_entry(uint32_t host_api);

static int failures;
#define CHECK(cond, ...)                                \
    do {                                                \
        if (!(cond)) {                                  \
            failures++;                                 \
            printf("FAIL %s:%d: ", __FILE__, __LINE__); \
            printf(__VA_ARGS__);                        \
            printf("\n");                               \
        }                                               \
    } while (0)

static const sel_tag* hooks_of(const sel_tag* desc) {
    return (const sel_tag*)sel_tag_find(desc, SEL_PLUGIN_TAGS, 0);
}

int main(void) {
    static uint8_t chip[0x200];
    const sel_tag* hw = paula_entry(1);
    const sel_tag* sh = shadow_entry(1);
    const sel_tag* hh = hooks_of(hw);
    const sel_tag* sho = hooks_of(sh);
    sel_paula_bus bus;
    uint32_t w0, i;
    uint16_t v;
    static const uint32_t shared[] = {SEL_OUT_CLOSE, SEL_OUT_SET_SOURCE, SEL_OUT_PUMP, SEL_OUT_STATS,
                                      SEL_OUT_VOICE_BACKEND};

    CHECK(strcmp((const char*)sel_tag_find(hw, SEL_PLUGIN_ID, 0), "paula") == 0, "hardware id");
    CHECK(strcmp((const char*)sel_tag_find(sh, SEL_PLUGIN_ID, 0), "paula-shadow") == 0, "shadow id");
    for (i = 0; i < sizeof shared / sizeof shared[0]; i++) {
        CHECK(sel_tag_find(hh, shared[i], 0) != 0 && sel_tag_find(hh, shared[i], 0) == sel_tag_find(sho, shared[i], 0),
              "hook %x is not shared", (unsigned)shared[i]);
    }
    CHECK(sel_tag_find(hh, SEL_OUT_PAULA_HW, 0) == 1, "hardware output lacks PAULA_HW");
    CHECK(sel_tag_find(sho, SEL_OUT_PAULA_HW, 0) == 0, "shadow must not claim the chip");
    CHECK(sel_tag_find(hh, SEL_OUT_VOICE_MAX, 0) == 4 && sel_tag_find(sho, SEL_OUT_VOICE_MAX, 0) == 4, "voice max");

    /* hardware bus: one writer; AUD0PER lands at base + 0xA6, AUDxLC is two word writes */
    memset(chip, 0, sizeof chip);
    sel_paula_hw_bus(&bus, chip);
    w0 = sel_paula_hw_writes();
    bus.write(bus.ctx, SEL_PAULA_AUD_PER, 428);
    memcpy(&v, chip + SEL_PAULA_AUD_PER, 2);
    CHECK(v == 428, "AUD0PER value %u", (unsigned)v);
    bus.set_lc(bus.ctx, 1, chip, 0, 7);
    CHECK(sel_paula_hw_writes() == w0 + 3, "writes %u", (unsigned)(sel_paula_hw_writes() - w0));

    /* routing table (I121) */
    CHECK(sel_engine_voice_route(4, 1, 4) == SEL_ROUTE_DIRECT, "4 voices direct");
    CHECK(sel_engine_voice_route(1, 1, 4) == SEL_ROUTE_DIRECT, "1 voice direct");
    CHECK(sel_engine_voice_route(5, 1, 4) == SEL_ROUTE_MIXER, "5 voices mixer");
    CHECK(sel_engine_voice_route(8, 1, 4) == SEL_ROUTE_MIXER, "8 voices mixer");
    CHECK(sel_engine_voice_route(0, 1, 4) == SEL_ROUTE_MIXER, "RENDER kind mixer");
    CHECK(sel_engine_voice_route(4, 0, 4) == SEL_ROUTE_MIXER, "no sink mixer");
    CHECK(sel_engine_voice_route(32, 1, 0) == SEL_ROUTE_DIRECT, "no limit direct");
    /* I219: a HW-kind replayer (KPlayer) takes the hardware over with or without a voice sink. */
    CHECK(sel_engine_route(SEL_RP_KIND_HW, 0, 1, 4) == SEL_ROUTE_HANDOVER, "HW kind handover with sink");
    CHECK(sel_engine_route(SEL_RP_KIND_HW, 4, 0, 0) == SEL_ROUTE_HANDOVER, "HW kind handover without sink");
    CHECK(sel_engine_route(SEL_RP_KIND_VOICE, 4, 1, 4) == SEL_ROUTE_DIRECT, "VOICE row direct");
    CHECK(sel_engine_route(SEL_RP_KIND_RENDER, 4, 1, 4) == SEL_ROUTE_MIXER, "RENDER row mixer");
    CHECK(sel_engine_route(99, 4, 1, 4) == SEL_ROUTE_MIXER, "unknown kind mixer");

    printf("paula_out hw_writes=%u failures=%d\n", (unsigned)sel_paula_hw_writes(), failures);
    return failures != 0;
}
