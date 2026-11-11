/* Magic byte table for nuade content detection (I1305i2); see signatures.h for the why.
 *
 * Each row is one signature alternative of one player: every part must match, and the buffer must hold at least
 * `min_size` bytes. A part with len 0 means "the byte at offset is not zero". The source of each row is the header
 * gate of that player's probe (file:function noted per row); players whose probe parses the whole module, checks
 * code patterns at varying offsets or decides by file name have no row and fall back to the extension claim.
 *
 * A row marked `unsure` mirrors a probe that by content alone only says Unsure (I1305i3): its hit makes the manifest
 * answer Unsure, never a sure claim, so a replayer that is sure of the file still wins. audiosculpture is the case:
 * EXO4/FLT4 at 0x438 is also the common 4-channel tracker mark, and a sure row would steal those tracker mods.
 * `repeat`/`stride`: parts after the first are also tried shifted by j * stride for j < repeat (any shift may match),
 * for probes that loop over a table of fixed-size records.
 *
 * Without a row on purpose (I1305i3; extension claim only): pokeynoise and dynamicsynthesizer (file name decides),
 * dsc (field range checks, no mark), pumatracker and zoundmonitor (the mark sits at an offset computed from header
 * counts), infogrames (the first word is the offset of the checked envelope bytes), earache (three header layouts,
 * one without the EASO mark).
 * Also without a row (I1305i3 part 2): activisionpro, ashleyhogg, bendaglish, bendaglishsid, dariuszendeh, desire,
 * davidwhittaker, fredgray (the probe scans the replayer code for instruction patterns; the fixed bytes at the start
 * are BRA.W opcodes or moveq/rts shared by many replayer executables), hippel7v and dirkbialluch (the header and the
 * tags are searched for, not at a fixed offset), ems and gmc (field range checks, no mark), davelowe (three layouts:
 * game driver, Deli module, new driver), fred_np (three layouts: "Fred Editor ", the Final replayer's 0x4EFA jump
 * table, and either wrapped in a hunk executable).
 * Also without a row (I1305i3 part 3): hippelhip and maniacsofnoise (a branch or jump table at 0, the header is found
 * by following the code), jeroentel (code pattern searched in the first bytes), jasonbrooke and jesperolsen (several
 * replayers; version A is found by code search or a code hash, so no single header covers the player),
 * kimchristensen, markii, mikedavies and markcooksey_old (file name decides), maximumeffect (field range checks, no
 * mark). */
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
    uint8_t unsure; /* 1: a hit is only Unsure (the player's own probe never says Supported by content) */
    uint8_t repeat; /* parts 1.. also tried at offset + j * stride, j < repeat; 0 or 1: no shift */
    uint8_t stride;
} sig_row;

