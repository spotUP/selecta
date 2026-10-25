/* Display strings and small panel models shared by the text UI and the desktop UI, lifted out of
 * plugins/ui/text/text_ui.c (DUI-P1-5). Declared in include/selecta/uimodel.h. Pure C99, no allocation. */
#include <selecta/uimodel.h>
#include <selecta/tags.h>

#include <stdio.h>
#include <string.h>

void sel_uim_clock(char* out, size_t cap, uint32_t ms, int known) {
    if (!known) {
        snprintf(out, cap, "--:--");
    } else {
        snprintf(out, cap, "%02u:%02u", (unsigned)(ms / 60000u), (unsigned)((ms / 1000u) % 60u));
    }
}

const char* sel_uim_play_word(sel_play_state p) {
    return p == SEL_PLAYING ? "Playing" : p == SEL_PAUSED ? "Paused" : "Stopped";
}

/* Appends the mixer stage settings that differ from the defaults (LED Filter off, Stereo Width 100). */
void sel_uim_stage_status(char* buf, size_t cap, const sel_state* s) {
    size_t n = strlen(buf);
    if (s->led_on && n < cap) {
        n += (size_t)snprintf(buf + n, cap - n, "  LED Filter On");
    }
    if (s->stereo_width < 100u && n < cap) {
        (void)snprintf(buf + n, cap - n, "  Stereo Width %u", (unsigned)s->stereo_width);
    }
}

/* Queue heading: the entry count, or the grouping key and the drilldown
 * breadcrumb of picked values. */
void sel_uim_queue_title(char* buf, size_t cap, const sel_view_state* v, int u_lib) {
    uint32_t i;
    size_t n;
    const char* what = v->source == SEL_VIEW_QUERY ? (u_lib ? "Library" : "Query") : "Queue";
    if (v->style == SEL_VIEW_STYLE_REGULAR || v->group == NULL) {
        snprintf(buf, cap, "%s  %u %s", what, (unsigned)v->row_count, v->source == SEL_VIEW_QUERY ? "results" : "entries");
        return;
    }
    snprintf(buf, cap, "%s by %s%s", what, v->group, v->style == SEL_VIEW_STYLE_TREE ? "  Tree" : "  Drilldown");
    for (i = 0; v->style == SEL_VIEW_STYLE_DRILLDOWN && i < v->drill_n && i < 2u; i++) {
        n = strlen(buf);
        snprintf(buf + n, cap - n, " > %s", v->drill[i]);
    }
}

/* Label for a META tag: META_TITLE becomes Title. */
void sel_uim_tag_label(uint32_t tag, char* out, size_t cap) {
    const char* name = sel_tag_name(tag);
    size_t i = 0;
    if (name != NULL && strncmp(name, "META_", 5) == 0) {
        name += 5;
    }
    for (; name != NULL && name[i] != '\0' && i + 1 < cap; i++) {
        char c = name[i];
        out[i] = i == 0 ? c : (char)(c >= 'A' && c <= 'Z' ? c + 32 : c);
        if (c == '_') {
            out[i] = ' ';
        }
    }
    out[i] = '\0';
}

/* Semitone index 0..95 of a voice rate in Hz (24.8), integer only: climbs the equal tempered
 * ladder, each step times 2^(1/12) = 69433 / 65536. The voice rate is the sample rate Paula plays
 * at, so tracker C-2 (period 428) is 8287 Hz and tracker C-0 (period 1712) is 2072 Hz = index 0. */
uint32_t sel_uim_semitone(uint32_t pitch_q8) {
    uint64_t t = 2072ull << 24, p = (uint64_t)pitch_q8 << 16;
    uint32_t semi = 0;
    while (semi < 95u) {
        uint64_t next = (t * 69433ull) >> 16;
        if (p < next) {
            break;
        }
        t = next;
        semi++;
    }
    return semi;
}

/* Eight-step block glyph for a Q15 magnitude. */
uint32_t sel_uim_level_glyph(uint32_t q15) {
    uint32_t step = (q15 * 8u) >> 15;
    return 0x2581u + (step > 7u ? 7u : step);
}

/* Note name of the pattern scale (1 = C-0) into out[4]. */
void sel_uim_note_name(uint8_t n, char* out) {
    static const char names[] = "C-C#D-D#E-F-F#G-G#A-A#B-";
    if (n >= 1u && n <= 120u) {
        out[0] = names[((n - 1u) % 12u) * 2u];
        out[1] = names[((n - 1u) % 12u) * 2u + 1u];
        out[2] = (char)('0' + (n - 1u) / 12u % 10u);
        out[3] = '\0';
    } else {
        memcpy(out, n == 254u ? "^^^" : n == 255u ? "===" : "...", 4);
    }
}

/* The four text fields of a pattern cell, shared by the text UI and the desktop tracker page. */
void sel_uim_cell_fields(const sel_rp_cell* cell, char* note, char* ins, char* vol, char* fx) {
    sel_uim_note_name(cell->note, note);
    if (cell->instrument != 0) {
        snprintf(ins, 4, "%02X", (unsigned)cell->instrument);
    } else {
        memcpy(ins, "..", 3);
    }
    if (cell->volume != 0) {
        snprintf(vol, 4, "%02X", (unsigned)cell->volume);
    } else {
        memcpy(vol, "..", 3);
    }
    if (cell->effect != 0 || cell->param != 0) {
        snprintf(fx, 8, "%X%02X", (unsigned)(cell->effect & 15u), (unsigned)cell->param);
    } else {
        memcpy(fx, "...", 4);
    }
}
