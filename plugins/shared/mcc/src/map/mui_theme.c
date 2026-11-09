#include "mui_theme.h"
#include "mui_log.h"

#include <selecta/uimodel/tokens.h>

#include <stdio.h>
#include <string.h>

static uint32_t g_applied[MUI_TH_COUNT];

void mui_theme_bg_spec(uint32_t rgb, char* out, size_t cap) {
    /* MUI RGB image spec: each 8-bit component replicated to 32 bits. */
    snprintf(out, cap, "2:%08x,%08x,%08x", (unsigned)(((rgb >> 16) & 0xFFu) * 0x01010101u), (unsigned)(((rgb >> 8) & 0xFFu) * 0x01010101u),
             (unsigned)((rgb & 0xFFu) * 0x01010101u));
}

int mui_theme_look_for(mui_theme_role role, const uint32_t* tokens, mui_theme_look* out) {
    uint32_t rgb;
    uint32_t frame = MUI_FRAME_NONE;
    if (tokens == NULL || out == NULL || (unsigned)role >= MUI_TH_COUNT) {
        return 0;
    }
    switch (role) {
    case MUI_TH_WINDOW:
        rgb = tokens[SEL_GUI_TOKEN_BACKGROUND];
        break;
    case MUI_TH_PANEL:
        rgb = tokens[SEL_GUI_TOKEN_SURFACE];
        break;
    case MUI_TH_BUTTON:
        rgb = tokens[SEL_GUI_TOKEN_SURFACE_RAISED];
        frame = MUI_FRAME_TEXT;
        break;
    case MUI_TH_FIELD:
        rgb = tokens[SEL_GUI_TOKEN_SURFACE];
        frame = MUI_FRAME_STRING;
        break;
    case MUI_TH_LIST:
        rgb = tokens[SEL_GUI_TOKEN_SURFACE];
        frame = MUI_FRAME_TEXT;
        break;
    default: /* MUI_TH_STATUS */
        rgb = tokens[SEL_GUI_TOKEN_PANEL];
        break;
    }
    mui_theme_bg_spec(rgb, out->background, sizeof out->background);
    out->frame = frame;
    out->bg_rgb = rgb;
    g_applied[role]++;
    return 1;
}

/* sRGB component decoded to linear light, scaled 0..10000 (WCAG 2 transfer function, generated offline). */
static const uint16_t k_lin[256] = {
    0, 3, 6, 9, 12, 15, 18, 21, 24, 27, 30, 33, 37, 40, 44, 48,
    52, 56, 60, 65, 70, 75, 80, 86, 91, 97, 103, 110, 116, 123, 130, 137,
    144, 152, 160, 168, 176, 185, 194, 203, 212, 222, 232, 242, 252, 262, 273, 284,
    296, 307, 319, 331, 343, 356, 369, 382, 395, 409, 423, 437, 452, 467, 482, 497,
    513, 529, 545, 561, 578, 595, 612, 630, 648, 666, 685, 704, 723, 742, 762, 782,
    802, 823, 844, 865, 887, 908, 931, 953, 976, 999, 1022, 1046, 1070, 1095, 1119, 1144,
    1170, 1195, 1221, 1248, 1274, 1301, 1329, 1356, 1384, 1413, 1441, 1470, 1500, 1529, 1559, 1590,
    1620, 1651, 1683, 1714, 1746, 1779, 1812, 1845, 1878, 1912, 1946, 1981, 2016, 2051, 2086, 2122,
    2159, 2195, 2232, 2270, 2307, 2346, 2384, 2423, 2462, 2502, 2542, 2582, 2623, 2664, 2705, 2747,
    2789, 2831, 2874, 2918, 2961, 3005, 3050, 3095, 3140, 3185, 3231, 3278, 3325, 3372, 3419, 3467,
    3515, 3564, 3613, 3663, 3712, 3763, 3813, 3864, 3916, 3968, 4020, 4072, 4125, 4179, 4233, 4287,
    4342, 4397, 4452, 4508, 4564, 4621, 4678, 4735, 4793, 4851, 4910, 4969, 5029, 5089, 5149, 5210,
    5271, 5333, 5395, 5457, 5520, 5583, 5647, 5711, 5776, 5841, 5906, 5972, 6038, 6105, 6172, 6240,
    6308, 6376, 6445, 6514, 6584, 6654, 6724, 6795, 6867, 6939, 7011, 7084, 7157, 7231, 7305, 7379,
    7454, 7529, 7605, 7682, 7758, 7835, 7913, 7991, 8070, 8148, 8228, 8308, 8388, 8469, 8550, 8632,
    8714, 8796, 8879, 8963, 9047, 9131, 9216, 9301, 9387, 9473, 9560, 9647, 9734, 9823, 9911, 10000};

