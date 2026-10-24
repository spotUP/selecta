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
