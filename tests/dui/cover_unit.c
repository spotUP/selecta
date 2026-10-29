/* Unit test of the platform covers (I691a, thoughts/shared/research/2026-10-29-i691-platform-covers.md sections 1, 2, 7):
 * the extension -> family resolver, the UADE prefix rule, the title-hash tone, the AGA palette budget and the cover
 * drawing, each checked where it lives. Not a reachability test: the call sites switch in I691b-e, which carry the
 * one wiring test per surface. */
#include <selecta/gui/gui_genart.h>
#include <selecta/uimodel/tokens.h>
#include <source.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_fail;

#define CHECK(cond)                                                                    \
    do {                                                                               \
        if (!(cond)) {                                                                 \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);                      \
            g_fail++;                                                                  \
        }                                                                              \
    } while (0)

static sel_gui_rect R(int32_t x, int32_t y, int32_t w, int32_t h) {
    sel_gui_rect r;
    r.x = x;
    r.y = y;
    r.w = w;
    r.h = h;
    return r;
}

static uint32_t g_tokens[SEL_GUI_TOKEN_COUNT];

static sel_gui_canvas* make(uint32_t format, int32_t w, int32_t h) {
    sel_gui_canvas* c = (sel_gui_canvas*)malloc(sizeof *c);
    void* px = malloc(sel_gui_pixel_bytes(format, w, h));
    sel_gui_canvas_init(c, format, w, h, px);
    if (format == SEL_GUI_INDEX8) {
        uint32_t pal[256];
        sel_gui_palette_default(pal, 0x3B82F6u, 0x1E1E24u, 0xE6E6EBu);
        sel_gui_set_palette(c, pal, 0u, 256u);
    }
    return c;
}

static void drop(sel_gui_canvas* c) {
    free(c->pixels);
    free(c);
}

/* The design's extension list (section 1): every Modland extension and the ones gme, ym and openmpt claim. */
typedef struct row {
    uint32_t family;
    const char* exts; /* space separated */
} row;

static const row k_design[] = {
    {SEL_PLATFORM_FAMILY_AMIGA, "mod stk nst med mmd0 mmd1 mmd2 mmd3 okt digi dbm symmod fc13 fc14 hip hipc hip7 dw fred bp "
                           "bp3 sid1 sid2 dm1 dm2 sfx tfx mc rh jpn ahx hvl"},
    {SEL_PLATFORM_FAMILY_COMMODORE_64, "sid"},
    {SEL_PLATFORM_FAMILY_ATARI, "ym sndh sap"},
    {SEL_PLATFORM_FAMILY_ZX_SPECTRUM, "ay"},
    {SEL_PLATFORM_FAMILY_PC_TRACKER, "xm s3m it mtm 669 far ult gtk rad"},
    {SEL_PLATFORM_FAMILY_NES, "nsf nsfe"},
    {SEL_PLATFORM_FAMILY_GAME_BOY, "gbs"},
    {SEL_PLATFORM_FAMILY_SNES, "spc"},
    {SEL_PLATFORM_FAMILY_PC_ENGINE, "hes"},
    {SEL_PLATFORM_FAMILY_MSX, "kss"},
    {SEL_PLATFORM_FAMILY_MEGA_DRIVE, "gym vgm"},
};

static void test_extensions(void) {
    size_t i;
    uint32_t n = 0u;

    for (i = 0; i < sizeof k_design / sizeof k_design[0]; i++) {
        char buf[512], *tok, *save = NULL;
        strcpy(buf, k_design[i].exts);
        for (tok = strtok_r(buf, " ", &save); tok != NULL; tok = strtok_r(NULL, " ", &save)) {
            n++;
            if (sel_platform_of_ext(tok) != k_design[i].family) {
                printf("FAIL extension %s maps to %u, want %u\n", tok, sel_platform_of_ext(tok), k_design[i].family);
                g_fail++;
            }
        }
    }
    printf("design extensions checked: %u\n", n);
    CHECK(n == 54u);
    CHECK(sel_platform_ext_count() >= n);

    /* Every Modland row of the library table resolves to a platform family, none falls through to Music. */
    for (i = 0; i < sel_format_count(); i++) {
        const sel_format_entry* e = sel_format_get((uint32_t)i);
        if (e == NULL || e->ext == NULL || sel_platform_of_ext(e->ext) == SEL_PLATFORM_FAMILY_MUSIC) {
            printf("FAIL Modland row %u (%s) has no family\n", (unsigned)i, e != NULL && e->ext != NULL ? e->ext : "?");
            g_fail++;
        }
    }
    printf("modland rows checked: %u\n", (unsigned)sel_format_count());

    CHECK(sel_platform_of_ext("MOD") == SEL_PLATFORM_FAMILY_AMIGA); /* any case */
    CHECK(sel_platform_of_ext("mp3") == SEL_PLATFORM_FAMILY_MUSIC);
    CHECK(sel_platform_of_ext("zzz") == SEL_PLATFORM_FAMILY_MUSIC);
    CHECK(sel_platform_of_ext(NULL) == SEL_PLATFORM_FAMILY_MUSIC);
}