/* WCAG relative luminance in 0..10000: Rec.709 weights over linear components. Integer only (68020 has no FPU). */
static uint32_t lum(uint32_t rgb) {
    return (2126u * k_lin[(rgb >> 16) & 0xFFu] + 7152u * k_lin[(rgb >> 8) & 0xFFu] + 722u * k_lin[rgb & 0xFFu]) / 10000u;
}

int mui_theme_readable(uint32_t text_rgb, uint32_t bg_rgb) {
    uint32_t a = lum(text_rgb) + 500u; /* +0.05 of full scale */
    uint32_t b = lum(bg_rgb) + 500u;
    uint32_t hi = a > b ? a : b;
    uint32_t lo = a > b ? b : a;
    return hi * 10u / lo >= 40u;
}

/* I667n: a truecolour screen shows fg exactly as designed (no obtained pen to drift from the token), so the pen contrast gate does not
 * apply there: ACCENT_TEXT on ACCENT is the designed pair even though it is below 4.0 (RTG Play glyph measured C8C8D0, not E4E4EA). */
int mui_theme_text_uses_token(int truecolour, int plain, uint32_t fg_rgb, uint32_t bg_rgb) {
    return !plain && (truecolour || mui_theme_readable(fg_rgb, bg_rgb));
}

int mui_theme_pen_precisions(int* out, int cap) {
    static const int order[2] = {MUI_THEME_PRECISION_EXACT, MUI_THEME_PRECISION_GUI};
    int i;
    for (i = 0; i < 2 && i < cap; i++) {
        out[i] = order[i];
    }
    return i;
}

static uint32_t lum_dist(uint32_t a, uint32_t b) {
    uint32_t x = lum(a), y = lum(b);
    return x > y ? x - y : y - x;
}

int mui_theme_pick_pen(const uint32_t* pen_rgb, uint32_t n, const uint32_t* tokens) {
    static const int bgs[4] = {SEL_GUI_TOKEN_BACKGROUND, SEL_GUI_TOKEN_SURFACE, SEL_GUI_TOKEN_SURFACE_RAISED, SEL_GUI_TOKEN_PANEL};
    int best = -1;
    uint32_t best_d = 0;
    if (pen_rgb == NULL || tokens == NULL) {
        return -1;
    }
    for (uint32_t i = 0; i < n; i++) {
        int ok = 1;
        for (int b = 0; b < 4; b++) {
            ok = ok && mui_theme_readable(pen_rgb[i], tokens[bgs[b]]);
        }
        if (ok) {
            uint32_t d = lum_dist(pen_rgb[i], tokens[SEL_GUI_TOKEN_TEXT]);
            if (best < 0 || d < best_d) {
                best = (int)i;
                best_d = d;
            }
        }
    }
    return best;
}

uint32_t mui_theme_applied(void) {
    uint32_t n = 0;
    for (int i = 0; i < MUI_TH_COUNT; i++) {
        n += g_applied[i];
    }
    return n;
}

uint32_t mui_theme_applied_role(mui_theme_role role) {
    return (unsigned)role < MUI_TH_COUNT ? g_applied[role] : 0;
}

/* I685t: the file reader. Default stdio; up-mcc.library installs a dos.library reader (mui_theme_set_reader) because libnix stdio
 * must not run where the library is opened. Returns the bytes read, -1 when the file cannot be opened. */
static long stdio_read(const char* path, char* buf, size_t cap) {
    FILE* f = fopen(path, "rb");
    size_t n;
    if (f == NULL) {
        return -1;
    }
    n = fread(buf, 1, cap, f);
    fclose(f);
    return (long)n;
}
static mui_theme_reader g_reader = stdio_read;

void mui_theme_set_reader(mui_theme_reader rd) {
    g_reader = rd != NULL ? rd : stdio_read;
}

