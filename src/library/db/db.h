/* Library database and worker (plan 2026-10-25-m3-library.md Phase 1; D-M3-1,
 * D-M3-2; design doc section 9.2). One worker owns the only SQLite
 * connection of a library; callers talk to it through a request and a result
 * SPSC queue. SQLite runs on a memsys5 heap allocated once at open and sized
 * from sel_machine (rule 4: no allocation after start). Every entry point
 * takes the library handle, which stands for the account (F52): no global
 * library. On the Amiga build requests run on the caller's task until the
 * library subtask lands (Phase 12). */
#ifndef SELECTA_LIBRARY_DB_H
#define SELECTA_LIBRARY_DB_H

#include <selecta/host.h>
#include <selecta/protocol.h>
#include <selecta/tags.h>

#include <stdint.h>

typedef struct sel_library sel_library;

/* Request codes (sel_message.id of a SEL_MSG_CMD on the request queue). */
enum {
    SEL_LIB_REQ_STATS = 1, /* counts plus an FTS5 self-check in a rolled-back savepoint */
    SEL_LIB_REQ_STOP = 2,
    SEL_LIB_REQ_SEARCH = 3,    /* run a query text; keys come back in sel_lib_job.out (I149) */
    SEL_LIB_REQ_SAVE_VIEW = 4, /* store text2 as the saved query `text` (I150) */
    SEL_LIB_REQ_IMPORT = 5,    /* import a TSV catalogue held in `text` (test and CLI seeding) */
    SEL_LIB_REQ_SCAN_BATCH = 6, /* payload: const sel_scan_batch*; writes files and songs rows (I158) */
    SEL_LIB_REQ_REMOVE = 7,     /* payload: const sel_remove_batch*; deletes files and songs rows (I161) */
    SEL_LIB_REQ_ANNOTATE = 8,   /* payload: const sel_annotate_batch*; lengths and notes onto existing songs (I162) */
    SEL_LIB_REQ_MOVE = 9,       /* payload: const sel_move*; re-keys one song row in place (I161b) */
    SEL_LIB_REQ_USER = 10,      /* payload: const sel_user_op*; history, liked, kept, per-song stickers (I170) */
    SEL_LIB_REQ_META = 11,      /* payload: sel_meta_op*; reads or writes one row of the meta table (I161a) */
    SEL_LIB_REQ_GENRE = 12      /* payload: sel_genre_op*; genre_cache and songs.genre (I181) */
};

/* One genre_cache operation (SEL_LIB_REQ_GENRE). The reply is INT [0] 1 = done and found/stored, 0 = not found
 * or failed, [1] SQLite result code, [2] row id (NEXT). */
enum {
    SEL_GENRE_NEXT = 1, /* next local song with id > `after`: `out` = song key NUL path NUL, reply [2] = its id (0: none left) */
    SEL_GENRE_GET,      /* cached entry of `key`: reply [0] = 1 when cached; `out` = genre, "" when cached as "no genre" */
    SEL_GENRE_PUT,      /* cache `key` -> `value` (NULL = no genre) with fetch time `at` */
    SEL_GENRE_APPLY,    /* songs.genre of song key `key` = `value`, unless the user edited the tag (sticker tag.genre) */
    SEL_SONGDB_GET,     /* I182: cached songdb answer of UADE key `key`: reply [0] = 1 when cached; `out` = authors, one per line, "" = not in songdb */
    SEL_SONGDB_PUT,     /* I182: cache UADE key `key` -> authors `value` (lines; NULL = not in songdb) with fetch time `at` */
    SEL_SONGDB_APPLY,   /* I182: rebuild song_composers of song key `key` from authors `value` (lines) */
    SEL_COMPOSER_NEXT,  /* I182: next song with id > `after` that has composer canon `key`: `out` = song key NUL name NUL display NUL, reply [2] = its id (0: none) */
    SEL_SIMILAR_NEXT,   /* I183: song at row offset `after` of the similar list of song key `key`: `out` = key NUL name NUL genre NUL rating NUL genre_match NUL (decimal text), reply [2] = after + 1 (0: none) */
    SEL_ALSOBY_NEXT,    /* I183: song at row offset `after` among the other songs sharing a composer with song key `key`: `out` = key NUL name NUL composer display NUL, reply [2] = after + 1 (0: none) */
    SEL_DUP_HASH,       /* I184: hashes the next local song with id > `after` that has no md5 and stores it: reply [2] = its id (0: none left) */
    SEL_DUP_NEXT,       /* I184: duplicate group at row offset `after` (same md5, two or more songs): `out` = md5 NUL member count NUL best key NUL, reply [2] = after + 1 (0: none) */
    SEL_BEST_SOURCE     /* I184: best song among those sharing the md5 of song key `key` (D-M3-14): `out` = key NUL, reply [2] = 1 (0: key unknown or no md5) */
};
typedef struct sel_genre_op {
    uint32_t op;       /* SEL_GENRE_* */
    const char* key;
    const char* value;
    int64_t at;
    int64_t after;
    char* out;
    uint32_t out_cap;
} sel_genre_op;

