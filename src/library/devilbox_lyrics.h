/* DEViLBOX lyrics endpoint client (I193, F19, D-M3-13): GET <devilbox>/api/lyrics/<md5> through the one HTTP client.
 * The hash is the lower-case md5 of the file bytes, the same key as the loudness endpoint (devilbox_loudness.h). The
 * route does not exist upstream yet (research 2026-10-25-m3-external-endpoints.md: "lyrics: absent"), so this is the
 * shape the client expects and the fixture serves: 200 with the lyrics as text/plain (LRC or plain text), 404 (the
 * server's body is {"error":"unknown md5"}, routes/lyrics.ts) when the endpoint is there but holds nothing for the
 * file; any 404 counts, whatever the body. Anything else (5xx, network down) is a missing endpoint and the song plays without lyrics (devilbox_missing=lyrics). */
#ifndef SELECTA_LIBRARY_DEVILBOX_LYRICS_H
#define SELECTA_LIBRARY_DEVILBOX_LYRICS_H

#include <stddef.h>

#define SEL_DVB_LYRICS_MAX 8192u /* bytes of lyrics kept; a longer answer is cut */

typedef enum sel_dvb_lyrics_status {
    SEL_DVB_LYRICS_HIT = 1,     /* 200: text stored */
    SEL_DVB_LYRICS_UNKNOWN = 0, /* 404, any body: the endpoint is there, no lyrics for this file */
    SEL_DVB_LYRICS_MISSING = -1 /* route missing, network down, 429/5xx: degrade to local only */
} sel_dvb_lyrics_status;

/* GET /api/lyrics/<hash> into out (NUL terminated, at most cap - 1 bytes; *len optional). */
sel_dvb_lyrics_status sel_dvb_lyrics_get(const char* hash, char* out, size_t cap, size_t* len);

/* Same, and *http_status (optional) gets the HTTP status of the answer, 0 when no answer came (network error). */
sel_dvb_lyrics_status sel_dvb_lyrics_get_ex(const char* hash, char* out, size_t cap, size_t* len, int* http_status);

/* I193b: POST <devilbox>/api/lyrics/<hash>, body = raw LRC, Bearer token. Returns the HTTP status (204 stored), 0 on a
 * network error or when the body is empty or over SEL_DVB_LYRICS_UPLOAD_MAX. */
#define SEL_DVB_LYRICS_UPLOAD_MAX 65536u
int sel_dvb_lyrics_put(const char* hash, const char* token, const char* text, size_t len);

#endif
