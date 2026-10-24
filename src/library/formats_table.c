/* Modland format names (plan Phase 4, I157; design doc 9.1). The names are Modland's directory names
 * under pub/modules, shared by every source; the replayer for a name is found through the plugin
 * registry (sel_registry_format_owner), not stored here. */
#include "local.h"
#include "source.h"

#include <ctype.h>
#include <string.h>

static const sel_format_entry g_formats[] = {
    {"Protracker", "mod"},
    {"Fasttracker 2", "xm"},
    {"Scream Tracker 3", "s3m"},
    {"Impulse Tracker", "it"},
    {"Octamed", "med"},
    {"Octamed MMD0", "mmd0"},
    {"Octamed MMD1", "mmd1"},
    {"Octamed MMD2", "mmd2"},
    {"Octamed MMD3", "mmd3"},
    {"Soundtracker", "stk"},
    {"Noisetracker", "nst"},
    {"Startrekker", "mod"},
    {"Oktalyzer", "okt"},
    {"Digibooster", "digi"},
    {"Digibooster Pro", "dbm"},
    {"Farandole Composer", "far"},
    {"Ultratracker", "ult"},
    {"MultiTracker", "mtm"},
    {"Composer 669", "669"},
    {"Graoumf Tracker", "gtk"},
    {"Symphonie", "symmod"},
    {"Future Composer 1.4", "fc14"},
    {"Future Composer 1.3", "fc13"},
    {"Hippel", "hip"},
    {"Hippel COSO", "hipc"},
    {"Hippel ST", "hip7"},
    {"David Whittaker", "dw"},
    {"Fred", "fred"},
    {"SoundMon 2.2", "bp"},
    {"SoundMon 3", "bp3"},
    {"Sidmon 1", "sid1"},
    {"Sidmon 2", "sid2"},
    {"Delta Music 1", "dm1"},
    {"Delta Music 2", "dm2"},
    {"Sound FX", "sfx"},
    {"Jochen Hippel", "hip"},
    {"TFMX", "tfx"},
    {"Mark Cooksey", "mc"},
    {"Rob Hubbard", "rh"},
    {"Jason Page", "jpn"},
    {"SID", "sid"},
    {"AdLib", "rad"},
    {"Ayumi", "ay"},
    {"SNDH", "sndh"},
    {"YM", "ym"},
    {"Nintendo SPC", "spc"},
    {"Game Boy Sound Format", "gbs"},
    {"HES", "hes"},
    {"NSF", "nsf"},
    {"VGM", "vgm"},
};

#define FORMAT_COUNT (sizeof g_formats / sizeof g_formats[0])

static int equal_nocase(const char* a, const char* b) {
    for (; *a != '\0' && *b != '\0'; a++, b++) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) {
            return 0;
        }
    }
    return *a == *b;
}

uint32_t sel_format_count(void) {
    return (uint32_t)FORMAT_COUNT;
}

const sel_format_entry* sel_format_get(uint32_t index) {
    return index < FORMAT_COUNT ? &g_formats[index] : NULL;
}

const sel_format_entry* sel_format_by_name(const char* name) {
    uint32_t i;

    for (i = 0; name != NULL && i < FORMAT_COUNT; i++) {
        if (equal_nocase(g_formats[i].name, name)) {
            return &g_formats[i];
        }
    }
    return NULL;
}

const sel_format_entry* sel_format_by_ext(const char* ext) {
    uint32_t i;

    if (ext != NULL && ext[0] == '.') {
        ext++;
    }
    for (i = 0; ext != NULL && ext[0] != '\0' && i < FORMAT_COUNT; i++) {
        if (equal_nocase(g_formats[i].ext, ext)) {
            return &g_formats[i];
        }
    }
    return NULL;
}

/* Extensions and Amiga prefixes of files the replayers play that the Modland table above does not list
 * (it holds Modland directory names, one usual extension each). Used only by sel_format_known_file. */
static const char* const g_extra_names[] = {
    "mp3", "mp2", "ogg", "oga", "wav", "flac", "aif", "aiff", "ahx", "hvl", "p61", "p61a", "pt", "stm",
    "ice", "dmf", "amf", "ams", "dsm", "psm", "mdl", "j2b", "umx", "mo3", "xmz", "s3z", "itz", "mdz",
    "mdat", "smpl", "sc68", "aon", "cust", "cus", "mus", "sa", "sb", "sng", "ss", "tp", "tw", "ma", "fp",
    "gmc", "ml", "mm4", "mm8", "ps", "pt3", "psc", "pn", "pr", "puma", "scn", "sdr", "sap", "sjs", "wb",
    "tme", "thm", "tf", "vss", "cm", "dlt", "ems", "dns", "hd", "kris", "mkii", "bd", "bds",
    "dz", "ex", "fw", "fuchs", "jd", "jmf", "jt", "mcmd", "mso", "np", "np1", "np2", "np3", "ntp", "pap", "pvp",
    "rho", "riff", "sfx13", "dm", "fc", "dum", "jp", "mcr", "mco", "rjp", "sm", "tfmx", "emod", "okta", "oct",
    "ptm", "plm", "gt2", "rol", "mid", "midi", "vgz", "nsfe", "kss", "gym", "psf", "minipsf", "usf", "ssf",
    "dsf", "gsf",
};

static int extra_name(const char* s, size_t n) {
    size_t i;

    for (i = 0; i < sizeof g_extra_names / sizeof g_extra_names[0]; i++) {
        if (strlen(g_extra_names[i]) == n) {
            size_t j;
            int same = 1;

            for (j = 0; same && j < n; j++) {
                same = tolower((unsigned char)s[j]) == g_extra_names[i][j];
            }
            if (same) {
                return 1;
            }
        }
    }
    return 0;
}

static int known_name(const char* s, size_t n) {
    char buf[16];

    if (n == 0u || n >= sizeof buf) {
        return 0;
    }
    memcpy(buf, s, n);
    buf[n] = '\0';
    return sel_format_by_ext(buf) != NULL || extra_name(s, n);
}

int sel_format_known_file(const char* name) {
    const char* dot;
    const char* last;

    if (name == NULL) {
        return 0;
    }
    dot = strchr(name, '.');
    if (dot == NULL) {
        return 0;
    }
    if (known_name(name, (size_t)(dot - name))) { /* Amiga prefix name: mod.songname, mdat.x */
        return 1;
    }
    last = strrchr(name, '.') + 1;
    return known_name(last, strlen(last));
}