/* Counters of one sel_library_genre_sync run. */
typedef struct sel_genre_stats {
    uint32_t songs;        /* local songs looked at */
    uint32_t unreadable;   /* files that could not be read */
    uint32_t genre_lookups;    /* requests sent to the DEViLBOX genre endpoint */
    uint32_t genre_cache_hits; /* songs served from genre_cache without a request */
    uint32_t genre_set;    /* songs whose songs.genre was set */
    uint32_t unknown;      /* lookups that left the genre unknown (503, 404, transport error, untagged) */
    uint32_t http_requests;
    uint32_t songdb_lookups;    /* I182: requests sent to the DEViLBOX songdb endpoint */
    uint32_t songdb_cache_hits; /* I182: songs whose songdb answer came from songdb_cache */
    uint32_t composers_set;     /* I182: songs that got song_composers rows */
} sel_genre_stats;

/* One user-data operation (SEL_LIB_REQ_USER). The reply is INT [0] 0 = failed, 1 = done, 2 = done and a
 * STICKER_GET found its value, [1] SQLite result code. `at` is epoch seconds. */
enum {
    SEL_USER_HISTORY = 1, SEL_USER_LIKE, SEL_USER_KEEP, SEL_USER_STICKER_SET, SEL_USER_STICKER_GET,
    SEL_USER_FLAG,         /* I171: name "wrong" (value = note) or "skip" on the song key */
    SEL_USER_FLAG_SKIPPED, /* I171: reply [0] = 2 when the key carries the skip flag */
    SEL_USER_FLAG_EXPORT,  /* I171: name = path of the sounds-wrong.txt to write */
    SEL_USER_NOTE,         /* I172: value = note text on the song key (empty text removes it); queues a pending_push comment */
    SEL_USER_TAG_EDIT,     /* I172: name = artist|name|album|genre, value = edited tag (empty drops the edit) */
    SEL_USER_COUNTS,       /* I172: reply [2..5] = rows in history, flags, notes, pending_push */
    SEL_USER_STICKER_DEL,  /* I176: reply [0] = 2 when a row was deleted */
    SEL_USER_STICKER_LIST, /* I176: out = "name\0value\0" pairs in name order that fit; reply [2] = rows found */
    SEL_USER_CUE_SET,      /* I175: at = cue index, duration_ms = position in ms; name = label or NULL */
    SEL_USER_CUE_GET,      /* I175: at = cue index; reply [0] = 2 and [2] = position in ms when the cue exists */
    SEL_USER_FOLDER_ADD,   /* I178: name = folder name, at = parent folder id (0 = root); reply [2] = new id */
    SEL_USER_FOLDER_MOVE,  /* I178: at = folder id, to = new parent id (0 = root) */
    SEL_USER_FOLDER_DEL,   /* I178: at = folder id; refused while it holds folders or playlists */
    SEL_USER_PLAYLIST_MOVE,/* I178: at = playlist id, to = folder id (0 = root) */
    SEL_USER_PLAYLIST_ADD, /* I178: name = title, at = folder id, value = song keys joined by newline; reply [2] = new id */
    SEL_USER_FOLDER_COUNTS,/* I178: reply [2..5] = folders, playlists in folders, playlists, playlist entries */
    SEL_USER_FOLDER_PLAYLISTS, /* I178: at = folder id (0 = all); out = "id\0name\0" pairs then "\0"; reply [2] = playlists */
    SEL_USER_PLAYLIST_KEYS,/* I178: at = playlist id; out = song keys, each NUL terminated, then "\0"; reply [2] = keys */
    SEL_USER_FOLDER_TREE,  /* I178: out = indented tree text "F|P <id> <name>" per line; reply [2] = lines */
    SEL_USER_PANEL,        /* I185: out = "label\0value\0" rows of the module info panel, then "\0"; reply [2] = rows */
    SEL_USER_LOUDNESS_SET, /* I187: at = gain in centi-dB; value = path used when the song row does not exist yet */
    SEL_USER_LOUDNESS_GET, /* I187: reply [0] = 2 and [2] = gain in centi-dB (signed) when songs.loudness_gain is set */
    SEL_USER_RATING_SET,   /* I173: at = stars 1..5 (0 removes the rating), to = SEL_RATING_*; replaces the ratings row */
    SEL_USER_RATING_GET,   /* I173: reply [0] = 2 and [2] = stars when the song is rated */
    SEL_USER_RATING_PENDING, /* I173: out = "key\0stars\0" pairs of the ratings not yet on DEViLBOX, then "\0"; reply [2] = rows */
    SEL_USER_RATING_DONE   /* I173: the push of the key's rating is over: drops its pending row, marks the rating synced */
};
/* Where a rating came from (sel_user_op.to of SEL_USER_RATING_SET). */
enum { SEL_RATING_LOCAL = 0, SEL_RATING_QUEUE = 1, SEL_RATING_SERVER = 2 };
/* Why a folder op was refused: reply [0] = 0, [1] = SQLITE_CONSTRAINT, [2] = one of these. */
enum { SEL_FOLDER_OK = 0, SEL_FOLDER_ERR_NO_FOLDER = 1, SEL_FOLDER_ERR_CYCLE = 2, SEL_FOLDER_ERR_NOT_EMPTY = 3,
       SEL_FOLDER_ERR_NO_PLAYLIST = 4, SEL_FOLDER_ERR_FAILED = 5 };