static void test_names(void) {
    CHECK(sel_platform_of_name("song.mod") == SEL_PLATFORM_FAMILY_AMIGA);
    CHECK(sel_platform_of_name("mod.song") == SEL_PLATFORM_FAMILY_AMIGA); /* the Amiga prefix form */
    CHECK(sel_platform_of_name("dir/sub/SONG.XM") == SEL_PLATFORM_FAMILY_PC_TRACKER);
    CHECK(sel_platform_of_name("DH0:Music/mod.song") == SEL_PLATFORM_FAMILY_AMIGA);
    CHECK(sel_platform_of_name("a.b.sid") == SEL_PLATFORM_FAMILY_COMMODORE_64);
    CHECK(sel_platform_of_name("x.mp3") == SEL_PLATFORM_FAMILY_MUSIC);
    CHECK(sel_platform_of_name("noext") == SEL_PLATFORM_FAMILY_MUSIC);
    CHECK(sel_platform_of_name("dir.d/noext") == SEL_PLATFORM_FAMILY_MUSIC);
    CHECK(sel_platform_of_name("") == SEL_PLATFORM_FAMILY_MUSIC);
    CHECK(sel_platform_of_name(NULL) == SEL_PLATFORM_FAMILY_MUSIC);

    /* UADE prefix rule: a prefix that is in no extension row, only in the explicit eagleplayer list. */
    CHECK(sel_platform_of_ext("dum") == SEL_PLATFORM_FAMILY_MUSIC);
    CHECK(sel_platform_of_name("dum.song") == SEL_PLATFORM_FAMILY_AMIGA);
    CHECK(sel_platform_of_name("DUM.Song") == SEL_PLATFORM_FAMILY_AMIGA);
    CHECK(sel_platform_of_name("mon_old.x") == SEL_PLATFORM_FAMILY_AMIGA);
    CHECK(sel_platform_of_name("qqq.song") == SEL_PLATFORM_FAMILY_MUSIC);
}

/* I691c: the majority rule. A tie goes to the family earlier in table order; Music wins only when alone. */
static void tally_of(sel_platform_family_tally* t, const char* const* names) {
    memset(t, 0, sizeof *t);
    while (*names != NULL) {
        sel_platform_family_tally_add(t, *names++);
    }
}

static void test_majority(void) {
    sel_platform_family_tally t;
    char f[16];
    const char* mixed[] = {"a.mp3", "b.mod", "c.mod", "d.mod", NULL};
    const char* tie[] = {"a.xm", "b.xm", "c.mod", "d.mod", NULL};
    const char* tie_rev[] = {"c.mod", "d.mod", "a.xm", "b.xm", NULL};
    const char* plain[] = {"a.mp3", "b.ogg", NULL};
    const char* stray[] = {"a.mp3", "b.mp3", "c.mp3", "d.xm", NULL};
    const char* prefix[] = {"mod.song", "x.mp3", NULL};
    const char* none[] = {NULL};

    tally_of(&t, mixed);
    CHECK(sel_platform_family_tally_majority(&t) == SEL_PLATFORM_FAMILY_AMIGA);
    sel_platform_family_tally_format(&t, f, sizeof f);
    CHECK(f[0] != '\0' && sel_platform_of_ext(f) == SEL_PLATFORM_FAMILY_AMIGA);
    tally_of(&t, tie);
    CHECK(sel_platform_family_tally_majority(&t) == SEL_PLATFORM_FAMILY_AMIGA); /* Amiga before PC Tracker */
    tally_of(&t, tie_rev);
    CHECK(sel_platform_family_tally_majority(&t) == SEL_PLATFORM_FAMILY_AMIGA); /* independent of song order */
    tally_of(&t, plain);
    CHECK(sel_platform_family_tally_majority(&t) == SEL_PLATFORM_FAMILY_MUSIC);
    sel_platform_family_tally_format(&t, f, sizeof f);
    CHECK(strcmp(f, "mp3") == 0);
    tally_of(&t, stray);
    CHECK(sel_platform_family_tally_majority(&t) == SEL_PLATFORM_FAMILY_PC_TRACKER); /* Music never beats a real family, even 3 to 1 */
    tally_of(&t, prefix);
    CHECK(sel_platform_family_tally_majority(&t) == SEL_PLATFORM_FAMILY_AMIGA);
    tally_of(&t, none);
    CHECK(sel_platform_family_tally_majority(&t) == SEL_PLATFORM_FAMILY_MUSIC);
    sel_platform_family_tally_format(&t, f, sizeof f);
    CHECK(f[0] == '\0');
}

