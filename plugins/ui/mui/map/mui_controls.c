/* MUI transport row, frame-to-attribute map and key bindings (plan 2026-10-25-amiga-mui.md MUI-P2-5, P2-6, P2-7).
 * Ids, labels and keys come from the shared action table (src/uimodel/actions.c); nothing is listed twice. */
#include <selecta/uimodel/keybind.h>
#include <selecta/uimodel/keys.h>
#include <selecta/uimodel.h>

#include <stdio.h>
#include <string.h>

#include "mui_map.h"

const char* mui_transport_action(uint32_t button, const sel_uim_frame* f) {
    switch (button) {
    case MUI_BTN_PREV:
        return "prev";
    case MUI_BTN_PLAY:
        return f->play == SEL_PLAYING ? "pause" : "play";
    case MUI_BTN_NEXT:
        return "next";
    case MUI_BTN_STOP:
        return "stop";
    case MUI_BTN_REPEAT: /* the button shows the next state: off -> all -> one -> off */
        return f->repeat_mode == 0 ? "repeat_all" : f->repeat_mode == 1 ? "repeat_one" : "repeat_off";
    case MUI_BTN_RANDOM:
        return f->random_on != 0 ? "random_off" : "random_on";
    default:
        return NULL;
    }
}

const char* mui_transport_label(uint32_t button, const sel_uim_frame* f) {
    const char* id = mui_transport_action(button, f);
    uint32_t i, n = sel_uim_action_count();
    for (i = 0; id != NULL && i < n; i++) {
        const sel_uim_action* a = sel_uim_action_at(i);
        if (strcmp(a->id, id) == 0) {
            return a->label;
        }
    }
    return NULL;
}

/* I667b: the longest label a transport button shows over play, repeat and random states: the minimum width of its class. */
const char* mui_transport_widest(uint32_t button) {
    sel_uim_frame f;
    const char* best = "";
    uint32_t play, rep, rnd;
    memset(&f, 0, sizeof f);
    for (play = 0; play < 2u; play++) {
        for (rep = 0; rep < 3u; rep++) {
            for (rnd = 0; rnd < 2u; rnd++) {
                const char* l;
                f.play = play ? SEL_PLAYING : SEL_STOPPED;
                f.repeat_mode = rep;
                f.random_on = rnd;
                l = mui_transport_label(button, &f);
                if (l != NULL && strlen(l) > strlen(best)) {
                    best = l;
                }
            }
        }
    }
    return best;
}

int mui_transport_icon_only(uint32_t button) {
    return button != MUI_BTN_STOP && button < MUI_BTN_COUNT;
}

int mui_transport_on(uint32_t button, const sel_uim_frame* f) {
    return button == MUI_BTN_REPEAT ? f->repeat_mode != 0 : button == MUI_BTN_RANDOM ? f->random_on != 0 : 0;
}

uint32_t mui_seek_line(uint32_t q16, uint32_t duration_ms, char* out, size_t cap) {
    if (q16 > MUI_SEEK_MAX) {
        q16 = MUI_SEEK_MAX;
    }
    if (duration_ms == 0u) {
        out[0] = '\0';
        return 0;
    }
    snprintf(out, cap, "SEEK %u", (unsigned)(((uint64_t)q16 * duration_ms) / MUI_SEEK_MAX));
    return 1;
}

void mui_volume_line(int32_t level, char* out, size_t cap) {
    snprintf(out, cap, "VOLUME %d", level < 0 ? 0 : level > MUI_VOLUME_MAX ? MUI_VOLUME_MAX : (int)level);
}

static const uint32_t k_attr_region[MUI_ATTR_COUNT] = {
    SEL_UIM_DIRTY_SONG,     SEL_UIM_DIRTY_SONG,   SEL_UIM_DIRTY_SONG,   SEL_UIM_DIRTY_SONG, SEL_UIM_DIRTY_PROGRESS,
    SEL_UIM_DIRTY_PROGRESS, SEL_UIM_DIRTY_PROGRESS, SEL_UIM_DIRTY_STATUS, SEL_UIM_DIRTY_VOLUME, SEL_UIM_DIRTY_TRANSPORT,
};

uint32_t mui_attr_region(uint32_t attr) {
    return attr < MUI_ATTR_COUNT ? k_attr_region[attr] : 0u;
}

uint32_t mui_dirty_attrs(uint32_t dirty) {
    uint32_t a, mask = 0;
    for (a = 0; a < MUI_ATTR_COUNT; a++) {
        if ((k_attr_region[a] & dirty) != 0) {
            mask |= 1u << a;
        }
    }
    return mask;
}

const char* mui_attr_text(uint32_t attr, const sel_uim_frame* f) {
    switch (attr) {
    case MUI_ATTR_TITLE:
        return f->title;
    case MUI_ATTR_ARTIST:
        return f->artist;
    case MUI_ATTR_FORMAT:
        return f->format;
    case MUI_ATTR_LENGTH:
        return f->length;
    case MUI_ATTR_CLOCK:
        return f->clock;
    case MUI_ATTR_REMAINING:
        return f->remaining;
    case MUI_ATTR_STATUS:
        return f->status;
    default:
        return NULL;
    }
}

uint32_t mui_keys_build(mui_key_bind* out, uint32_t cap) {
    uint32_t i, n = sel_uim_action_count(), count = 0;
    for (i = 0; i < n; i++) {
        const sel_uim_action* a = sel_uim_action_at(i);
        uint32_t code = 0, mods = 0;
        if (a->key[0] == '\0' || !txt_key_parse(a->key, &code, &mods)) {
            continue;
        }
        if (count < cap) {
            out[count].id = a->id;
            out[count].code = code;
            out[count].mods = mods;
            out[count].shortcut = a->key[1] == '\0';
        }
        count++;
    }
    return count;
}

static const mui_mode_key k_mode_keys[] = {
    {'1', "full", "PREF_SET ui.mui.mode full"},     {'2', "window", "PREF_SET ui.mui.mode window"},
    {'3', "lowered", "PREF_SET ui.mui.mode lowered"}, {'4', "mini", "PREF_SET ui.mui.mode mini"},
    {'5', "micro", "PREF_SET ui.mui.mode micro"},   {'I', "icon", "PREF_SET ui.mui.mode icon"},
};

const mui_mode_key* mui_mode_keys(uint32_t* count) {
    *count = (uint32_t)(sizeof k_mode_keys / sizeof k_mode_keys[0]);
    return k_mode_keys;
}
