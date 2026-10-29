/* Platform covers and generated artwork (design doc 2.7, DUI-P2-5; I691 platform-covers design). Integer only. */
#include <selecta/gui/gui_genart.h>
#include <selecta/uimodel/tokens.h>

#include <string.h>

uint32_t sel_gui_str_hash(const char* s) {
    uint32_t h = 2166136261u;

    for (; *s != '\0'; s++) {
        h = (h ^ (uint8_t)*s) * 16777619u;
    }
    return h;
}

typedef struct chip_row {
    const char* ext;
    uint8_t family;
} chip_row;

/* The one extension -> family table: the Modland names (library/formats_table.c) and the extensions the gme, ym and
 * openmpt replayers claim. Each console with formats of its own is its own family (Johan 2026-10-29). */
#define F(f) SEL_GUI_FAMILY_##f
static const chip_row k_chips[] = {
    {"mod", F(AMIGA)},    {"stk", F(AMIGA)},    {"nst", F(AMIGA)},     {"med", F(AMIGA)},     {"mmd0", F(AMIGA)},
    {"mmd1", F(AMIGA)},   {"mmd2", F(AMIGA)},   {"mmd3", F(AMIGA)},    {"okt", F(AMIGA)},     {"digi", F(AMIGA)},
    {"dbm", F(AMIGA)},    {"symmod", F(AMIGA)}, {"fc13", F(AMIGA)},    {"fc14", F(AMIGA)},    {"hip", F(AMIGA)},
    {"hipc", F(AMIGA)},   {"hip7", F(AMIGA)},   {"dw", F(AMIGA)},      {"fred", F(AMIGA)},    {"bp", F(AMIGA)},
    {"bp3", F(AMIGA)},    {"sid1", F(AMIGA)},   {"sid2", F(AMIGA)},    {"dm1", F(AMIGA)},     {"dm2", F(AMIGA)},
    {"sfx", F(AMIGA)},    {"tfx", F(AMIGA)},    {"mc", F(AMIGA)},      {"rh", F(AMIGA)},      {"jpn", F(AMIGA)},
    {"ahx", F(AMIGA)},    {"hvl", F(AMIGA)},
    {"sid", F(COMMODORE_64)},
    {"ym", F(ATARI)},     {"sndh", F(ATARI)},   {"sap", F(ATARI)},
    {"ay", F(ZX_SPECTRUM)},
    {"xm", F(PC_TRACKER)}, {"s3m", F(PC_TRACKER)}, {"it", F(PC_TRACKER)}, {"mtm", F(PC_TRACKER)}, {"669", F(PC_TRACKER)},
    {"far", F(PC_TRACKER)}, {"ult", F(PC_TRACKER)}, {"gtk", F(PC_TRACKER)}, {"rad", F(PC_TRACKER)},
    {"nsf", F(NES)},      {"nsfe", F(NES)},
    {"gbs", F(GAME_BOY)},
    {"spc", F(SNES)},
    {"hes", F(PC_ENGINE)},
    {"kss", F(MSX)},
    {"gym", F(MEGA_DRIVE)}, {"vgm", F(MEGA_DRIVE)},
    {"mp3", F(MUSIC)},    {"mp2", F(MUSIC)},    {"ogg", F(MUSIC)},     {"flac", F(MUSIC)},
};
#undef F

/* The Amiga file name prefixes the UADE eagleplayers claim ("dum.song"): every .extensions of third_party/nuade
 * players, matched on top of the extension table. Sorted (strcmp) for the binary search. */
