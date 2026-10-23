/* In-memory playlist and its four codecs (plan M1.2 phase 3, row I72;
 * design doc section 3, PLAYLIST_LOAD / PLAYLIST_SAVE). No allocation: the
 * caller supplies the entry array and the text buffer. A key is the entry's
 * identity string (a local path or a URL); path resolution against the
 * playlist directory belongs to the verb (row I73), not to the codecs. */
#ifndef SELECTA_LIBRARY_PLAYLIST_H
#define SELECTA_LIBRARY_PLAYLIST_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
    SEL_PL_SONG = 0,
    SEL_PL_DIVIDER = 1, /* name only, never played */
    SEL_PL_STEALTH = 2  /* a song played but not shown in the regular style */
} sel_pl_kind;

typedef enum {
    SEL_PL_AUTO = 0, /* read only: sniff the first bytes */
    SEL_PL_HIPPO,
    SEL_PL_M3U,
    SEL_PL_PLS,
    SEL_PL_XSPF
} sel_pl_format;

/* Status codes of read and write. */
#define SEL_PL_OK 0
#define SEL_PL_RANGE (-1)  /* too many entries or text does not fit */
#define SEL_PL_FORMAT (-2) /* bytes are not this format / unknown format */
#define SEL_PL_SINK (-3)   /* the sink refused bytes */

/* Hippo files hold at most 131071 entries (MAX_MODULES). */
#define SEL_PL_HIPPO_MAX 131071u

typedef struct {
    uint8_t kind;         /* sel_pl_kind */
    uint8_t subsong;      /* 0 = none, else 1..255 */
    uint32_t key;         /* offset into text; 0 = empty */
    uint32_t name;        /* offset into text; 0 = empty */
    uint32_t duration_ms; /* 0 = unknown */
} sel_pl_entry;

typedef struct {
    sel_pl_entry* entries;
    uint32_t count;
    uint32_t cap;
    char* text; /* text[0] is always NUL so offset 0 is the empty string */
    uint32_t text_len;
    uint32_t text_cap;
} sel_playlist;

/* Warning callback: msg is a short token such as "lossy divider". */
typedef void (*sel_pl_warn_fn)(void* ctx, const char* msg);
/* Byte sink for writers. Return 0 to refuse (write answers SEL_PL_SINK). */
typedef int (*sel_pl_sink_fn)(void* ctx, const char* bytes, uint32_t len);

void sel_playlist_init(sel_playlist* pl, sel_pl_entry* entries, uint32_t cap, char* text, uint32_t text_cap);
const char* sel_playlist_str(const sel_playlist* pl, uint32_t offset);
/* Appends an entry; strings are copied. Returns 0 or SEL_PL_RANGE. */
int sel_playlist_add(sel_playlist* pl, sel_pl_kind kind, const char* key, size_t key_len, const char* name, size_t name_len,
                     uint8_t subsong, uint32_t duration_ms);

/* By extension (.prg .m3u .m3u8 .pls .xspf, case-insensitive), else by
 * sniffing (HiPPrg, #EXTM3U, [playlist], <?xml). SEL_PL_AUTO = unknown. */
sel_pl_format sel_playlist_format_from_name(const char* name);
sel_pl_format sel_playlist_sniff(const char* bytes, size_t len);

/* Appends the entries of `bytes` to pl. fmt SEL_PL_AUTO sniffs. */
int sel_playlist_read(sel_pl_format fmt, const char* bytes, size_t len, sel_playlist* pl, sel_pl_warn_fn warn, void* warn_ctx);
int sel_playlist_write(sel_pl_format fmt, const sel_playlist* pl, sel_pl_sink_fn sink, void* sink_ctx, sel_pl_warn_fn warn,
                       void* warn_ctx);

#endif /* SELECTA_LIBRARY_PLAYLIST_H */
