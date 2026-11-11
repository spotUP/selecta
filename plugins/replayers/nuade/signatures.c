/* Magic byte table for nuade content detection (I1305i2); see signatures.h for the why.
 *
 * Each row is one signature alternative of one player: every part must match, and the buffer must hold at least
 * `min_size` bytes. A part with len 0 means "the byte at offset is not zero". The source of each row is the header
 * gate of that player's probe (file:function noted per row); players whose probe parses the whole module, checks
 * code patterns at varying offsets or decides by file name have no row and fall back to the extension claim.
 * Not listed on purpose: audiosculpture (EXO4/FLT4 at 0x438 is the common 4-channel tracker mark; its probe only
 * says Unsure unless the extension is .adsc), pokeynoise and dynamicsynthesizer (file name decides). */
#include "signatures.h"

#include <string.h>

typedef struct {
    uint32_t offset;
    uint8_t len; /* 0: byte at offset is non-zero */
    const char* bytes;
} sig_part;

typedef struct {
    const char* player;
    uint32_t min_size;
    sig_part part[4];
} sig_row;

static const sig_row k_rows[] = {
    /* actionamics.c probe: "ACTIONAMICS SOUND TOOL" at 62 */
    { "actionamics", 90, { { 62, 22, "ACTIONAMICS SOUND TOOL" } } },
    /* amcomposer: 16-byte replay mark at 0 */
    { "amcomposer", 16, { { 0, 16, "AMC V1.2 REPLAY!" } } },
    { "andrewparton", 4, { { 0, 4, "BANK" } } },
    /* artandmagic: "EXIT" at 0x1A */
    { "artandmagic", 0x1E, { { 0x1A, 4, "EXIT" } } },
    { "cinemaware", 4, { { 0, 4, "IBLK" } } },
    { "davidhanney", 4, { { 0, 4, "DSNG" } } },
    { "deltamusic1", 4, { { 0, 4, "ALL " } } },
    /* deltamusic2: ".FNL" at 0xBC6, header to 0xFDA */
    { "deltamusic2", 0xFDA, { { 0xBC6, 4, ".FNL" } } },
    { "digmug", 204, { { 0, 24, " MUGICIAN/SOFTEYES 1990 " } } },
    { "digmug", 204, { { 0, 24, " MUGICIAN2/SOFTEYES 1990" } } },
    { "dss", 4, { { 0, 4, "MMU2" } } },
    { "emsv6", 10, { { 0, 10, "E.M.S. V6." } } },
    /* facethemusic: "FTM" and version byte 3 */
    { "facethemusic", 5, { { 0, 3, "FTM" }, { 4, 1, "\x03" } } },
    /* fashiontracker: fixed replay code at the start of the file */
    { "fashiontracker", 0x1A,
      { { 0, 4, "\x13\xFC\x00\x40" }, { 8, 4, "\x4E\x71\x04\x39" }, { 12, 2, "\x00\x01" }, { 0x12, 8, "\x66\xF4\x4E\x75\x48\xE7\xFF\xFE" } } },
    { "fcbsi", 4, { { 0, 4, "FUCO" } } },
    { "forgottenworlds", 4, { { 0, 4, "FWMP" } } },
    /* futurecomposer.c fc_create: size >= 100, FC14 or SMOD */
    { "futurecomposer", 100, { { 0, 4, "FC14" } } },
    { "futurecomposer", 100, { { 0, 4, "SMOD" } } },
    /* hippel: COSO packed TFMX */
    { "hippel", 0x24, { { 0, 4, "COSO" }, { 0x20, 4, "TFMX" } } },
    { "instereo1", 8, { { 0, 8, "ISM!V1.2" } } },
    { "instereo2", 8, { { 0, 8, "IS20DF10" } } },
    { "jamcracker", 4, { { 0, 4, "BeEp" } } },
    { "markcooksey", 6, { { 0, 4, "\xD0\x40\xD0\x40" }, { 4, 2, "\x4E\xFB" } } },
    { "mcmd", 4, { { 0, 4, "MCMD" } } },
    { "oktalyzer", 1368, { { 0, 8, "OKTASONG" } } },
    /* protracker4.c parse_module: IFF FORM of type MODL starting with a VERS chunk */
    { "protracker4", 16, { { 0, 4, "FORM" }, { 8, 4, "MODL" }, { 12, 4, "VERS" } } },
    { "quadracomposer", 12, { { 0, 4, "FORM" }, { 8, 8, "EMODEMIC" } } },
    { "richardjoseph", 8, { { 0, 3, "RJP" }, { 4, 4, "SMOD" } } },
    { "ronklaren", 63, { { 0, 4, "\x00\x00\x03\xF3" }, { 40, 23, "RON_KLAREN_SOUNDMODULE!" } } },
    { "sidmon2", 86, { { 58, 28, "SIDMON II - THE MIDI VERSION" } } },
    { "sonicarranger", 8, { { 0, 8, "SOARV1.0" } } },
    /* soundfx.c sfx_find_layout: SONG at 60 (15 samples) or SO31 at 124 (31 samples) */
    { "soundfx", 660, { { 60, 4, "SONG" } } },
    { "soundfx", 1204, { { 124, 4, "SO31" } } },
    /* soundmon.c sniff_type: version mark at 26, size >= 512 */
    { "soundmon", 512, { { 26, 3, "V.2" } } },
    { "soundmon", 512, { { 26, 4, "BPSM" } } },
    { "soundmon", 512, { { 26, 3, "V.3" } } },
    { "synthesis", 8, { { 0, 8, "Synth4.0" } } },
    { "synthesis", 0x1F16, { { 0x1F0E, 8, "Synth4.2" } } },
    { "syntracker", 16, { { 0, 16, "SYNTRACKER-SONG:" } } },
    /* timetracker: "TMK" and a non-zero pattern count */
    { "timetracker", 4, { { 0, 3, "TMK" }, { 3, 0, NULL } } },
    /* ufo_song.c has_tags: FORM....DDAT BODY .... CHAN */
    { "ufo", 0x18, { { 0, 4, "FORM" }, { 8, 4, "DDAT" }, { 12, 4, "BODY" }, { 0x14, 4, "CHAN" } } },
};