static const char* const k_uade_prefix[] = {
    "aam", "abk", "adsc", "amc", "aps", "arp", "ash", "ast", "avp", "bd", "bds", "bfc", "bp", "bp2", "bp3",
    "bsi", "bss", "bye", "cin", "core", "dat", "dh", "dl", "dl_deli", "dlm1", "dln", "dm", "dm1", "dm2",
    "dmu", "dmu2", "dns", "dp", "dsc", "dsr", "dss", "dum", "dw", "dwold", "dz", "dzp", "ea", "emod", "ems",
    "ex", "fc", "fc13", "fc14", "fc3", "fc4", "fp", "fred", "ftm", "fw", "glue", "gm", "gmc", "gray", "hd",
    "hip", "hip7", "hipc", "hot", "is", "is10", "is20", "jam", "jb", "jc", "jcb", "jcbo", "jd", "jmf", "jo",
    "jpn", "jpnd", "jpo", "jpold", "jt", "kh", "kim", "lme", "ma", "max", "mc", "mcmd", "mcmd_org", "mco",
    "mcr", "md", "mg", "mii", "mk2", "mkii", "mkiio", "mm4", "mm8", "mmdc", "mod_adsc4", "mok", "mon",
    "mon_old", "mso", "mth", "mug", "mug2", "mus", "mw", "okt", "okta", "one", "osp", "pap", "pn", "powt",
    "ps", "psa", "psf", "pt", "ptm", "puma", "pvp", "qc", "qpa", "rh", "rho", "riff", "rj", "rjp", "rk",
    "rkb", "s-c", "s7g", "sa", "sas", "sb", "sc", "scn", "scr", "sct", "scumm", "sd1", "sd2", "sdata", "sdr",
    "sfx", "sg", "sid", "sid1", "sid2", "sj", "sm", "sm1", "sm2", "sm3", "smod", "smpro", "smus", "sng",
    "snk", "snx", "soc", "sonic", "spl", "ss", "st", "sun", "syn", "synmod", "tf", "thm", "tiny", "tme",
    "tmk", "tpu", "trc", "tro", "tron", "tronic", "tw", "two", "vss", "wb", "zm",};
#define UADE_PREFIX_COUNT (sizeof k_uade_prefix / sizeof k_uade_prefix[0])

static const char* const k_label[SEL_GUI_FAMILY_COUNT] = {"Music",       "Amiga",   "Commodore 64", "Atari", "ZX Spectrum",
                                                          "PC Tracker",  "NES",     "Game Boy",     "SNES",  "PC Engine",
                                                          "MSX",         "Mega Drive", "Console"};
static const char* const k_id[SEL_GUI_FAMILY_COUNT] = {"music", "amiga", "commodore_64", "atari", "zx_spectrum",
                                                       "pc_tracker", "nes", "game_boy", "snes", "pc_engine",
                                                       "msx", "mega_drive", "console"};

static void lower(char* dst, const char* src, size_t cap) {
    size_t i;

    for (i = 0; i + 1u < cap && src[i] != '\0'; i++) {
        dst[i] = (src[i] >= 'A' && src[i] <= 'Z') ? (char)(src[i] + 32) : src[i];
    }
    dst[i] = '\0';
}

static int ext_row(const char* f) {
    size_t i;

    for (i = 0; i < sizeof k_chips / sizeof k_chips[0]; i++) {
        if (strcmp(k_chips[i].ext, f) == 0) {
            return (int)i;
        }
    }
    return -1;
}

static int uade_prefix(const char* f) {
    size_t lo = 0, hi = UADE_PREFIX_COUNT;

    while (lo < hi) {
        size_t mid = (lo + hi) / 2u;
        int d = strcmp(f, k_uade_prefix[mid]);
        if (d == 0) {
            return 1;
        }
        if (d < 0) {
            hi = mid;
        } else {
            lo = mid + 1u;
        }
    }
    return 0;
}

uint32_t sel_gui_platform_ext_count(void) {
    return (uint32_t)(sizeof k_chips / sizeof k_chips[0]);
}

uint32_t sel_gui_platform_uade_prefix_count(void) {
    return (uint32_t)UADE_PREFIX_COUNT;
}

const char* sel_gui_platform_uade_prefix(uint32_t i) {
    return i < UADE_PREFIX_COUNT ? k_uade_prefix[i] : "";
}