static const sig_row k_rows[] = {
    /* actionamics.c probe: "ACTIONAMICS SOUND TOOL" at 62 */
    { "actionamics", 90, { { 62, 22, "ACTIONAMICS SOUND TOOL" } }, 0, 0, 0 },
    /* amcomposer: 16-byte replay mark at 0 */
    { "amcomposer", 16, { { 0, 16, "AMC V1.2 REPLAY!" } }, 0, 0, 0 },
    /* amos.c read_header: "AmBk", length long flagged 0x80 and "Music   " at 12; without "AmBk" the length long
     * starts with 0xC0 and "Music   " sits at 8. Size >= 0x30. */
    { "amos", 0x30, { { 0, 4, "AmBk" }, { 8, 1, "\x80" }, { 12, 8, "Music   " } }, 0, 0, 0 },
    { "amos", 0x30, { { 0, 1, "\xC0" }, { 8, 8, "Music   " } }, 0, 0, 0 },
    /* andersoland.c probe_chunks: chunk "mpl" (3-byte tag) at 0 with an 8-byte chunk header */
    { "andersoland", 8, { { 0, 3, "mpl" } }, 0, 0, 0 },
    { "andrewparton", 4, { { 0, 4, "BANK" } }, 0, 0, 0 },
    /* aprosys_module.c aps_module_parse: "ADRVPACK" at 0 */
    { "aprosys", 8, { { 0, 8, "ADRVPACK" } }, 0, 0, 0 },
    /* artandmagic: "EXIT" at 0x1A */
    { "artandmagic", 0x1E, { { 0x1A, 4, "EXIT" } }, 0, 0, 0 },
    /* audiosculpture.c asc_probe: EXO4/FLT4 at 0x438 and one of the first 32 sample headers (30 bytes from 20) with
     * length 0 (word at +22) and a non-zero volume (+25). Unsure: the probe says Supported only for .adsc names. */
    { "audiosculpture", 0x43C, { { 0x438, 4, "EXO4" }, { 42, 2, "\0\0" }, { 45, 0, NULL } }, 1, 32, 30 },
    { "audiosculpture", 0x43C, { { 0x438, 4, "FLT4" }, { 42, 2, "\0\0" }, { 45, 0, NULL } }, 1, 32, 30 },
    /* beathoven_module.c check2 + bth_module_open: hunk header 0x3F3 with one hunk, memory flags byte 0x14 set, code
     * at 0x20 = moveq #-1,d0; rts, "BEATHOVE", version N100/N109/NNEW; size >= 0x20 + 16 + 15 longs */
    { "beathoven", 0x6C,
      { { 0, 4, "\0\0\x03\xF3" }, { 8, 4, "\0\0\0\x01" }, { 0x14, 0, NULL }, { 0x20, 16, "\x70\xFF\x4E\x75" "BEATHOVEN100" } },
      0, 0, 0 },
    { "beathoven", 0x6C,
      { { 0, 4, "\0\0\x03\xF3" }, { 8, 4, "\0\0\0\x01" }, { 0x14, 0, NULL }, { 0x20, 16, "\x70\xFF\x4E\x75" "BEATHOVEN109" } },
      0, 0, 0 },
    { "beathoven", 0x6C,
      { { 0, 4, "\0\0\x03\xF3" }, { 8, 4, "\0\0\0\x01" }, { 0x14, 0, NULL }, { 0x20, 16, "\x70\xFF\x4E\x75" "BEATHOVENNEW" } },
      0, 0, 0 },
    { "cinemaware", 4, { { 0, 4, "IBLK" } }, 0, 0, 0 },
    /* cd_module.c load_hunk + has_magic: hunk header 0x3F3, no library names, one hunk (first = last = 0); the hunk
     * data at 32 starts with moveq #-1,d0; rts, "S.PH", "IPPS" */
    { "coredesign", 44,
      { { 0, 20, "\0\0\x03\xF3" "\0\0\0\0" "\0\0\0\x01" "\0\0\0\0" "\0\0\0\0" }, { 32, 12, "\x70\xFF\x4E\x75" "S.PHIPPS" } },
      0, 0, 0 },
    { "davidhanney", 4, { { 0, 4, "DSNG" } }, 0, 0, 0 },
    { "deltamusic1", 4, { { 0, 4, "ALL " } }, 0, 0, 0 },
    /* deltamusic2: ".FNL" at 0xBC6, header to 0xFDA */
    { "deltamusic2", 0xFDA, { { 0xBC6, 4, ".FNL" } }, 0, 0, 0 },
    { "digmug", 204, { { 0, 24, " MUGICIAN/SOFTEYES 1990 " } }, 0, 0, 0 },
    { "digmug", 204, { { 0, 24, " MUGICIAN2/SOFTEYES 1990" } }, 0, 0, 0 },
    { "dss", 4, { { 0, 4, "MMU2" } }, 0, 0, 0 },
    { "emsv6", 10, { { 0, 10, "E.M.S. V6." } }, 0, 0, 0 },
    /* facethemusic: "FTM" and version byte 3 */
    { "facethemusic", 5, { { 0, 3, "FTM" }, { 4, 1, "\x03" } }, 0, 0, 0 },
    /* fashiontracker: fixed replay code at the start of the file */
    { "fashiontracker", 0x1A,
      { { 0, 4, "\x13\xFC\x00\x40" }, { 8, 4, "\x4E\x71\x04\x39" }, { 12, 2, "\x00\x01" }, { 0x12, 8, "\x66\xF4\x4E\x75\x48\xE7\xFF\xFE" } }, 0, 0, 0 },
    { "fcbsi", 4, { { 0, 4, "FUCO" } }, 0, 0, 0 },
    { "forgottenworlds", 4, { { 0, 4, "FWMP" } }, 0, 0, 0 },
    /* futurecomposer.c fc_create: size >= 100, FC14 or SMOD */
    { "futurecomposer", 100, { { 0, 4, "FC14" } }, 0, 0, 0 },
    { "futurecomposer", 100, { { 0, 4, "SMOD" } }, 0, 0, 0 },
    /* futureplayer.c fp_hunk_parse: hunk header 0x3F3, no library names, one hunk (first = last = 0), memory flags byte
     * 20 set, HUNK_CODE at 24, code at 32 = moveq #-1,d0; rts, "F.PLAYER" */
    { "futureplayer", 44,
      { { 0, 20, "\0\0\x03\xF3" "\0\0\0\0" "\0\0\0\x01" "\0\0\0\0" "\0\0\0\0" },
        { 20, 0, NULL },
        { 24, 4, "\0\0\x03\xE9" },
        { 32, 12, "\x70\xFF\x4E\x75" "F.PLAYER" } },
      0, 0, 0 },
    /* gm_module.c gm_module_parse: "GLUE" and its bitwise complement */
    { "gluemon", 8, { { 0, 4, "GLUE" }, { 4, 4, "\xB8\xB3\xAA\xBA" } }, 0, 0, 0 },
    /* hippel: COSO packed TFMX */
    { "hippel", 0x24, { { 0, 4, "COSO" }, { 0x20, 4, "TFMX" } }, 0, 0, 0 },
    /* hd_module.c load_hunks + read_header: one-hunk executable (0x3F3, 0, 1, 0, 0, size word non-zero, HUNK_CODE), code at 0x20 starts
     * moveq #-1,d0; rts and "H.DAVIES" (Check2) */
    { "howiedavies", 44,
      { { 0, 20, "\0\0\x03\xF3" "\0\0\0\0" "\0\0\0\x01" "\0\0\0\0" "\0\0\0\0" },
        { 20, 0, NULL },
        { 24, 4, "\0\0\x03\xE9" },
        { 32, 12, "\x70\xFF\x4E\x75" "H.DAVIES" } },
      0, 0, 0 },
    /* jmf_module.c load_hunks + read_header: one-hunk executable (0x3F3, 0, 1, 0, 0, size word non-zero, HUNK_CODE), code at 0x20 starts
     * moveq #-1,d0; rts and "J.FLOGEL" (Check2) */
    { "jankomrsicflogel", 44,
      { { 0, 20, "\0\0\x03\xF3" "\0\0\0\0" "\0\0\0\x01" "\0\0\0\0" "\0\0\0\0" },
        { 20, 0, NULL },
        { 24, 4, "\0\0\x03\xE9" },
        { 32, 12, "\x70\xFF\x4E\x75" "J.FLOGEL" } },
      0, 0, 0 },
    { "instereo1", 8, { { 0, 8, "ISM!V1.2" } }, 0, 0, 0 },
    { "instereo2", 8, { { 0, 8, "IS20DF10" } }, 0, 0, 0 },
    { "jamcracker", 4, { { 0, 4, "BeEp" } }, 0, 0, 0 },
    /* laxity.c check_module: 0x18-byte records, flag byte 0xFF in records 1 and 2 (0x1F, 0x37), and either the known
     * header (0x00820048 at 0, record 2 = 00010008 001800FF) or the default title "YET ANOTHER COOL" in record 0 */
    { "laxity", 0x38, { { 0, 4, "\0\x82\0\x48" }, { 0x1F, 1, "\xFF" }, { 0x30, 8, "\0\x01\0\x08\0\x18\0\xFF" } }, 0, 0, 0 },
    { "laxity", 0x38, { { 8, 16, "YET ANOTHER COOL" }, { 0x1F, 1, "\xFF" }, { 0x37, 1, "\xFF" } }, 0, 0, 0 },
    /* lme_module.c lme_module_parse: "LME\0" at 0, zero long at 0x24, tables to 0x48 */
    { "legglessmusiceditor", 0x48, { { 0, 4, "LME\0" }, { 0x24, 4, "\0\0\0\0" } }, 0, 0, 0 },
    /* mt_module.c: "ARP." at 0 */
    { "majortom", 4, { { 0, 4, "ARP." } }, 0, 0, 0 },
    { "markcooksey", 6, { { 0, 4, "\xD0\x40\xD0\x40" }, { 4, 2, "\x4E\xFB" } }, 0, 0, 0 },
    { "mcmd", 4, { { 0, 4, "MCMD" } }, 0, 0, 0 },
    /* mdl_module.c: "MSOB" at 0, header 20 bytes */
    { "medley", 20, { { 0, 4, "MSOB" } }, 0, 0, 0 },
    /* ntsp.c module_read: "SPNT" at 0, header 8 bytes */
    { "ntsp", 8, { { 0, 4, "SPNT" } }, 0, 0, 0 },
    { "oktalyzer", 1368, { { 0, 8, "OKTASONG" } }, 0, 0, 0 },
    /* professionalsoundartists.c: "PSA" and a zero byte at 0, subsong table to 0x40 */
    { "professionalsoundartists", 0x40, { { 0, 4, "PSA\0" } }, 0, 0, 0 },
    /* protracker4.c parse_module: IFF FORM of type MODL starting with a VERS chunk */
    { "protracker4", 16, { { 0, 4, "FORM" }, { 8, 4, "MODL" }, { 12, 4, "VERS" } }, 0, 0, 0 },
    { "quadracomposer", 12, { { 0, 4, "FORM" }, { 8, 8, "EMODEMIC" } }, 0, 0, 0 },
    { "richardjoseph", 8, { { 0, 3, "RJP" }, { 4, 4, "SMOD" } }, 0, 0, 0 },
    { "ronklaren", 63, { { 0, 4, "\x00\x00\x03\xF3" }, { 40, 23, "RON_KLAREN_SOUNDMODULE!" } }, 0, 0, 0 },
    { "sidmon2", 86, { { 58, 28, "SIDMON II - THE MIDI VERSION" } }, 0, 0, 0 },
    { "sonicarranger", 8, { { 0, 8, "SOARV1.0" } }, 0, 0, 0 },
    /* sonix.c: IFF FORM of type SMUS; the probe says Supported by content only for SMUS (SNX/TINY need the name) */
    { "sonix", 24, { { 0, 4, "FORM" }, { 8, 4, "SMUS" } }, 0, 0, 0 },
    /* soundfx.c sfx_find_layout: SONG at 60 (15 samples) or SO31 at 124 (31 samples) */
    { "soundfx", 660, { { 60, 4, "SONG" } }, 0, 0, 0 },
    { "soundfx", 1204, { { 124, 4, "SO31" } }, 0, 0, 0 },
    /* soundmon.c sniff_type: version mark at 26, size >= 512 */
    { "soundmon", 512, { { 26, 3, "V.2" } }, 0, 0, 0 },
    { "soundmon", 512, { { 26, 4, "BPSM" } }, 0, 0, 0 },
    { "soundmon", 512, { { 26, 3, "V.3" } }, 0, 0, 0 },
    /* ss_module.c check_magic: "SPEEDY-SYSTEM" at 0 */
    { "speedysystem", 13, { { 0, 13, "SPEEDY-SYSTEM" } }, 0, 0, 0 },
    { "synthesis", 8, { { 0, 8, "Synth4.0" } }, 0, 0, 0 },
    { "synthesis", 0x1F16, { { 0x1F0E, 8, "Synth4.2" } }, 0, 0, 0 },
    /* sp_module.c sp_song_layout: "OBISYNTHPACK" at 0, notes at 0x50C */
    { "synthpack", 0x510, { { 0, 12, "OBISYNTHPACK" } }, 0, 0, 0 },
    { "syntracker", 16, { { 0, 16, "SYNTRACKER-SONG:" } }, 0, 0, 0 },
    /* timetracker: "TMK" and a non-zero pattern count */
    { "timetracker", 4, { { 0, 3, "TMK" }, { 3, 0, NULL } }, 0, 0, 0 },
    /* ufo_song.c has_tags: FORM....DDAT BODY .... CHAN */
    { "ufo", 0x18, { { 0, 4, "FORM" }, { 8, 4, "DDAT" }, { 12, 4, "BODY" }, { 0x14, 4, "CHAN" } }, 0, 0, 0 },
};