typedef struct sel_user_op {
    uint32_t op;       /* SEL_USER_* */
    const char* key;   /* song key */
    const char* name;  /* STICKER_*: sticker name */
    const char* value; /* STICKER_SET: value */
    int64_t at;
    int64_t to;          /* FOLDER_MOVE / PLAYLIST_MOVE: target folder id */
    int32_t skipped;     /* HISTORY: 1 = ended by the user before the natural end */
    int32_t duration_ms; /* HISTORY: 0 = unknown */
    char* out;           /* STICKER_GET: value, "" when absent */
    uint32_t out_cap;
} sel_user_op;

/* One meta key/value (text). set != 0 stores `value`; set == 0 reads into `value` (capacity `cap`, "" when the
 * key is unset). Reply: INT [0] 1 when the key exists (read) or was stored (write), [1] SQLite code. */
typedef struct sel_meta_op {
    const char* key;
    char* value;
    uint32_t cap;
    int set;
} sel_meta_op;

/* The data of a SEARCH, SAVE_VIEW or IMPORT request. The caller fills the inputs, passes the job to
 * sel_library_run and blocks until the worker answers, so the pointers stay valid. No allocation:
 * `out` is a caller buffer of NUL-terminated keys. */
typedef struct sel_lib_job {
    const char* text;  /* SEARCH: query text; SAVE_VIEW: view name; IMPORT: catalogue TSV */
    const char* text2; /* SAVE_VIEW: the query text */
    long long now;     /* SEARCH: seconds since the epoch for relative times */
    char* out;         /* SEARCH: keys, each NUL terminated */
    uint32_t out_cap;
    char* meta;         /* SEARCH, optional: per key in `out`, four NUL terminated fields: title, artist, format, duration "m:ss" ("" unknown) */
    uint32_t meta_cap;
    uint32_t meta_used; /* out: bytes of `meta` written (whole rows) */
    uint32_t count;     /* out: keys stored (SEARCH), rows imported (IMPORT) */
    uint32_t truncated; /* out: keys that did not fit */
    uint32_t used_fts;  /* out: 1 when the FTS5 index served the query */
    char error[24];     /* out: "" or a query error code (unknown_key, unknown_view, ...) */
} sel_lib_job;

/* One catalogue row from a source (design doc 9.2 `files` plus the `songs` columns a scan knows).
 * Text is NUL-terminated, "" = unknown. */
typedef struct sel_scan_row {
    const char* source; /* "local" */
    const char* path;
    const char* format;
    const char* artist;
    const char* name;
    const char* md5;
    int64_t size;
    int32_t duration_ms; /* 0 = unknown */
    int32_t year;        /* 0 = unknown */
} sel_scan_row;

