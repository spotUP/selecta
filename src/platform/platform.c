/* Platform families (I691a, I691c2): the one extension -> family table, the file name resolver and the majority tally.
 * Pure C with no dependencies, so the library (which hands the majority format to the UI) and the GUI (which draws the
 * cover of a family) both link it; neither depends on the other. Integer only. */
#include <selecta/platform.h>

#include <string.h>

typedef struct chip_row {
    const char* ext;
    uint8_t family;
} chip_row;

/* The one extension -> family table: the Modland names (library/formats_table.c) and the extensions the gme, ym and
 * openmpt replayers claim. Each console with formats of its own is its own family (Johan 2026-10-29). */
#define F(f) SEL_PLATFORM_FAMILY_##f
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

static const char* const k_label[SEL_PLATFORM_FAMILY_COUNT] = {"Music",       "Amiga",   "Commodore 64", "Atari", "ZX Spectrum",
                                                          "PC Tracker",  "NES",     "Game Boy",     "SNES",  "PC Engine",
                                                          "MSX",         "Mega Drive", "Console"};
static const char* const k_id[SEL_PLATFORM_FAMILY_COUNT] = {"music", "amiga", "commodore_64", "atari", "zx_spectrum",
                                                       "pc_tracker", "nes", "game_boy", "snes", "pc_engine",
                                                       "msx", "mega_drive", "console"};

void sel_platform_lower(char* dst, const char* src, size_t cap) {
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

uint32_t sel_platform_ext_count(void) {
    return (uint32_t)(sizeof k_chips / sizeof k_chips[0]);
}

uint32_t sel_platform_uade_prefix_count(void) {
    return (uint32_t)UADE_PREFIX_COUNT;
}

const char* sel_platform_uade_prefix(uint32_t i) {
    return i < UADE_PREFIX_COUNT ? k_uade_prefix[i] : "";
}

uint32_t sel_platform_of_ext(const char* ext) {
    char f[16];
    int row;

    if (ext == NULL) {
        return SEL_PLATFORM_FAMILY_MUSIC;
    }
    sel_platform_lower(f, ext, sizeof f);
    row = ext_row(f);
    return row >= 0 ? k_chips[row].family : SEL_PLATFORM_FAMILY_MUSIC;
}

const char* sel_platform_path_ext(const char* name) {
    const char *base, *p, *dot = NULL;

    if (name == NULL) {
        return NULL;
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
        }
    }
    return dot != NULL ? dot + 1 : NULL;
}

uint32_t sel_platform_of_name(const char* name) {
    const char *base, *p, *dot, *first = NULL;
    char f[16], pre[16];
    size_t n;
    int row;

    dot = sel_platform_path_ext(name);
    if (dot == NULL) {
        return SEL_PLATFORM_FAMILY_MUSIC;
    }
    base = name;
    for (p = name; *p != '\0'; p++) {
        if (*p == '/' || *p == ':') {
            base = p + 1;
        }
    }
    for (p = base; *p != '\0' && first == NULL; p++) {
        if (*p == '.') {
            first = p;
        }
    }
    sel_platform_lower(f, dot, sizeof f); /* the last extension */
    row = ext_row(f);
    if (row >= 0) {
        return k_chips[row].family;
    }
    n = (size_t)(first - base); /* the Amiga prefix */
    if (n > 0u && n < sizeof pre) {
        memcpy(pre, base, n);
        pre[n] = '\0';
        sel_platform_lower(f, pre, sizeof f);
        row = ext_row(f);
        if (row >= 0) {
            return k_chips[row].family;
        }
        if (uade_prefix(f)) {
            return SEL_PLATFORM_FAMILY_AMIGA;
        }
    }
    return SEL_PLATFORM_FAMILY_MUSIC;
}

void sel_platform_family_tally_add(sel_platform_family_tally* t, const char* name) {
    uint32_t f = sel_platform_of_name(name);
    const char* ext;

    t->n[f]++;
    if (f == SEL_PLATFORM_FAMILY_MUSIC && t->music_ext[0] == '\0') {
        ext = sel_platform_path_ext(name);
        if (ext != NULL) {
            sel_platform_lower(t->music_ext, ext, sizeof t->music_ext);
        }
    }
}

uint32_t sel_platform_family_tally_majority(const sel_platform_family_tally* t) {
    uint32_t f, best = SEL_PLATFORM_FAMILY_MUSIC, top = 0;

    for (f = SEL_PLATFORM_FAMILY_MUSIC + 1u; f < SEL_PLATFORM_FAMILY_COUNT; f++) { /* strictly greater: a tie keeps the earlier family */
        if (t->n[f] > top) {
            top = t->n[f];
            best = f;
        }
    }
    return best;
}

void sel_platform_family_tally_format(const sel_platform_family_tally* t, char* out, size_t cap) {
    uint32_t f = sel_platform_family_tally_majority(t), i, rows = sel_platform_ext_count();
    const char* ext = "";

    if (f == SEL_PLATFORM_FAMILY_MUSIC) {
        ext = t->music_ext;
    } else {
        for (i = 0; i < rows; i++) {
            if (k_chips[i].family == f) {
                ext = k_chips[i].ext;
                break;
            }
        }
    }
    sel_platform_lower(out, ext, cap);
}

const char* sel_platform_label(uint32_t family) {
    return family < SEL_PLATFORM_FAMILY_COUNT ? k_label[family] : k_label[0];
}

const char* sel_platform_id(uint32_t family) {
    return family < SEL_PLATFORM_FAMILY_COUNT ? k_id[family] : k_id[0];
}

