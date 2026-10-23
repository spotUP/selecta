/* DEViLBOX account and ratings client (plan 2026-10-25-m3-library.md I173, decision D-M3-12; design doc
 * section 9.2). Login stores the JWT in <config>/devilbox.token (mode 0600), never in the library DB.
 * PUT/DELETE /api/ratings push one rating (Bearer token), GET /api/ratings/batch reads up to 200 keys per
 * request (no token needed; with one the answer carries the user's own rating). Only modland: and hvsc: song
 * keys exist on DEViLBOX; local songs never go through here.
 *
 * Environment: SELECTA_DEVILBOX_URL replaces the base (sel_devilbox_base), SELECTA_NET_MAP / SELECTA_NET_ALLOW
 * steer the net guard like every other transport. */
#ifndef SELECTA_LIBRARY_NET_DEVILBOX_H
#define SELECTA_LIBRARY_NET_DEVILBOX_H

#include <stddef.h>

#define SEL_DEVILBOX_BATCH_MAX 200 /* keys per ratings batch request (server limit) */
#define SEL_DEVILBOX_TOKEN_MAX 1024

/* Directory that holds devilbox.token (the engine's config directory). Copied. */
void sel_devilbox_set_config_dir(const char* dir);

/* POST /api/auth/login and store the token. 1 = stored. 0 = refused or unreachable; *status (may be NULL) is the
 * HTTP status, 0 when none arrived. */
int sel_devilbox_login(const char* user, const char* password, int* status);

/* The stored token, trimmed, into out. 1 when a token file exists and is not empty. */
int sel_devilbox_token(char* out, size_t cap);

/* Splits a song key "<source>:<path>" for a source DEViLBOX rates (modland, hvsc). 1 and *path point into key;
 * 0 for local songs and every other source. */
int sel_devilbox_item(const char* key, char* source, size_t cap, const char** path);

/* Pushes one rating: 1..5 sends PUT, 0 sends DELETE. 1 = the server took it. 0 = try again later (no token,
 * unreachable, 401, 403, 5xx). -1 = the server refused the request for good (400). */
int sel_devilbox_rating_push(const char* key, int rating);

/* Reads the ratings of `n` paths of one source in batches of at most SEL_DEVILBOX_BATCH_MAX keys (fewer when the
 * URL would not fit). For every path the server holds the caller's own rating of, fn(ctx, index, rating) runs.
 * Returns the number of requests that answered 200; stops at the first failed batch. */
typedef void (*sel_devilbox_rating_fn)(void* ctx, unsigned index, int rating);
unsigned sel_devilbox_ratings_read(const char* source, const char* const* paths, unsigned n, sel_devilbox_rating_fn fn,
                                   void* ctx);

/* HippoPlayer vote service (I174): GET /api/hip/vote?k=<token>&m=<key>&r=<0|5> casts or withdraws a vote, r < 0 reads
 * GET /api/hip/votes?m=<key>&k=<token>. The answer is text/plain "<count> <yours>\n". token = 32 hex digits (the
 * device token), mkey = 12 hex digits (sel_uade_key). Needs no account. */
#define SEL_VOTE_OK 1       /* answered; *count and *yours are set */
#define SEL_VOTE_RETRY 0    /* unreachable, 5xx or an unparsable answer */
#define SEL_VOTE_BAD (-1)   /* "ERR bad" (400), or a token or key that is not hex */
#define SEL_VOTE_RATE (-2)  /* "ERR rate" (429): the service limits this token */
#define SEL_VOTE_MISSING (-3) /* 404: the server has no vote endpoint (devilbox_missing=hip_vote) */
int sel_devilbox_vote(const char* token, const char* mkey, int r, unsigned* count, unsigned* yours);

/* Counters since the process started. */
unsigned sel_devilbox_vote_posts(void); /* votes the service accepted (r >= 0) */
unsigned sel_devilbox_pushes(void);  /* rating pushes the server accepted */
unsigned sel_devilbox_batches(void); /* batch read requests sent */

#endif