typedef struct sel_scan_batch {
    const sel_scan_row* rows;
    uint32_t count;
    int64_t now; /* epoch seconds stored in songs.added */
} sel_scan_batch;

/* Rows to delete again (a changelog's deleted: and moved: entries): `count` paths of one source. */
typedef struct sel_remove_batch {
    const char* source;
    const char* const* paths;
    uint32_t count;
} sel_remove_batch;

/* One moved file (a changelog's moved:/to: pair): the row at `from` is re-keyed to `to->path` in place, so
 * md5, size, duration, year, added, stickers, likes, ratings, history and playlist entries stay. Only
 * name, artist and format are taken from `to` (the new path may parse differently). A row already at the
 * destination is replaced. Reply: INT [0] 1 when a row moved, 0 when `from` was unknown; [1] SQLite code. */
typedef struct sel_move {
    const char* source;
    const char* from;
    const sel_scan_row* to;
} sel_move;

/* Side data for songs that already have a row (HVSC Songlengths.md5 and STIL, I162). `duration_ms` > 0
 * and a non-empty `md5` replace the song's values; every sticker is stored under the song key in the
 * stickers table. A path without a songs row is skipped (not an error, not counted). */
typedef struct sel_sticker {
    const char* name;
    const char* value;
} sel_sticker;
typedef struct sel_annotation {
    const char* path;
    const char* md5;      /* "" or NULL = keep */
    int32_t duration_ms;  /* 0 = keep */
    const sel_sticker* stickers;
    uint32_t sticker_n;
} sel_annotation;
typedef struct sel_annotate_batch {
    const char* source;
    const sel_annotation* items;
    uint32_t count;
} sel_annotate_batch;
/* Reply to SEL_LIB_REQ_ANNOTATE: INT [0] songs annotated, [1] SQLite result code (0 = ok). */

/* Reply to SEL_LIB_REQ_REMOVE: INT [0] song rows deleted, [1] SQLite result code (0 = ok). */

/* Reply to SEL_LIB_REQ_SCAN_BATCH: INT [0] rows written, [1] SQLite result code (0 = ok). */

/* Reply to SEL_LIB_REQ_STATS (sel_message.id = the request code, kind SEL_MSG_EVT): args are INT
 * [0] schema_version, [1] fts5 compiled in, [2] fts_hits of the self-check, [3] songs rows,
 * [4] requests served so far, [5] SQLite result code of the last statement (0 = ok). */
enum {
    SEL_LIB_STAT_SCHEMA = 0,
    SEL_LIB_STAT_FTS5,
    SEL_LIB_STAT_FTS_HITS,
    SEL_LIB_STAT_SONGS,
    SEL_LIB_STAT_SERVED,
    SEL_LIB_STAT_RC
};

/* Schema version this build writes (PRAGMA user_version after migration). */
#define SEL_LIB_SCHEMA_VERSION 2

/* Heap bytes for SQLite on a machine: the scaling table of D-M3-2 over fast + chip RAM
 * (<= 4 MiB: 512 KiB, <= 16 MiB: 2 MiB, <= 128 MiB: 8 MiB, above: 32 MiB). */
uint32_t sel_library_heap_bytes(const sel_tag* machine);

/* Opens a library for `account` at `path` (":memory:" for a throwaway DB), allocates the heap
 * once, starts the worker, runs the schema and migrations. NULL on failure. */
sel_library* sel_library_open(const sel_host* host, const sel_tag* machine, uint32_t account, const char* path);
void sel_library_close(sel_library* lib);

/* Sends a request and waits for its reply. 1 on success. */
int sel_library_call(sel_library* lib, uint32_t req, sel_message* reply);
/* Same with a payload the worker reads while the caller waits (carried in args[0].v.s). */
int sel_library_request(sel_library* lib, uint32_t req, const void* payload, sel_message* reply);

/* Runs a request that carries a job (SEARCH, SAVE_VIEW, IMPORT). 1 on success; query errors are in job->error. */
int sel_library_run(sel_library* lib, uint32_t req, sel_lib_job* job);

/* LIBRARY_FETCH (I165, I166, I166a): `spec` is "<source>:<path>" with an optional "#md5=<hex>". The
 * song and the companions `rules` name (sel_companion_rules) land in the handle's file cache. 1 with
 * the cache path in `out` and the files delivered in `files`; 0 on failure; -1 when every mirror
 * delivered a body whose md5 differs. */