uint32_t sel_gui_platform_of_ext(const char* ext) {
    char f[16];
    int row;

    if (ext == NULL) {
        return SEL_GUI_FAMILY_MUSIC;
    }
    lower(f, ext, sizeof f);
    row = ext_row(f);
    return row >= 0 ? k_chips[row].family : SEL_GUI_FAMILY_MUSIC;
}

uint32_t sel_gui_platform_of_name(const char* name) {
    const char *base, *p, *dot = NULL, *first = NULL;
    char f[16], pre[16];
    size_t n;
    int row;

    if (name == NULL) {
        return SEL_GUI_FAMILY_MUSIC;
    }
    base = name;
    for (p = name; *p != '\0'; p++) { /* the file part of a path ('/' or the Amiga ':') */
        if (*p == '/' || *p == ':') {
            base = p + 1;
        }
    }
    for (p = base; *p != '\0'; p++) {
        if (*p == '.') {
            dot = p;
            if (first == NULL) {
                first = p;
            }
        }
    }
    if (dot == NULL) {
        return SEL_GUI_FAMILY_MUSIC;
    }
    lower(f, dot + 1, sizeof f); /* the last extension */
    row = ext_row(f);
    if (row >= 0) {
        return k_chips[row].family;
    }
    n = (size_t)(first - base); /* the Amiga prefix */
    if (n > 0u && n < sizeof pre) {
        memcpy(pre, base, n);
        pre[n] = '\0';
        lower(f, pre, sizeof f);
        row = ext_row(f);
        if (row >= 0) {
            return k_chips[row].family;
        }
        if (uade_prefix(f)) {
            return SEL_GUI_FAMILY_AMIGA;
        }
    }
    return SEL_GUI_FAMILY_MUSIC;
}

const char* sel_gui_platform_label(uint32_t family) {
    return family < SEL_GUI_FAMILY_COUNT ? k_label[family] : k_label[0];
}

const char* sel_gui_platform_id(uint32_t family) {
    return family < SEL_GUI_FAMILY_COUNT ? k_id[family] : k_id[0];
}

const char* sel_gui_genart_chip(const char* format) {
    switch (sel_gui_platform_of_ext(format)) {
    case SEL_GUI_FAMILY_AMIGA: return "paula";
    case SEL_GUI_FAMILY_COMMODORE_64: return "sid";
    case SEL_GUI_FAMILY_ATARI: return "ay";
    case SEL_GUI_FAMILY_NES: return "2a03";
    default: return "software";
    }
}

/* ---- Platform covers (I691a) ---- */

enum { COVER_TONES = 3, ART_SLOTS = 32 };

static uint32_t family_base(const uint32_t* tokens, uint32_t family) {
    if (family >= SEL_GUI_FAMILY_COUNT) {
        family = SEL_GUI_FAMILY_MUSIC;
    }
    if (family == SEL_GUI_FAMILY_MUSIC) {
        return tokens[SEL_GUI_TOKEN_ACCENT] & 0xFFFFFFu;
    }
    return tokens[SEL_GUI_TOKEN_COVER_AMIGA + (family - 1u)] & 0xFFFFFFu;
}

uint32_t sel_gui_cover_aga_tones(void) {
    return (SEL_GUI_FAMILY_COUNT * COVER_TONES + 1u <= ART_SLOTS) ? COVER_TONES : 1u;
}

uint32_t sel_gui_cover_tone(const uint32_t* tokens, uint32_t family, uint32_t tone) {
    uint32_t base = family_base(tokens, family), out = 0u, sh;

    if (tone == 0u || tone >= COVER_TONES) {
        return base;
    }
    for (sh = 0u; sh < 24u; sh += 8u) {
        uint32_t ch = (base >> sh) & 0xFFu;
        ch = (tone == 1u) ? ch * 88u / 100u : ch + (255u - ch) * 12u / 100u;
        out |= ch << sh;
    }
    return out;
}

