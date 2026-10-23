/* Share links (plan M3 Phase 7, row I180, decision D-M3-11).
 *   selecta://song/<source>/<path>[?sub=<n>&t=<ms>]
 *   selecta://playlist/<source>/<id>
 *   selecta://view/<urlencoded query>
 * and the DEViLBOX URL form, which carries the same three kinds as query parameters of the web player:
 *   https://devilbox.uprough.net/?song=<enc source/path>[&sub=<n>][&t=<ms>]
 *   https://devilbox.uprough.net/?playlist=<enc source/id>
 *   https://devilbox.uprough.net/?view=<enc query>
 * Only keys leave the machine, never local state beyond the path the user chose to share. */
#ifndef SELECTA_LIBRARY_SHARE_H
#define SELECTA_LIBRARY_SHARE_H

#include <stddef.h>
#include <stdint.h>

#define SEL_SHARE_SOURCE_MAX 16u
#define SEL_SHARE_PATH_MAX 1200u

#define SEL_SHARE_DEVILBOX_BASE "https://devilbox.uprough.net/"
#define SEL_SHARE_QUERY_MAX 512u

typedef enum sel_share_kind { SEL_SHARE_NONE = 0, SEL_SHARE_SONG, SEL_SHARE_PLAYLIST, SEL_SHARE_VIEW } sel_share_kind;

typedef struct sel_share_link {
    sel_share_kind kind;
    char source[SEL_SHARE_SOURCE_MAX]; /* song and playlist */
    char path[SEL_SHARE_PATH_MAX];   /* song: path within the source; playlist: id; view: the query text */
    int has_sub;
    uint32_t sub; /* subsong, 0-based like SUBSONG */
    int has_t;
    uint32_t t_ms;
} sel_share_link;

/* 1 when `text` starts like a share link (selecta:// or the DEViLBOX base), without validating it. */
int sel_share_is_link(const char* text);
/* Parses either form. 0 for anything malformed (the engine answers ERROR bad_link). */
int sel_share_parse(const char* text, sel_share_link* out);
/* Writes the selecta:// form / the DEViLBOX form. 0 when it does not fit or the link is incomplete. */
int sel_share_format(const sel_share_link* link, char* out, size_t n);
int sel_share_format_devilbox(const sel_share_link* link, char* out, size_t n);
/* The string OPEN takes for a song link: the plain path for the local source, else the song key
 * `<source>:<path>`. 0 when the link is not a song or does not fit. */
int sel_share_song_open_path(const sel_share_link* link, char* out, size_t n);
/* Song key `<source>:<path>` and its split: wrappers over source.h, which cannot be included next to playlist.h. */
int sel_share_key(const char* source, const char* path, char* out, size_t n);
int sel_share_key_split(const char* key, char* source, size_t sn, char* path, size_t pn);
/* Two links name the same target (kind, source, path, sub, t). */
int sel_share_equal(const sel_share_link* a, const sel_share_link* b);

#endif /* SELECTA_LIBRARY_SHARE_H */