int sel_library_fetch(sel_library* lib, const char* spec, const char* rules, char* out, size_t cap, uint32_t* files);
/* Cache counters; 0 when the handle served no fetch. */
struct sel_cache_stats;
/* Trim the file cache to `budget` bytes (0 = the machine's default, sel_cache_budget) by last access.
 * Songs in the `kept` and `liked` tables are exempt. Runs after every LIBRARY_FETCH that stored a
 * file. Returns the trim_runs counter after the call; 0 when the handle has no cache dir. */
struct sel_cache_trim_result;
unsigned sel_library_cache_trim(sel_library* lib, uint32_t budget, struct sel_cache_trim_result* out);
/* User data (I170): 1 on success. A finished play or a skip goes to `history`; like and keep to their tables. */
int sel_library_history_add(sel_library* lib, const char* key, int64_t at, int skipped);
int sel_library_like(sel_library* lib, const char* key, int64_t at);
int sel_library_keep(sel_library* lib, const char* key, int64_t at);
/* Per-song replayer options (D-M2-8) live in the sticker "rp.config" of the song key. */
int sel_library_song_config_set(sel_library* lib, const char* key, const char* list);
/* Copies the list into `out` ("" when the song has none); 1 on success. */
int sel_library_song_config_get(sel_library* lib, const char* key, char* out, uint32_t cap);
/* FLAG (I171): kind "wrong" with a note, or "skip". 1 on success; *rows = rows now in `flags` (may be NULL). */
int sel_library_flag(sel_library* lib, const char* key, const char* kind, const char* note, uint32_t* rows);
/* 1 when the song key carries the skip flag. */
int sel_library_flag_skipped(sel_library* lib, const char* key);
/* Writes the wrong-flagged songs to `path` as "key<TAB>note" lines in key order (sounds-wrong.txt). 1 on
 * success; *rows = lines written (may be NULL). */
int sel_library_flags_export(sel_library* lib, const char* path, uint32_t* rows);
/* NOTE (I172): stores the note for the song key and queues a "comment" row in pending_push for the optional
 * DEViLBOX push (nothing is sent by the library; no endpoint is defined in the repo). Empty note deletes it. */
int sel_library_note(sel_library* lib, const char* key, const char* note);
/* Tag edit (I172): `field` is artist, name, album or genre. The edit is kept in the sticker "tag.<field>" and
 * written to the songs row, so queries, FTS and display read it from one place; a rescan or catalogue import
 * keeps it (SEL_SONG_TAG_KEEP). An empty value drops the edit (the scanned value returns on the next scan). */
int sel_library_tag_edit(sel_library* lib, const char* key, const char* field, const char* value);
/* Stickers (I176, D-M3-17): per-key name/value rows. get: 1 when the sticker exists (value copied to `out`), 0
 * when absent or failed. del: 1 when a row was deleted. list: fills `out` with "name\0value\0" pairs in name
 * order (as many as fit) and *rows with the total; 1 on success. */
#define SEL_STICKER_VALUE_MAX 256u /* longest sticker value kept, with its NUL */
int sel_library_sticker_set(sel_library* lib, const char* key, const char* name, const char* value);
int sel_library_sticker_get(sel_library* lib, const char* key, const char* name, char* out, uint32_t cap);
int sel_library_sticker_del(sel_library* lib, const char* key, const char* name);
int sel_library_sticker_list(sel_library* lib, const char* key, char* out, uint32_t cap, uint32_t* rows);
/* I185: the catalogue rows of the module info panel for song `key` (STIL note, composer, release, year, genre,
 * rating, play count): "label\0value\0" pairs in display order, then an empty label. Data the DB lacks adds no row.
 * 1 on success; *rows = rows written (may be NULL). */
int sel_library_song_panel(sel_library* lib, const char* key, char* out, uint32_t cap, uint32_t* rows);
/* Cue points (I175): cue `idx` of a song key at `pos_ms`. get: 1 and *pos_ms when the cue exists. */
int sel_library_cue_set(sel_library* lib, const char* key, uint32_t idx, uint32_t pos_ms);
int sel_library_cue_get(sel_library* lib, const char* key, uint32_t idx, uint32_t* pos_ms);
/* I187: per-song loudness gain (centi-dB) in songs.loudness_gain; set creates the song row when missing. */
int sel_library_loudness_set(sel_library* lib, const char* key, const char* path, int32_t gain_cdb);
int sel_library_loudness_get(sel_library* lib, const char* key, int32_t* gain_cdb);
/* Ratings (I173): stars 1..5, 0 removes. origin is SEL_RATING_LOCAL / QUEUE / SERVER. _pending fills out with
 * "key\0stars\0" pairs then "\0" (cap bytes) and *rows with the number of pending ratings; _done drops the key's
 * pending push. */