uint32_t sel_gui_cover_tone_of(uint32_t title_hash) {
    return (title_hash ^ (title_hash >> 15)) % COVER_TONES;
}

uint32_t sel_gui_cover_palette(uint32_t out[32], const uint32_t* tokens) {
    uint32_t tones = sel_gui_cover_aga_tones(), f, t, n = 0u;
    uint32_t text = tokens[SEL_GUI_TOKEN_ACCENT_TEXT] & 0xFFFFFFu;

    for (f = 0u; f < SEL_GUI_FAMILY_COUNT; f++) {
        for (t = 0u; t < tones; t++) {
            out[n++] = sel_gui_cover_tone(tokens, f, t);
        }
    }
    while (n < ART_SLOTS) {
        out[n++] = text; /* first spare slot holds the mark and word colour; the rest repeat it */
    }
    return tones;
}

static void box(sel_gui_canvas* c, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t argb) {
    sel_gui_rect q;

    q.x = x;
    q.y = y;
    q.w = w < 1 ? 1 : w;
    q.h = h < 1 ? 1 : h;
    sel_gui_fill_rect(c, q, argb);
}

static void disc(sel_gui_canvas* c, int32_t x, int32_t y, int32_t d, uint32_t argb) {
    sel_gui_rect q;

    q.x = x;
    q.y = y;
    q.w = d < 1 ? 1 : d;
    q.h = q.w;
    sel_gui_round_rect(c, q, q.w / 2, argb);
}

/* The family mark in the square (x0, y0, s, s): solid `fg`; `bg` is the field colour for cut-outs. Only fills,
 * round rects and frames, so it holds on the INDEX8 palette. */
