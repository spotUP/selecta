/* I651: button icon lookup and palette. See mui_icons.h. */
#include "mui_icons.h"
#include "mui_aa.h"

#include <string.h>

#include <selecta/uimodel/tokens.h>

static uint32_t g_lookups[8];

int mui_icon_index(const char* name) {
    uint32_t i;
    if (name == NULL) {
        return -1;
    }
    for (i = 0; i < mui_icon_body_count; i++) {
        if (strcmp(mui_icon_names[i], name) == 0) {
            return (int)i;
        }
    }
    return -1;
}

const uint8_t* mui_icon_body_wh(int icon, int w, int h) {
    uint32_t z;
    if (icon < 0 || (uint32_t)icon >= mui_icon_body_count) {
        return (const uint8_t*)0;
    }
    for (z = 0; z < mui_icon_size_count; z++) {
        if ((int)mui_icon_sizes[z] == w && (int)mui_icon_heights[z] == h) {
            g_lookups[z]++;
            return mui_icon_sets[z][icon];
        }
    }
    return (const uint8_t*)0;
}

const uint8_t* mui_icon_by_name(const char* name, int w, int h) {
    return mui_icon_body_wh(mui_icon_index(name), w, h);
}

const uint8_t* mui_icon_body(int icon, int px) {
    return mui_icon_body_wh(icon, px, px);
}

uint32_t mui_icon_body_lookups_wh(int w, int h) {
    uint32_t z;
    for (z = 0; z < mui_icon_size_count; z++) {
        if ((int)mui_icon_sizes[z] == w && (int)mui_icon_heights[z] == h) {
            return g_lookups[z];
        }
    }
    return 0;
}

uint32_t mui_icon_body_lookups(int px) {
    return mui_icon_body_lookups_wh(px, px);
}

/* Font the width and height models assume: tier M (the laced 640x512 screen) on square pixels; on tall pixels the screen is
 * 256 rows (640x256 PAL), so the model takes the small font and with it the tight tier (I949e2). */
static void model_metrics(int aspect_q8, mui_aa_metrics_t* m) {
    int asp = aspect_q8;
    mui_aa_metrics_aspect(mui_aa_font_px(asp > 256 ? 256 : 512, 0), asp, m);
}

int mui_model_icon_px(int aspect_q8) {
    mui_aa_metrics_t m;
    model_metrics(aspect_q8, &m);
    return m.icon_px;
}

int mui_model_icon_h(int aspect_q8) {
    mui_aa_metrics_t m;
    model_metrics(aspect_q8, &m);
    return m.icon_h;
}

static uint32_t spread(uint32_t v) {
    return (v & 0xFFu) * 0x01010101u;
}

void mui_icon_palette(const uint32_t* tokens, int themed, uint32_t out[12]) {
    uint32_t face = themed ? tokens[SEL_GUI_TOKEN_SURFACE_RAISED] : 0xAAAAAAu;
    uint32_t ink = themed ? tokens[SEL_GUI_TOKEN_TEXT] : 0x000000u;
    uint32_t pen, ch;
    for (pen = 0; pen < 4; pen++) {
        for (ch = 0; ch < 3; ch++) {
            uint32_t sh = 16u - 8u * ch, a = (face >> sh) & 0xFFu, b = (ink >> sh) & 0xFFu;
            out[pen * 3 + ch] = spread((a * (3u - pen) + b * pen) / 3u); /* thirds from face to ink; pen 0 is transparent */
        }
    }
}

uint32_t mui_button_width(const char* label, int icon_only, int aspect_q8) {
    uint32_t w = 2u * MUI_BTN_PAD_X + MUI_BTN_FRAME + (uint32_t)mui_model_icon_px(aspect_q8);
    if (!icon_only && label != NULL) {
        w += MUI_BTN_ICON_GAP + (uint32_t)strlen(label) * MUI_FONT_W;
    }
    return w;
}