int sel_library_rating_set(sel_library* lib, const char* key, int stars, int origin);
int sel_library_rating_get(sel_library* lib, const char* key, int* stars);
int sel_library_rating_pending(sel_library* lib, char* out, uint32_t cap, uint32_t* rows);
int sel_library_rating_done(sel_library* lib, const char* key);
/* Row counts of the user tables (any pointer may be NULL). 1 on success. */
/* I178 folder tree. Each returns SEL_FOLDER_OK or a SEL_FOLDER_ERR_* code; *out_id (when not NULL) = new id. */
int sel_library_folder_add(sel_library* lib, const char* name, uint32_t parent, uint32_t* out_id);
int sel_library_folder_move(sel_library* lib, uint32_t id, uint32_t parent);
int sel_library_folder_del(sel_library* lib, uint32_t id);
int sel_library_playlist_move(sel_library* lib, uint32_t id, uint32_t folder);
int sel_library_playlist_add(sel_library* lib, const char* name, uint32_t folder, const char* keys_nl, uint32_t* out_id);
int sel_library_folder_counts(sel_library* lib, uint32_t* folders, uint32_t* in_folders, uint32_t* playlists, uint32_t* entries);
/* Fills `out` per the SEL_USER_FOLDER_PLAYLISTS / PLAYLIST_KEYS / FOLDER_TREE formats; *count = rows found. */
int sel_library_folder_playlists(sel_library* lib, uint32_t folder, char* out, uint32_t cap, uint32_t* count);
int sel_library_playlist_keys(sel_library* lib, uint32_t playlist, char* out, uint32_t cap, uint32_t* count);
int sel_library_folder_tree(sel_library* lib, char* out, uint32_t cap, uint32_t* count);
int sel_library_user_counts(sel_library* lib, uint32_t* history, uint32_t* flags, uint32_t* notes, uint32_t* pending_push);
/* Marks song key `key` in table "kept" or "liked" (seeding and tests). 1 on success. */
int sel_library_mark(sel_library* lib, const char* table, const char* key);
/* Companion rules (sel_companion_rules) the trim honours; every LIBRARY_FETCH sets them. */
void sel_library_set_rules(sel_library* lib, const char* rules);
/* Overrides the machine-scaled cache budget of this handle (tests). */
void sel_library_set_budget(sel_library* lib, uint32_t budget);
/* Prefetch (I168): fetches `spec` ("<source>:<path>") and its companions into the cache on a low-priority
 * thread, one request at a time; a new request replaces a pending one. 0 when it cannot run (no thread,
 * no cache). Cancel drops a pending request; wait blocks until the thread is idle (tests). */
int sel_library_prefetch(sel_library* lib, const char* spec, const char* rules);
void sel_library_prefetch_cancel(sel_library* lib);
/* Smart downloads (I169, F34): plans up to `top` (capped at 8) remote songs not yet cached, first the songs of the
 * saved view `view` (the smart playlist; "" or NULL = none), then the most played songs of the history, and queues
 * them behind the prefetch on the same low-priority thread. A foreground fetch drops them, a prefetch request goes
 * first, and none starts once the cache holds its budget. Returns the songs queued. */
unsigned sel_library_smart(sel_library* lib, const char* view, unsigned top, const char* rules);
void sel_library_prefetch_wait(sel_library* lib);
/* Builds without threads (Amiga): runs the pending prefetch request, one blocking transfer per call; the engine
 * calls it from its tick right after playback started. Threaded builds: no-op, returns 0. Returns 1 when a
 * request was run. */
int sel_library_prefetch_poll(sel_library* lib);
void sel_library_prefetch_stop(sel_library* lib);

/* I173c: sends the pending DEViLBOX rating pushes on a background thread, so an unreachable server never stalls the
 * caller (the engine command thread). The rating is already stored (sel_library_rating_set). Without threads the push
 * runs inline. _wait returns once the thread is idle (test hook, T79); _stop is called by sel_library_close. */