static void family_mark(sel_gui_canvas* c, uint32_t family, int32_t x0, int32_t y0, int32_t s, uint32_t fg, uint32_t bg) {
    sel_gui_rect q;
    int32_t i, j, k, u;

    q.x = x0;
    q.y = y0;
    q.w = s;
    q.h = s;
    switch (family) {
    case SEL_GUI_FAMILY_AMIGA: {
        static const int32_t hh[4] = {8, 5, 7, 3};
        for (i = 0; i < 4; i++) {
            box(c, x0 + i * (s / 4), y0 + s - s * hh[i] / 8, s / 7, s * hh[i] / 8, fg);
        }
        break;
    }
    case SEL_GUI_FAMILY_COMMODORE_64:
        for (i = 0; i < 3; i++) {
            box(c, x0, y0 + i * (s * 2 / 5), s * (3 - i) / 3, s / 5, fg);
        }
        break;
    case SEL_GUI_FAMILY_ATARI: {
        static const int32_t hh[3] = {6, 4, 6};
        for (i = 0; i < 3; i++) {
            box(c, x0 + i * (s * 2 / 5), y0 + s - s / 8 - s * hh[i] / 8, s / 5, s * hh[i] / 8, fg);
        }
        box(c, x0, y0 + s - s / 8, s, s / 8, fg);
        break;
    }
    case SEL_GUI_FAMILY_ZX_SPECTRUM: {
        int32_t w = s / 8 < 1 ? 1 : s / 8;
        for (j = 0; j < s; j++) {
            for (k = 0; k < 4; k++) {
                int32_t x = (s - 1 - j + k * (s / 4)) % s, ww = w;
                if (x + ww > s) {
                    ww = s - x;
                }
                box(c, x0 + x, y0 + j, ww, 1, fg);
            }
        }
        break;
    }
    case SEL_GUI_FAMILY_PC_TRACKER: {
        static const uint32_t cells = 0xA5C3B9u;
        u = s / 6 < 1 ? 1 : s / 6;
        for (j = 0; j < 6; j++) {
            for (i = 0; i < 4; i++) {
                if (((cells >> (uint32_t)(j * 4 + i)) & 1u) != 0u) {
                    box(c, x0 + i * u, y0 + j * u, u > 2 ? u - 1 : u, u > 2 ? u - 1 : u, fg);
                }
            }
        }
        break;
    }
    case SEL_GUI_FAMILY_NES:
        u = s / 3;
        box(c, x0, y0 + u, s, u, fg);
        box(c, x0 + u, y0, u, s, fg);
        break;
    case SEL_GUI_FAMILY_GAME_BOY:
        q.x = x0 + s / 6;
        q.w = s * 2 / 3;
        sel_gui_round_rect(c, q, s / 10, fg);
        box(c, q.x + s / 10, y0 + s / 10, q.w - s / 5, s * 2 / 5, bg);
        u = s / 8;
        box(c, q.x + s / 5, y0 + s * 7 / 10, u, u, bg);
        box(c, q.x + q.w - s / 5 - u, y0 + s * 7 / 10, u, u, bg);
        break;
    case SEL_GUI_FAMILY_SNES:
        u = s / 3;
        disc(c, x0 + u, y0, u, fg);
        disc(c, x0 + u, y0 + s - u, u, fg);
        disc(c, x0, y0 + u, u, fg);
        disc(c, x0 + s - u, y0 + u, u, fg);
        break;
    case SEL_GUI_FAMILY_PC_ENGINE:
        sel_gui_round_frame(c, q, s / 2, s / 6 < 1 ? 1 : s / 6, fg);
        break;
    case SEL_GUI_FAMILY_MSX:
        u = s / 3;
        box(c, x0, y0 + 2 * u, u, u, fg);
        box(c, x0 + u, y0 + u, u, u, fg);
        box(c, x0 + 2 * u, y0, u, u, fg);
        break;
    case SEL_GUI_FAMILY_MEGA_DRIVE:
        u = s / 2;
        disc(c, x0, y0 + s / 4, u, fg);
        disc(c, x0 + s - u, y0 + s / 4, u, fg);
        break;
    case SEL_GUI_FAMILY_CONSOLE:
        sel_gui_round_frame(c, q, s / 5, s / 8 < 1 ? 1 : s / 8, fg);
        break;
    default:
        disc(c, x0, y0, s, fg);
        break;
    }
}

void sel_gui_cover_draw(sel_gui_canvas* c, sel_gui_rect r, uint32_t family, uint32_t title_hash, const uint32_t* tokens,
                        const sel_gui_atlas* label_font) {
    uint32_t t0, region, tone, field, text;
    int32_t edge, margin, s;

    if (r.w <= 0 || r.h <= 0) {
        return;
    }
    if (family >= SEL_GUI_FAMILY_COUNT) {
        family = SEL_GUI_FAMILY_MUSIC;
    }
    t0 = sel_gui_prof_begin(c);
    region = c->region;
    c->region = SEL_GUI_RGN_ART;
    tone = (c->format == SEL_GUI_INDEX8 && sel_gui_cover_aga_tones() == 1u) ? 0u : sel_gui_cover_tone_of(title_hash);
    field = 0xFF000000u | sel_gui_cover_tone(tokens, family, tone);
    text = 0xFF000000u | (tokens[SEL_GUI_TOKEN_ACCENT_TEXT] & 0xFFFFFFu);
    sel_gui_fill_rect(c, r, field);

    edge = r.w < r.h ? r.w : r.h;
    margin = edge / 10;
    s = edge / 4;
    if (s >= 2) {
        family_mark(c, family, r.x + r.w - margin - s, r.y + margin, s, text, field);
    }
    if (label_font != NULL && edge >= 64) {
        sel_gui_text_draw(c, label_font, r.x + margin, r.y + r.h - margin - label_font->line_height,
                          sel_gui_platform_label(family), text, r.w - 2 * margin, NULL);
    }
    c->region = region;
    sel_gui_prof_end(c, SEL_GUI_PROF_GENART, t0);
}