size_t sel_nuade_sig_count(void) {
    return sizeof k_rows / sizeof k_rows[0];
}

const char* sel_nuade_sig_player(size_t i) {
    return i < sel_nuade_sig_count() ? k_rows[i].player : NULL;
}

int sel_nuade_sig_entry_unsure(size_t i) {
    return i < sel_nuade_sig_count() && k_rows[i].unsure != 0u;
}

/* Parts first..3 of row r match with parts after the first shifted by `shift` bytes. */
static int parts_match(const sig_row* r, size_t first, size_t end, size_t shift, const uint8_t* data, size_t size) {
    size_t k;
    for (k = first; k < end; k++) {
        const sig_part* p = &r->part[k];
        size_t at;
        if (p->len == 0u && p->bytes == NULL && k > 0u && p->offset == 0u) {
            break; /* unused slot */
        }
        at = (size_t)p->offset + (k > 0u ? shift : 0u);
        if (p->len == 0u) {
            if (at >= size || data[at] == 0u) {
                return 0;
            }
            continue;
        }
        if (at + p->len > size || memcmp(data + at, p->bytes, p->len) != 0) {
            return 0;
        }
    }
    return 1;
}

int sel_nuade_sig_match_entry(size_t i, const uint8_t* data, size_t size) {
    const sig_row* r;
    size_t j;
    if (i >= sel_nuade_sig_count() || data == NULL) {
        return 0;
    }
    r = &k_rows[i];
    /* part 0 sits at its fixed offset; parts 1.. are tried at each shift */
    if (size < r->min_size || !parts_match(r, 0, 1, 0, data, size)) {
        return 0;
    }
    for (j = 0; j < (r->repeat > 1u ? r->repeat : 1u); j++) {
        if (parts_match(r, 1, sizeof r->part / sizeof r->part[0], j * r->stride, data, size)) {
            return 1;
        }
    }
    return 0;
}

const char* sel_nuade_sig_match(const uint8_t* data, size_t size) {
    size_t i;
    for (i = 0; i < sel_nuade_sig_count(); i++) {
        if (!k_rows[i].unsure && sel_nuade_sig_match_entry(i, data, size)) {
            return k_rows[i].player;
        }
    }
    return NULL;
}

int sel_nuade_sig_probe_level(const uint8_t* data, size_t size, const char* name, const char* exts) {
    const char* base = name;
    const char* p;
    size_t i;
    if (sel_nuade_sig_match(data, size) != NULL) {
        return 2;
    }
    for (i = 0; i < sel_nuade_sig_count(); i++) {
        if (k_rows[i].unsure && sel_nuade_sig_match_entry(i, data, size)) {
            return 1;
        }
    }
    for (p = name; p != NULL && *p != '\0'; p++) {
        if (*p == '/' || *p == ':') {
            base = p + 1;
        }
    }
    return sel_nuade_sig_name_claimed(exts, base) ? 1 : 0;
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