void sel_library_rating_push(sel_library* lib);
void sel_library_rating_wait(sel_library* lib);
int sel_library_cache_stats(const sel_library* lib, struct sel_cache_stats* out);
/* Prints `cache_hits= fetches= companions= md5_mismatches= mirror_tries=` when a fetch was served. */
void sel_library_cache_report(const sel_library* lib);

/* Genre (I181, T85): for every local song reads the file, builds the UADE key (xxh32.h), takes the genre from
 * genre_cache or, on a miss, from GET <SELECTA_DEVILBOX_URL>/api/genre/<key>?name=&size= through the one HTTP
 * client, caches the answer and sets songs.genre. Only source "modarchive" with a genre counts as known; untagged,
 * miss and ambiguous answers are cached as "no genre". A 503, other status or transport failure caches nothing
 * and is not an error. 1 on success. */
int sel_library_genre_sync(sel_library* lib, sel_genre_stats* stats);

/* Composer page (I182, T85): the songs of `name` under every spelling and alias songdb gave them (composer.h
 * folds both the stored authors and `name`). Calls `fn(ctx, song_key, song_name, composer_display)` per song in
 * library order; returns the row count, or -1 on a library error. Data comes from sel_library_genre_sync's songdb
 * step (GET /api/songdb/lookup?hash=<uade key>, cached in songdb_cache). */
typedef void (*sel_composer_row_fn)(void* ctx, const char* song_key, const char* song_name, const char* composer);
int sel_library_composer_page(sel_library* lib, const char* name, sel_composer_row_fn fn, void* ctx);

/* Also-by (I183, T86): the other songs that share a composer with song `song_key` (song_composers, canonical
 * spelling), in library order. Same row callback as the composer page (`composer` = the shared display name).
 * Returns the row count, or -1 on a library error. */
int sel_library_also_by(sel_library* lib, const char* song_key, sel_composer_row_fn fn, void* ctx);

/* Similar (I183, T86): every other song ordered by genre match with `song_key` (1 = same genre, case-insensitive;
 * empty genre never matches), then rating (unrated = 0, higher first), then library order; at most 100 rows.
 * Returns the row count, or -1 on a library error. */
typedef void (*sel_similar_row_fn)(void* ctx, const char* song_key, const char* song_name, const char* genre,
                                   int rating, int genre_match);
int sel_library_similar(sel_library* lib, const char* song_key, sel_similar_row_fn fn, void* ctx);

/* Radio (I186, T88; radio.c): the next song of a radio seeded from `seed` (a library key or the queue path of a
 * local song). Candidates are the seed's genre matches, its songs rated 4 or more and its composer's other songs
 * (the two queries above), never a skip-flagged song; the weighted draw uses `draw` (a caller-held xorshift
 * step), so one seed gives one sequence. `avoid` (the previous pick, may be NULL) is left out unless nothing else
 * is a candidate. Writes the queue form of the pick (plain path for a local song, the key otherwise) to `out`.
 * Returns 1 with a pick, 0 when there is none. */
int sel_library_radio_next(sel_library* lib, const char* seed, uint32_t draw, const char* avoid, char* out, size_t cap);

/* Duplicates (I184, T86): groups of songs with the same md5 across sources. Hashes the local songs that have no
 * md5 yet, then calls `fn` once per group with the md5, the member count and the best key (D-M3-14: local >
 * home mirror > cache > modland > hvsc > hippo URL, ties by library order). Returns the group count, -1 on a
 * library error. */
typedef void (*sel_dup_row_fn)(void* ctx, const char* md5, int members, const char* best_key);
int sel_library_duplicates(sel_library* lib, sel_dup_row_fn fn, void* ctx);
/* Hashes the local songs without an md5 (what the duplicates step needs). Returns how many files were looked
 * at, -1 on a library error. */
int sel_library_hash_local(sel_library* lib);
/* The song OPEN plays for `song_key`: the best source among the songs with the same md5 (D-M3-14), which is
 * `song_key` itself when it is the best. A local best source comes back as its plain path, a remote one as its key
 * (the form read_song opens). 0 when the song has no md5 or is unknown. */
int sel_library_best_source(sel_library* lib, const char* song_key, char* out, size_t cap);

uint32_t sel_library_account(const sel_library* lib);
uint32_t sel_library_heap_size(const sel_library* lib);
/* Heap allocations made so far by this library (1 after a successful open). */
uint32_t sel_library_heap_allocs(const sel_library* lib);
const char* sel_library_sqlite_version(void);

#endif