static void test_tones(void) {
    uint32_t seen[3] = {0u, 0u, 0u}, i, d = 0u;
    const char* titles[] = {"Axel F", "Cannon Fodder", "Monkey Island", "Lotus", "Turrican", "Elysium"};

    for (i = 0u; i < 6u; i++) {
        uint32_t h = sel_gui_str_hash(titles[i]), t = sel_gui_cover_tone_of(h);
        CHECK(t < 3u);
        CHECK(sel_gui_cover_tone_of(h) == t); /* stable */
        seen[t]++;
    }
    CHECK((seen[0] != 0u) + (seen[1] != 0u) + (seen[2] != 0u) >= 2u);
    for (i = 0u; i + 1u < 6u; i++) {
        d += sel_gui_cover_tone_of(sel_gui_str_hash(titles[i])) != sel_gui_cover_tone_of(sel_gui_str_hash(titles[i + 1u]));
    }
    CHECK(d >= 2u);

    for (i = 0u; i < SEL_PLATFORM_FAMILY_COUNT; i++) {
        uint32_t base = sel_gui_cover_tone(g_tokens, i, 0u), dark = sel_gui_cover_tone(g_tokens, i, 1u),
                 light = sel_gui_cover_tone(g_tokens, i, 2u);
        int sh;
        for (sh = 0; sh < 24; sh += 8) {
            uint32_t b = (base >> sh) & 255u;
            CHECK(((dark >> sh) & 255u) == b * 88u / 100u);
            CHECK(((light >> sh) & 255u) == b + (255u - b) * 12u / 100u);
        }
        CHECK(base == (i == 0u ? g_tokens[SEL_GUI_TOKEN_ACCENT] : g_tokens[SEL_GUI_TOKEN_COVER_AMIGA + i - 1u]));
    }
}

static double lum(uint32_t rgb) {
    double v[3];
    int i;
    for (i = 0; i < 3; i++) {
        double s = (double)((rgb >> (16 - 8 * i)) & 255u) / 255.0;
        v[i] = s <= 0.03928 ? s / 12.92 : pow((s + 0.055) / 1.055, 2.4);
    }
    return 0.2126 * v[0] + 0.7152 * v[1] + 0.0722 * v[2];
}

static void test_contrast(void) {
    uint32_t f;
    double lt = lum(g_tokens[SEL_GUI_TOKEN_ACCENT_TEXT]);

    /* The platform families only: the Music family is the accent colour, whose contrast is the theme's own. */
    for (f = 1u; f < SEL_PLATFORM_FAMILY_COUNT; f++) {
        double lb = lum(sel_gui_cover_tone(g_tokens, f, 0u));
        double ratio = (lt + 0.05) / (lb + 0.05);
        if (ratio < 4.5) {
            printf("FAIL contrast of %s base tone is %.2f\n", sel_platform_label(f), ratio);
            g_fail++;
        }
    }
}