/* hue 0..1535 (six regions of 256), saturation and value 0..255. */
static uint32_t hsv(uint32_t hue, uint32_t s, uint32_t v) {
    uint32_t region = (hue / 256u) % 6u, f = hue % 256u;
    uint32_t p = v * (255u - s) / 255u;
    uint32_t q = v * (255u - s * f / 255u) / 255u;
    uint32_t t = v * (255u - s * (255u - f) / 255u) / 255u;
    uint32_t r, g, b;

    switch (region) {
    case 0: r = v; g = t; b = p; break;
    case 1: r = q; g = v; b = p; break;
    case 2: r = p; g = v; b = t; break;
    case 3: r = p; g = q; b = v; break;
    case 4: r = t; g = p; b = v; break;
    default: r = v; g = p; b = q; break;
    }
    return 0xFF000000u | (r << 16) | (g << 8) | b;
}

void sel_gui_genart_palette(uint32_t out[32]) {
    uint32_t i;
    for (i = 0; i < 16u; i++) {
        out[i] = hsv(i * 96u, 185u, 85u) & 0xFFFFFFu;       /* the gradient's start: saturation 170-201, value 70-101 */
        out[16u + i] = hsv(i * 96u, 200u, 215u) & 0xFFFFFFu; /* its end: saturation 200, value 200-231 */
    }
}

static void genart_draw(sel_gui_canvas* c, sel_gui_rect r, const char* format, uint32_t title_hash);

void sel_gui_genart_draw(sel_gui_canvas* c, sel_gui_rect r, const char* format, uint32_t title_hash) {
    uint32_t was = c->region;
    uint32_t t0 = sel_gui_prof_begin(c);
    c->region = SEL_GUI_RGN_ART;
    genart_draw(c, r, format, title_hash);
    c->region = was;
    sel_gui_prof_end(c, SEL_GUI_PROF_GENART, t0);
}

static void genart_draw(sel_gui_canvas* c, sel_gui_rect r, const char* format, uint32_t title_hash) {
    char f[16];
    uint32_t fh, ch, hue, a, b, bits;
    int32_t cell, ox, oy, x, y;
    sel_gui_rect g;

    lower(f, format, sizeof f);
    fh = sel_gui_str_hash(f);
    ch = sel_gui_str_hash(sel_gui_genart_chip(f));
    hue = (ch % 1536u + (title_hash >> 8) % 192u) % 1536u; /* the chip sets the colour family, the title nudges it */
    a = hsv(hue, 170u + (fh & 31u), 70u + (title_hash & 31u));
    b = hsv((hue + 128u + (fh >> 5) % 128u) % 1536u, 200u, 200u + ((title_hash >> 16) & 31u));
    sel_gui_gradient(c, r, 1, a, b);

    /* The glyph: a mirrored 5x5 pattern from the format hash and the title hash, centred. The title
     * must reach the glyph, not only the gradient: tiles of one format (a shelf of .mod files) would
     * otherwise all carry the same glyph (I360). */
    cell = r.w / 8;
    if (cell < 1) {
        return;
    }
    ox = r.x + (r.w - 5 * cell) / 2;
    oy = r.y + (r.h - 5 * cell) / 2;
    bits = fh ^ (fh >> 15) ^ (title_hash * 2654435761u >> 11);
    for (y = 0; y < 5; y++) {
        for (x = 0; x < 3; x++) {
            if (((bits >> (uint32_t)(y * 3 + x)) & 1u) != 0u) {
                g.y = oy + y * cell;
                g.w = cell;
                g.h = cell;
                g.x = ox + x * cell;
                sel_gui_fill_rect(c, g, 0xFFF4F4F4u);
                g.x = ox + (4 - x) * cell;
                if (x != 2) {
                    sel_gui_fill_rect(c, g, 0xFFF4F4F4u);
                }
            }
        }
    }
}