/* Parses one theme file over *t; *t is untouched unless the whole file is readable and valid. */
static int theme_apply_file(txt_theme* t, const char* path) {
    char buf[4096];
    long n;
    int bad = 0;
    txt_theme work = *t;
    if (path == NULL || path[0] == '\0' || (n = g_reader(path, buf, sizeof buf - 1)) < 0) {
        return 0;
    }
    buf[n] = '\0';
    if (txt_theme_parse(&work, buf, &bad) < 0) {
        aa_log("up-mcc: theme '%s' line %d is not a role or token line in range, ignored\n", path, bad);
        return 0;
    }
    *t = work;
    return 1;
}

int mui_theme_load(txt_theme* t, const char* env_path, const char* arg_path) {
    if (t == NULL) {
        return 0;
    }
    return theme_apply_file(t, env_path) + theme_apply_file(t, arg_path);
}

/* I685e2: the TrueType search list. Order: paths the host added (mui_font_file_add), the theme key font.file, the default. */
#define FONT_ADD_MAX 4
static char g_font_add[FONT_ADD_MAX][128];
static int g_font_add_n;
static char g_font_theme[128];
static const char* g_font_list[FONT_ADD_MAX + 3];

int mui_font_file_add(const char* path) {
    int i;
    if (path == NULL || path[0] == '\0' || strlen(path) >= sizeof g_font_add[0]) {
        return -1;
    }
    for (i = 0; i < g_font_add_n; i++) {
        if (strcmp(g_font_add[i], path) == 0) {
            return 0;
        }
    }
    if (g_font_add_n >= FONT_ADD_MAX) {
        return -1;
    }
    snprintf(g_font_add[g_font_add_n++], sizeof g_font_add[0], "%s", path);
    return 0;
}

static int font_listed(int n, const char* p) {
    int i;
    for (i = 0; i < n; i++) {
        if (strcmp(g_font_list[i], p) == 0) {
            return 1;
        }
    }
    return 0;
}

const char* const* mui_font_files(void) {
    int n = 0, i;
    for (i = 0; i < g_font_add_n; i++) {
        g_font_list[n++] = g_font_add[i];
    }
    if (g_font_theme[0] != '\0' && !font_listed(n, g_font_theme)) {
        g_font_list[n++] = g_font_theme;
    }
    if (!font_listed(n, MUI_FONT_DEFAULT)) {
        g_font_list[n++] = MUI_FONT_DEFAULT;
    }
    g_font_list[n] = NULL;
    return g_font_list;
}

/* I676b: the theme the next window open reads. A THEME request that validated stores its file here; mui_theme_resolve is the one
 * place that turns (built-in, ENV file, launch --theme, request) into tokens, for the window open and for the tests. */
static char g_request_path[256];
static uint32_t g_requests;

void mui_theme_resolve(txt_theme* t, const char* env_path, const char* launch_arg) {
    txt_theme_default(t);
    mui_theme_load(t, env_path, g_request_path[0] != '\0' ? g_request_path : launch_arg);
    snprintf(g_font_theme, sizeof g_font_theme, "%s", t->font_file);
}

int mui_theme_request(const char* env_path, const char* path) {
    txt_theme probe;
    const char* file = path != NULL && path[0] != '\0' ? path : env_path;
    if (file == NULL || file[0] == '\0' || (path != NULL && strlen(path) >= sizeof g_request_path)) {
        return -1;
    }
    txt_theme_default(&probe);
    if (mui_theme_load(&probe, NULL, file) != 1) { /* missing, unreadable or malformed: the whole request is refused */
        return -1;
    }
    snprintf(g_request_path, sizeof g_request_path, "%s", path != NULL && path[0] != '\0' ? path : "");
    g_requests++;
    return 0;
}

uint32_t mui_theme_requests(void) {
    return g_requests;
}

/* I685n: the token store (mui_theme_tokens, mui_theme_tokens_set) lives in up_store.c; the class libraries get theirs from up-mcc.library. */

static uint32_t g_live_applied;

int mui_theme_apply_live(const mui_theme_live_ops* ops, const char* env_path, const char* launch_arg) {
    txt_theme th;
    if (ops == NULL || ops->palette == NULL || ops->classes == NULL) {
        return -1;
    }
    mui_theme_resolve(&th, env_path, launch_arg);
    mui_theme_tokens_set(th.token);
    ops->palette(ops->ctx, mui_theme_tokens());
    ops->classes(ops->ctx);
    g_live_applied++;
    return 0;
}

uint32_t mui_theme_live_applied(void) {
    return g_live_applied;
}