static void test_aga_palette(void) {
    uint32_t pal[32], tones, f, t, text = g_tokens[SEL_GUI_TOKEN_ACCENT_TEXT] & 0xFFFFFFu;
    uint32_t used;
    sel_gui_canvas* c = make(SEL_GUI_INDEX8, 64, 64);

    tones = sel_gui_cover_palette(pal, g_tokens);
    CHECK(tones == sel_gui_cover_aga_tones());
    CHECK(tones == 1u || tones == 3u);
    used = SEL_PLATFORM_FAMILY_COUNT * tones + 1u; /* tones per family plus the text colour */
    printf("aga palette: %u families x %u tone(s) + text = %u of 32 slots\n", (unsigned)SEL_PLATFORM_FAMILY_COUNT, tones, used);
    CHECK(used <= 32u); /* the palette fits the free artwork slots (entries 32..63) */
    CHECK(pal[used - 1u] == text);
    sel_gui_set_palette(c, pal, 32u, 32u);
    for (f = 0u; f < SEL_PLATFORM_FAMILY_COUNT; f++) {
        for (t = 0u; t < tones; t++) {
            CHECK(pal[f * tones + t] == sel_gui_cover_tone(g_tokens, f, t));
        }
        sel_gui_cover_draw(c, R(0, 0, 64, 64), f, 12345u, g_tokens, NULL);
        /* The field is the base tone exactly: the palette holds it, so the nearest entry is an exact match. */
        CHECK((sel_gui_pixel(c, 2, 62) & 0xFFFFFFu) == sel_gui_cover_tone(g_tokens, f, 0u));
    }
    drop(c);
}

static void test_draw(void) {
    static const uint8_t alpha[16] = {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255};
    sel_gui_atlas font;
    uint32_t f, text = 0xFF000000u | g_tokens[SEL_GUI_TOKEN_ACCENT_TEXT];

    memset(&font, 0, sizeof font); /* no glyphs: every character draws the missing box, a 4x4 block */
    font.alpha = alpha;
    font.page_w = 4;
    font.page_h = 4;
    font.ascent = 6;
    font.line_height = 8;
    font.missing.w = 4;
    font.missing.h = 4;
    font.missing.adv = 5;
    font.missing.by = 4;

    for (f = 0u; f < SEL_PLATFORM_FAMILY_COUNT; f++) {
        uint32_t tone, field_ok = 0u, small_word = 0u, big_word = 0u, mark = 0u, hash;
        int32_t x, y;
        sel_gui_canvas* c;

        for (tone = 0u; tone < 3u; tone++) { /* a title hash for each tone */
            for (hash = 0u; sel_gui_cover_tone_of(hash) != tone; hash++) {
            }
            c = make(SEL_GUI_RGBA32, 128, 128);
            sel_gui_cover_draw(c, R(0, 0, 128, 128), f, hash, g_tokens, &font);
            field_ok += (sel_gui_pixel(c, 64, 64) & 0xFFFFFFu) == sel_gui_cover_tone(g_tokens, f, tone);
            for (y = 64; y < 128; y++) {
                for (x = 0; x < 64; x++) { /* bottom left quadrant: the word, never the mark */
                    big_word += (sel_gui_pixel(c, x, y) & 0xFFFFFFu) == (text & 0xFFFFFFu);
                }
            }
            for (y = 0; y < 64; y++) {
                for (x = 64; x < 128; x++) { /* top right quadrant: the mark */
                    mark += (sel_gui_pixel(c, x, y) & 0xFFFFFFu) == (text & 0xFFFFFFu);
                }
            }
            drop(c);
        }
        CHECK(field_ok == 3u);
        CHECK(big_word > 0u);
        CHECK(mark > 0u);

        c = make(SEL_GUI_RGBA32, 48, 48);
        sel_gui_cover_draw(c, R(0, 0, 48, 48), f, 7u, g_tokens, &font);
        for (y = 24; y < 48; y++) {
            for (x = 0; x < 24; x++) {
                small_word += (sel_gui_pixel(c, x, y) & 0xFFFFFFu) == (text & 0xFFFFFFu);
            }
        }
        CHECK(small_word == 0u); /* under 64 px: no word */
        drop(c);
    }
}

int main(void) {
    uint32_t i;

    for (i = 0u; i < SEL_GUI_TOKEN_COUNT; i++) {
        g_tokens[i] = sel_gui_token_default((sel_gui_token)i);
    }
    test_extensions();
    test_names();
    test_majority();
    test_tones();
    test_contrast();
    test_aga_palette();
    test_draw();
    if (g_fail == 0) {
        printf("cover_unit: PASS\n");
        return 0;
    }
    printf("cover_unit: %d FAIL\n", g_fail);
    return 1;
}