size_t sel_nuade_sig_count(void) {
    return sizeof k_rows / sizeof k_rows[0];
}

const char* sel_nuade_sig_player(size_t i) {
    return i < sel_nuade_sig_count() ? k_rows[i].player : NULL;
}

int sel_nuade_sig_match_entry(size_t i, const uint8_t* data, size_t size) {
    const sig_row* r;
    size_t k;
    if (i >= sel_nuade_sig_count() || data == NULL) {
        return 0;
    }
    r = &k_rows[i];
    if (size < r->min_size) {
        return 0;
    }
    for (k = 0; k < sizeof r->part / sizeof r->part[0]; k++) {
        const sig_part* p = &r->part[k];
        if (p->len == 0u && p->bytes == NULL && k > 0u && p->offset == 0u) {
            break; /* unused slot */
        }
        if (p->len == 0u) {
            if ((size_t)p->offset >= size || data[p->offset] == 0u) {
                return 0;
            }
            continue;
        }
        if ((size_t)p->offset + p->len > size || memcmp(data + p->offset, p->bytes, p->len) != 0) {
            return 0;
        }
    }
    return 1;
}

const char* sel_nuade_sig_match(const uint8_t* data, size_t size) {
    size_t i;
    for (i = 0; i < sel_nuade_sig_count(); i++) {
        if (sel_nuade_sig_match_entry(i, data, size)) {
            return k_rows[i].player;
        }
    }
    return NULL;
}

static int list_has_nocase(const char* list, const char* s, size_t n) {
    while (list != NULL && *list != '\0') {
        const char* e = strchr(list, ',');
        size_t l = e != NULL ? (size_t)(e - list) : strlen(list);
        if (l == n) {
            size_t j = 0;
            while (j < n && (list[j] | 0x20) == (s[j] | 0x20)) {
                j++;
            }
            if (j == n) {
                return 1;
            }
        }
        list = e != NULL ? e + 1 : NULL;
    }
    return 0;
}

int sel_nuade_sig_name_claimed(const char* exts, const char* name) {
    const char* first;
    const char* last;
    if (exts == NULL || name == NULL || (first = strchr(name, '.')) == NULL) {
        return 0;
    }
    last = strrchr(name, '.') + 1;
    return (first > name && list_has_nocase(exts, name, (size_t)(first - name))) ||
           (*last != '\0' && list_has_nocase(exts, last, strlen(last)));
}
