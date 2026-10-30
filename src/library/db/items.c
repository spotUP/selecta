/* The Your Library item list: pinned rows, playlists, albums, artists, composers and sources in one query (design doc
 * 2026-10-29-desktop-library-architecture.md section 2, I678a). The SQL is assembled from one part per kind in the caller's mask,
 * so a chip that wants only albums does not pay for the artist scan. Rows stream through op->row like SEL_COMPOSER_LIST: only the
 * window [op->offset, op->offset + op->limit) is emitted while every matching row is counted, so a 30 000 album library never
 * lands in one array and the caller still learns the total (reply [2]).
 *
 * Reused, not reinvented: the maintained tables composers (v7) and source_songs (v8) for their counts, the genre op streaming path
 * (sel_genre_call buffers the rows on the worker and replays them on the caller's thread), history.key and playlist_entries keys. */
#include "db_internal.h"
#include <stdlib.h>
#include <unistd.h>

#include "../composer.h"
#include "../source.h"

#include <selecta/platform.h>
#include <selecta/library_item.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Columns of every part: 1 kind, 2 key, 3 name, 4 sub, 5 songs, 6 added, 7 last_played, 8 art_id, 9 pinned, 10 sort name, 11 tie, 12 opened (opened table, I678c). */
#define PART_LIKED \
    "SELECT 0,'liked','Liked Songs','',(SELECT COUNT(*) FROM liked),0,IFNULL((SELECT MAX(at) FROM liked),0),0,1,'',0,0"
#define PART_RECENT \
    "SELECT 1,'recent','Recently Played','',(SELECT COUNT(DISTINCT key) FROM history),0," \
    "IFNULL((SELECT MAX(played_at) FROM history),0),0,1,'',0,0"
#define PART_LOCAL \
    "SELECT 2,'local','Local Files','',IFNULL((SELECT n FROM source_songs WHERE source='local'),0),0,0,0,1,'',0,0"
/* Playlists in creation order (ids grow); owner "You": every saved playlist is local. added = id, as the sidebar used. */
#define PART_PLAYLIST \
    "SELECT 3,CAST(p.id AS TEXT),p.name,'You',(SELECT COUNT(*) FROM playlist_entries e WHERE e.playlist_id=p.id),p.id," \
    "IFNULL((SELECT MAX(h.played_at) FROM playlist_entries e JOIN history h ON h.key=e.key WHERE e.playlist_id=p.id),0),0,0," \
    "'',p.id," \
    "IFNULL((SELECT at FROM opened WHERE kind=3 AND id=CAST(p.id AS TEXT)),0) FROM playlists p"
#define PART_ALBUM \
    "SELECT 4,CAST(a.id AS TEXT),a.name,IFNULL(a.artist,''),IFNULL(sa.n,0),IFNULL(sa.ad,0),IFNULL(la.t,0),a.id,0," \
    "a.name,a.id," \
    "IFNULL((SELECT at FROM opened WHERE kind=4 AND id=CAST(a.id AS TEXT)),0) FROM albums a LEFT JOIN sa ON sa.al IS a.name AND sa.ar IS a.artist" \
    " LEFT JOIN la ON la.al IS a.name AND la.ar IS a.artist"
#define PART_ARTIST \
    "SELECT 5,r.name,r.name,'',r.n,r.ad,IFNULL(lr.t,0),0,0,r.name,0," \
    "IFNULL((SELECT at FROM opened WHERE kind=5 AND id=r.name),0) FROM" \
    " (SELECT ar AS name,n,ad FROM sr UNION ALL SELECT artist,0,0 FROM albums WHERE artist<>'' AND artist NOT IN (SELECT ar FROM sr)) r" \
    " LEFT JOIN lr ON lr.ar=r.name"
/* Composers: the maintained table plus the Unknown composer group, like SEL_COMPOSER_LIST. */
#define PART_COMPOSER \
    "SELECT 6,c.canon,c.display,'',c.songs,0,IFNULL(lc.t,0),0,0,c.display,0," \
    "IFNULL((SELECT at FROM opened WHERE kind=6 AND id=c.canon),0) FROM composers c LEFT JOIN lc ON lc.canon=c.canon" \
    " UNION ALL SELECT 6,'" SEL_COMPOSER_UNKNOWN_CANON "','" SEL_COMPOSER_UNKNOWN_DISPLAY "','',n,0,0,0,0,'~',1," \
    "IFNULL((SELECT at FROM opened WHERE kind=6 AND id='" SEL_COMPOSER_UNKNOWN_CANON "'),0)" \
    " FROM composer_unknown WHERE n>0"
/* Sources: every registered source (the reg helper table), with 0 songs too, so a source that never synced can be opened and
 * synced (I678m), plus a source that still owns songs after its plugin went. The Local Files pinned row stands for 'local'. */
#define PART_SOURCE \
    "SELECT 7,r.id,r.id,'',IFNULL(s.n,0),0,IFNULL(ls.t,0),0,0,r.id,0," \
    "IFNULL((SELECT at FROM opened WHERE kind=7 AND id=r.id),0) FROM" \
    " (SELECT id FROM reg UNION SELECT source FROM source_songs WHERE n>0) r" \
    " LEFT JOIN source_songs s ON s.source=r.id LEFT JOIN ls ON ls.source=r.id WHERE r.id<>'local'"

static const char* const k_part[SEL_LIB_ITEM_KINDS] = {PART_LIKED, PART_RECENT, PART_LOCAL,    PART_PLAYLIST,
                                                       PART_ALBUM, PART_ARTIST, PART_COMPOSER, PART_SOURCE};

/* Helper tables the parts join (a CTE nobody reads costs nothing). I787: the history joins are CROSS JOINs, which fixes the loop
 * order: history (a few thousand rows) drives and songs is probed by its unique key. Left to the planner it walked the whole songs
 * table through songs_artist_added and probed history per song (10 s on 594 520 songs). The sa and sr aggregates read the covering
 * indexes songs_album and songs_artist_added (schema v13): one index scan, no table lookups. */
#define CTE_SA "sa AS (SELECT album AS al, artist AS ar, COUNT(*) AS n, MAX(added) AS ad FROM songs GROUP BY album, artist)"
#define CTE_LA \
    "la AS (SELECT s.album AS al, s.artist AS ar, MAX(h.played_at) AS t FROM history h CROSS JOIN songs s ON s.key=h.key" \
    " GROUP BY s.album, s.artist)"
#define CTE_SR "sr AS (SELECT artist AS ar, COUNT(*) AS n, MAX(added) AS ad FROM songs WHERE artist<>'' GROUP BY artist)"
#define CTE_LR \
    "lr AS (SELECT s.artist AS ar, MAX(h.played_at) AS t FROM history h CROSS JOIN songs s ON s.key=h.key WHERE s.artist<>''" \
    " GROUP BY s.artist)"
#define CTE_LC \
    "lc AS (SELECT c.canon AS canon, MAX(h.played_at) AS t FROM history h CROSS JOIN song_composers c ON c.song_key=h.key GROUP BY c.canon)"
#define CTE_LS \
    "ls AS (SELECT s.source AS source, MAX(h.played_at) AS t FROM history h CROSS JOIN songs s ON s.key=h.key GROUP BY s.source)"

/* reg (bit 6) is not a constant: put_reg writes the registered source ids. */
#define CTE_COUNT 7u
#define CTE_REG 6u

static size_t put(char* sql, size_t cap, size_t n, const char* text) {
    size_t len = strlen(text);

    if (n + len + 1u > cap) return cap; /* overflow marks the statement unusable */
    memcpy(sql + n, text, len + 1u);
    return n + len;
}

/* The reg helper table: one row per registered source id. Reads the registry only; the caller half registered it. */
static size_t put_reg(char* sql, size_t cap, size_t n) {
    uint32_t i, count = sel_source_count();

    n = put(sql, cap, n, "reg(id) AS (SELECT 'local'");
    for (i = 0; i < count && n < cap; i++) {
        const sel_source* src = sel_source_get(i);

        if (src == NULL) continue;
        n = put(sql, cap, n, " UNION ALL SELECT ");
        if (n < cap) {
            sqlite3_snprintf((int)(cap - n), sql + n, "%Q", src->id);
            n += strlen(sql + n);
            if (n + 1u >= cap) n = cap;
        }
    }
    return put(sql, cap, n, ")");
}

/* Builds the statement for `mask`; 0 when it does not fit or the mask names no kind. */
static int build_sql(uint32_t mask, char* sql, size_t cap) {
    static const char* const cte[CTE_COUNT] = {CTE_SA, CTE_LA, CTE_SR, CTE_LR, CTE_LC, CTE_LS, NULL};
    /* bit i = cte[i]: albums read sa+la, artists sr+lr, composers lc, sources ls+reg. */
    static const uint32_t need[SEL_LIB_ITEM_KINDS] = {0, 0, 0, 0, 0x03u, 0x0Cu, 0x10u, 0x60u};
    uint32_t k, with = 0, parts = 0, i;
    size_t n = 0;
    int first = 1;

    mask &= SEL_LIB_ITEM_ALL;
    if (mask == 0u) return 0;
    for (k = 0; k < SEL_LIB_ITEM_KINDS; k++) {
        if (mask & SEL_LIB_ITEM_BIT(k)) with |= need[k];
    }
    sql[0] = '\0';
    if (with != 0u) {
        n = put(sql, cap, n, "WITH ");
        for (i = 0; i < CTE_COUNT; i++) {
            if (!(with & (1u << i))) continue;
            if (!first) n = put(sql, cap, n, ",");
            first = 0;
            n = i == CTE_REG ? put_reg(sql, cap, n) : put(sql, cap, n, cte[i]);
        }
        n = put(sql, cap, n, " ");
    }
    n = put(sql, cap, n, "SELECT * FROM (");
    for (k = 0; k < SEL_LIB_ITEM_KINDS; k++) {
        if (!(mask & SEL_LIB_ITEM_BIT(k))) continue;
        if (parts++ > 0u) n = put(sql, cap, n, " UNION ALL ");
        n = put(sql, cap, n, k_part[k]);
    }
    n = put(sql, cap, n, ") ORDER BY 1, 10 COLLATE NOCASE, 11");
    return n < cap;
}

static void num(char* buf, size_t cap, sqlite3_stmt* st, int col) { snprintf(buf, cap, "%lld", (long long)sqlite3_column_int64(st, col)); }

static const char* text(sqlite3_stmt* st, int col) {
    const char* t = (const char*)sqlite3_column_text(st, col);

    return t != NULL ? t : "";
}

/* I691c: the song paths behind one list item, one statement per kind, ?1 the item key. */
static const char* format_sql(uint32_t kind) {
    switch (kind) {
    case SEL_LIB_ITEM_ALBUM:
        return "SELECT s.path FROM songs s JOIN albums a ON a.id=CAST(?1 AS INTEGER) AND s.album IS a.name AND s.artist IS a.artist";
    case SEL_LIB_ITEM_ARTIST: return "SELECT path FROM songs WHERE artist=?1";
    case SEL_LIB_ITEM_COMPOSER: /* I787: two branches; a CASE in one WHERE made every composer scan the whole songs table (600 000 rows each) */
        return "SELECT s.path FROM songs s WHERE ?1='" SEL_COMPOSER_UNKNOWN_CANON "' AND NOT EXISTS"
               " (SELECT 1 FROM song_composers c WHERE c.song_key=s.key)"
               " UNION ALL SELECT s.path FROM song_composers c JOIN songs s ON s.key=c.song_key WHERE c.canon=?1 AND ?1<>'" SEL_COMPOSER_UNKNOWN_CANON "'";
    case SEL_LIB_ITEM_PLAYLIST:
        return "SELECT s.path FROM playlist_entries e JOIN songs s ON s.key=e.key WHERE e.playlist_id=CAST(?1 AS INTEGER)";
    case SEL_LIB_ITEM_LIKED: return "SELECT s.path FROM songs s JOIN liked l ON l.key=s.key";
    case SEL_LIB_ITEM_RECENT: return "SELECT s.path FROM songs s WHERE s.key IN (SELECT key FROM history)";
    case SEL_LIB_ITEM_LOCAL: return "SELECT path FROM songs WHERE source='local'";
    case SEL_LIB_ITEM_SOURCE: return "SELECT path FROM songs WHERE source=?1";
    default: return NULL;
    }
}

void sel_db_majority_format(sqlite3* db, uint32_t kind, const char* key, char* out, size_t cap) {
    const char* q = format_sql(kind);
    sqlite3_stmt* st = NULL;
    sel_platform_family_tally t;

    out[0] = '\0';
    if (q == NULL || sqlite3_prepare_v2(db, q, -1, &st, NULL) != SQLITE_OK) return;
    memset(&t, 0, sizeof t);
    sqlite3_bind_text(st, 1, key != NULL ? key : "", -1, SQLITE_TRANSIENT); /* a statement without ?1 ignores the range error */
    while (sqlite3_step(st) == SQLITE_ROW) {
        const char* path = (const char*)sqlite3_column_text(st, 0);

        if (path != NULL) sel_platform_family_tally_add(&t, path);
    }
    sqlite3_finalize(st);
    sel_platform_family_tally_format(&t, out, cap);
}

void sel_db_majority_format_cached(sel_library* lib, uint32_t kind, const char* key, char* out, size_t cap) {
    const char* k = key != NULL ? key : "";
    int64_t stamp = (int64_t)sqlite3_total_changes64(lib->db) + 1;
    uint32_t h = kind * 2654435761u;
    sel_fmt_cache* c;
    size_t i;

    if (strlen(k) >= SEL_FMT_CACHE_KEY || cap < sizeof c->fmt) {
        sel_db_majority_format(lib->db, kind, key, out, cap);
        return;
    }
    for (i = 0; k[i] != '\0'; i++) h = h * 31u + (unsigned char)k[i];
    c = &lib->fmt_cache[h % SEL_FMT_CACHE_SLOTS];
    if (c->stamp != stamp || c->kind != kind || strcmp(c->key, k) != 0) {
        sel_db_majority_format(lib->db, kind, k, c->fmt, sizeof c->fmt);
        c->stamp = stamp;
        c->kind = kind;
        strcpy(c->key, k);
    }
    strcpy(out, c->fmt);
}

int sel_library_items_op(sel_library* lib, const sel_genre_op* op, int* found, int64_t* id) {
    char sql[4096];
    sqlite3_stmt* st = NULL;
    int64_t total = 0;
    uint32_t emitted = 0;
    int rc;

    *found = 0;
    *id = 0;
    if (op->row == NULL || !build_sql(op->mask, sql, sizeof sql)) return SQLITE_MISUSE;
#if !defined(__amigaos__) && !defined(AMIGA)
    {   /* I787 test knob: a slow query for the dui test that proves the frame does not wait on it */
        const char* slow = getenv("SELECTA_LIB_ITEMS_SLOW_MS");

        if (slow != NULL && atoi(slow) > 0) usleep((useconds_t)atoi(slow) * 1000u);
    }
#endif
    rc = sqlite3_prepare_v2(lib->db, sql, -1, &st, NULL);
    if (rc != SQLITE_OK) return rc;
    while ((rc = sqlite3_step(st)) == SQLITE_ROW) {
        if (total >= (int64_t)op->offset && emitted < op->limit) {
            char kind[24], songs[24], added[24], last[24], art[24], pinned[24], opened[24];
            char fmt[16];
            const char* f[11];

            num(kind, sizeof kind, st, 0);
            num(songs, sizeof songs, st, 4);
            num(added, sizeof added, st, 5);
            num(last, sizeof last, st, 6);
            num(art, sizeof art, st, 7);
            num(pinned, sizeof pinned, st, 8);
            num(opened, sizeof opened, st, 11);
            f[0] = kind;
            f[1] = text(st, 1);
            f[2] = text(st, 2);
            f[3] = text(st, 3);
            f[4] = songs;
            f[5] = added;
            f[6] = last;
            f[7] = art;
            f[8] = pinned;
            f[9] = opened;
            sel_db_majority_format_cached(lib, (uint32_t)sqlite3_column_int(st, 0), text(st, 1), fmt, sizeof fmt);
            f[10] = fmt;
            emitted++;
            (void)op->row(op->row_ctx, f);
        }
        total++;
    }
    sqlite3_finalize(st);
    if (rc != SQLITE_DONE) return rc;
    *id = total;
    *found = 1;
    return SQLITE_OK;
}

/* ---- caller half ---- */

typedef struct items_sink {
    sel_library_item* out;
    uint32_t n, max;
} items_sink;

static void copy_text(char* dst, size_t cap, const char* src) { snprintf(dst, cap, "%s", src != NULL ? src : ""); }

static int items_row(void* ctx, const char* const* f) {
    items_sink* k = ctx;
    sel_library_item* it;

    if (k->n >= k->max) return 1;
    it = &k->out[k->n++];
    it->kind = (unsigned)strtoul(f[0], NULL, 10);
    copy_text(it->key, sizeof it->key, f[1]);
    copy_text(it->name, sizeof it->name, f[2]);
    copy_text(it->sub, sizeof it->sub, f[3]);
    it->songs = (unsigned)strtoul(f[4], NULL, 10);
    it->added = (unsigned)strtoul(f[5], NULL, 10);
    it->last_played = (unsigned)strtoul(f[6], NULL, 10);
    it->art_id = (unsigned)strtoul(f[7], NULL, 10);
    it->pinned = f[8][0] == '1';
    it->opened = (unsigned)strtoul(f[9], NULL, 10);
    copy_text(it->format, sizeof it->format, f[10]);
    return 0;
}

int sel_library_items(sel_library* lib, uint32_t kind_mask, uint32_t offset, sel_library_item* out, uint32_t max, uint32_t* total) {
    items_sink k;
    sel_genre_op op;
    int64_t rows = 0;

    if (total != NULL) *total = 0;
    if (lib == NULL || out == NULL) return -1;
    k.out = out;
    k.n = 0;
    k.max = max;
    sel_source_register_builtin(); /* the worker's reg table reads the registry; register on this thread first */
    memset(&op, 0, sizeof op);
    op.op = SEL_LIBRARY_ITEMS;
    op.mask = kind_mask;
    op.offset = offset;
    op.limit = max;
    op.row = items_row;
    op.row_ctx = &k;
    if (sel_genre_call(lib, &op, &rows) < 0) return -1;
    if (total != NULL) *total = (uint32_t)rows;
    return (int)k.n;
}

/* ---- I678f: the lists of a collection page (design doc section 8) ----
 * One statement per (kind, shelf). Every statement returns the five columns key, name, artist, year, number and binds the item key as ?1. */
#define PLAYS "(SELECT COUNT(*) FROM history h WHERE h.key=s.key AND h.skipped=0)"
#define SONG_COLS "SELECT s.key,IFNULL(s.name,''),IFNULL(s.artist,''),IFNULL(s.album,'')," PLAYS " FROM songs s"
#define ALBUM_COLS \
    "SELECT CAST(a.id AS TEXT),a.name,IFNULL(a.artist,''),IFNULL(a.year,0)," \
    "(SELECT COUNT(*) FROM songs s WHERE s.album IS a.name AND s.artist IS a.artist) FROM albums a"
#define BY_PLAYS " ORDER BY 5 DESC, 2 COLLATE NOCASE, s.id"
#define NEWEST " ORDER BY IFNULL(a.year,0) DESC, a.name COLLATE NOCASE, a.id"

static const char* collection_sql(uint32_t kind, uint32_t shelf) {
    switch (shelf) {
    case SEL_COLL_SONGS:
        if (kind == SEL_LIB_ITEM_ALBUM) {
            return SONG_COLS " JOIN albums a ON a.id=CAST(?1 AS INTEGER) AND s.album IS a.name AND s.artist IS a.artist ORDER BY s.id";
        }
        if (kind == SEL_LIB_ITEM_ARTIST) return SONG_COLS " WHERE s.artist=?1" BY_PLAYS;
        if (kind == SEL_LIB_ITEM_COMPOSER) {
            return SONG_COLS " WHERE CASE WHEN ?1='" SEL_COMPOSER_UNKNOWN_CANON "' THEN NOT EXISTS"
                             " (SELECT 1 FROM song_composers c WHERE c.song_key=s.key)"
                             " ELSE s.key IN (SELECT song_key FROM song_composers WHERE canon=?1) END" BY_PLAYS;
        }
        if (kind == SEL_LIB_ITEM_LIKED) return SONG_COLS " JOIN liked l ON l.key=s.key ORDER BY l.at DESC, s.id";
        if (kind == SEL_LIB_ITEM_RECENT) { /* I678g: newest play first; the number column is the time of the last play, not a play count */
            return "SELECT s.key,IFNULL(s.name,''),IFNULL(s.artist,''),IFNULL(s.album,''),h.t FROM songs s"
                   " JOIN (SELECT key, MAX(played_at) AS t FROM history GROUP BY key) h ON h.key=s.key ORDER BY h.t DESC, s.id";
        }
        if (kind == SEL_LIB_ITEM_LOCAL) return SONG_COLS " WHERE s.source='local' ORDER BY s.id";
        if (kind == SEL_LIB_ITEM_SOURCE) return SONG_COLS " WHERE s.source=?1 ORDER BY s.id";
        return NULL;
    case SEL_COLL_ALBUMS: return kind == SEL_LIB_ITEM_ARTIST ? ALBUM_COLS " WHERE a.artist=?1" NEWEST : NULL;
    case SEL_COLL_APPEARS:
        return kind == SEL_LIB_ITEM_ARTIST
                   ? ALBUM_COLS " WHERE IFNULL(a.artist,'')<>?1 AND EXISTS (SELECT 1 FROM songs s WHERE s.album IS a.name AND s.artist=?1)" NEWEST
                   : NULL;
    case SEL_COLL_MORE:
        return kind == SEL_LIB_ITEM_ALBUM
                   ? ALBUM_COLS " WHERE a.id<>CAST(?1 AS INTEGER) AND IFNULL(a.artist,'')<>''"
                                " AND a.artist=(SELECT artist FROM albums WHERE id=CAST(?1 AS INTEGER))" NEWEST
                   : NULL;
    case SEL_COLL_HEAD: return kind == SEL_LIB_ITEM_ALBUM ? ALBUM_COLS " WHERE a.id=CAST(?1 AS INTEGER)" : NULL;
    case SEL_COLL_SPELLINGS:
        return kind == SEL_LIB_ITEM_COMPOSER ? "SELECT display,display,'',0,n FROM composer_spellings WHERE canon=?1 ORDER BY n DESC, display"
                                             : NULL;
    default: return NULL;
    }
}

/* I790: one level of a source page. Every statement filters on `source` and an index of schema v14 (songs_source_format,
 * songs_source_artist) and ends in LIMIT/OFFSET, so no query loads a whole source. SUM(size) is NULL until I801 fills sizes. */
#define DRILL_SONG "SELECT s.key,IFNULL(s.name,''),IFNULL(s.artist,''),IFNULL(s.album,''),0 FROM songs s"
static const char* drill_sql(uint32_t level) {
    switch (level) {
    case SEL_DRILL_FORMATS: return "SELECT IFNULL(format,''),COUNT(*),SUM(size),'','' FROM songs WHERE source=?1 GROUP BY format ORDER BY 2 DESC LIMIT ?5 OFFSET ?4";
    case SEL_DRILL_FORMAT_ARTISTS:
        return "SELECT IFNULL(artist,''),COUNT(*),SUM(size),'','' FROM songs WHERE source=?1 AND format IS NULLIF(?2,'') GROUP BY artist"
               " ORDER BY artist COLLATE NOCASE LIMIT ?5 OFFSET ?4";
    case SEL_DRILL_FORMAT_SONGS:
        return DRILL_SONG " WHERE s.source=?1 AND s.format IS NULLIF(?2,'') AND s.artist IS NULLIF(?3,'') ORDER BY s.name COLLATE NOCASE, s.id LIMIT ?5 OFFSET ?4";
    case SEL_DRILL_ARTISTS:
        return "SELECT IFNULL(artist,''),COUNT(*),SUM(size),'','' FROM songs WHERE source=?1 GROUP BY artist ORDER BY artist LIMIT ?5 OFFSET ?4"; /* index order: a NOCASE sort needs a temp B-tree of every artist (435 ms on 30k artists) */
    case SEL_DRILL_ARTIST_SONGS:
        return DRILL_SONG " WHERE s.source=?1 AND s.artist IS NULLIF(?2,'') ORDER BY s.name COLLATE NOCASE, s.id LIMIT ?5 OFFSET ?4";
    case SEL_DRILL_FORMAT_SAMPLE: /* one path of the format, for the family fold: a single index seek */
        return "SELECT IFNULL(path,''),1,NULL,'','' FROM songs WHERE source=?1 AND format IS NULLIF(?2,'') LIMIT 1";
    case SEL_DRILL_NEW:
        return "SELECT day,n,NULL,'','' FROM source_new WHERE source=?1 ORDER BY day DESC LIMIT ?5 OFFSET ?4";
    default: return NULL;
    }
}

/* key = source US level US a US b (US = 0x1f). Streams the window rows; *id = rows emitted (the sentinel the tests assert). */
static int drill_op(sel_library* lib, const sel_genre_op* op, int* found, int64_t* id) {
    char buf[1200];
    char* part[4] = {NULL, NULL, NULL, NULL};
    char* c;
    sqlite3_stmt* st = NULL;
    uint32_t level;
    const char* sql;
    int i, rc;
    int64_t total = 0;

    snprintf(buf, sizeof buf, "%s", op->key);
    part[0] = buf;
    for (i = 1, c = buf; i < 4 && (c = strchr(c, 0x1f)) != NULL; i++) {
        *c++ = '\0';
        part[i] = c;
    }
    level = part[1] != NULL ? (uint32_t)strtoul(part[1], NULL, 10) : 99u;
    sql = drill_sql(level);
    *found = 1;
    if (sql == NULL) return SQLITE_OK;
    rc = sqlite3_prepare_v2(lib->db, sql, -1, &st, NULL);
    if (rc != SQLITE_OK) return rc;
    sqlite3_bind_text(st, 1, part[0], -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, part[2] != NULL ? part[2] : "", -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 3, part[3] != NULL ? part[3] : "", -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(st, 4, (sqlite3_int64)op->at);
    sqlite3_bind_int64(st, 5, op->after > 0 ? (sqlite3_int64)op->after : 1000);
    while ((rc = sqlite3_step(st)) == SQLITE_ROW) {
        char count[24], size[24];
        const char* f[6];

        num(count, sizeof count, st, 1);
        if (level == SEL_DRILL_FORMAT_SONGS || level == SEL_DRILL_ARTIST_SONGS) {
            f[0] = text(st, 0); f[1] = text(st, 1); f[2] = text(st, 2); f[3] = text(st, 3); f[4] = count; /* count column is 0 here */
        } else {
            if (sqlite3_column_type(st, 2) == SQLITE_NULL) size[0] = '\0'; else num(size, sizeof size, st, 2);
            f[0] = text(st, 0); f[1] = count; f[2] = size; f[3] = ""; f[4] = "";
        }
        f[5] = "";
        total++;
        (void)op->row(op->row_ctx, f);
    }
    sqlite3_finalize(st);
    if (rc != SQLITE_DONE) return rc;
    *id = total;
    return SQLITE_OK;
}

int sel_library_collection_op(sel_library* lib, const sel_genre_op* op, int* found, int64_t* id) {
    if (op->mask == SEL_COLL_DRILL && op->row != NULL && op->key != NULL) return drill_op(lib, op, found, id);
    const char* sql = collection_sql(op->offset, op->mask);
    sqlite3_stmt* st = NULL;
    int64_t total = 0;
    int rc;

    *found = 0;
    *id = 0;
    if (op->row == NULL || op->key == NULL) return SQLITE_MISUSE;
    if (sql == NULL) { /* a shelf the kind does not have is an empty list */
        *found = 1;
        return SQLITE_OK;
    }
    rc = sqlite3_prepare_v2(lib->db, sql, -1, &st, NULL);
    if (rc != SQLITE_OK) return rc;
    sqlite3_bind_text(st, 1, op->key, -1, SQLITE_TRANSIENT);
    while ((rc = sqlite3_step(st)) == SQLITE_ROW) {
        char year[24], number[24];
        const char* f[6];
        char fmt[16] = "";

        num(year, sizeof year, st, 3);
        num(number, sizeof number, st, 4);
        f[0] = text(st, 0);
        f[1] = text(st, 1);
        f[2] = text(st, 2);
        f[3] = op->mask == SEL_COLL_SONGS ? text(st, 3) : year;
        f[4] = number;
        if (op->mask == SEL_COLL_ALBUMS || op->mask == SEL_COLL_APPEARS || op->mask == SEL_COLL_MORE || op->mask == SEL_COLL_HEAD) {
            sel_db_majority_format_cached(lib, SEL_LIB_ITEM_ALBUM, f[0], fmt, sizeof fmt); /* I691c: f[0] is the album id */
        }
        f[5] = fmt;
        total++;
        (void)op->row(op->row_ctx, f);
    }
    sqlite3_finalize(st);
    if (rc != SQLITE_DONE) return rc;
    *id = total;
    *found = 1;
    return SQLITE_OK;
}

typedef struct collection_sink {
    sel_collection_row_fn fn;
    void* ctx;
} collection_sink;

static int collection_row(void* ctx, const char* const* f) {
    collection_sink* k = ctx;

    k->fn(k->ctx, f);
    return 0;
}

int sel_library_source_drill(sel_library* lib, const char* source, uint32_t level, const char* a, const char* b, uint32_t skip,
                             uint32_t limit, sel_collection_row_fn fn, void* ctx) {
    collection_sink k;
    sel_genre_op op;
    char key[1200];
    int64_t rows = 0;

    if (lib == NULL || source == NULL || fn == NULL) return -1;
    snprintf(key, sizeof key, "%s\x1f%u\x1f%s\x1f%s", source, (unsigned)level, a != NULL ? a : "", b != NULL ? b : "");
    k.fn = fn;
    k.ctx = ctx;
    memset(&op, 0, sizeof op);
    op.op = SEL_COLLECTION;
    op.key = key;
    op.offset = SEL_LIB_ITEM_SOURCE;
    op.mask = SEL_COLL_DRILL;
    op.at = skip;
    op.after = limit;
    op.row = collection_row;
    op.row_ctx = &k;
    if (sel_genre_call(lib, &op, &rows) < 0) return -1;
    return (int)rows;
}

int sel_library_collection(sel_library* lib, uint32_t kind, const char* key, uint32_t shelf, sel_collection_row_fn fn, void* ctx) {
    collection_sink k;
    sel_genre_op op;
    int64_t rows = 0;

    if (lib == NULL || key == NULL || fn == NULL) return -1;
    k.fn = fn;
    k.ctx = ctx;
    memset(&op, 0, sizeof op);
    op.op = SEL_COLLECTION;
    op.key = key;
    op.offset = kind;
    op.mask = shelf;
    op.row = collection_row;
    op.row_ctx = &k;
    if (sel_genre_call(lib, &op, &rows) < 0) return -1;
    return (int)rows;
}

/* ---- I678j: the item lines of the Home feed (design doc section 11) ----
 * Runs on the worker inside SEL_USER_HOME (db in hand), so it cannot call sel_library_items. It reuses build_sql: the same parts, so
 * Home and the sidebar read one source. Recency is the newer of last_played and opened, as the Recents order of section 5. */
#define HOME_T "t(k,ik,nm,sub,songs,added,lp,art,pinned,sn,tie,op)"
#define HOME_REC "MAX(lp,op)"
/* History of the item as one aggregate over its songs' history rows (albums by name and artist, composers by canon, playlists by entry);
 * `agg` is the aggregate, `cond` an extra condition on h. HOME_PLAYS counts every play, a skipped one included, as the Recents recency of
 * section 5. HOME_FIN counts finished plays only and HOME_FIN_LAST is the time of the last one: Recently Played and Your Albums use
 * these (Johan 2026-10-29; an album with only skipped plays stays on Jump Back In). */
#define HOME_HIST(agg, cond) \
    "(CASE k WHEN 4 THEN (SELECT " agg " FROM history h CROSS JOIN songs s ON s.key=h.key WHERE s.album IS nm AND s.artist IS NULLIF(sub,'')" cond ")" \
    " WHEN 6 THEN (SELECT " agg " FROM history h CROSS JOIN song_composers c ON c.song_key=h.key WHERE c.canon=ik" cond ")" \
    " WHEN 3 THEN (SELECT " agg " FROM history h JOIN playlist_entries e ON e.key=h.key WHERE e.playlist_id=CAST(ik AS INTEGER)" cond ")" \
    " ELSE 0 END)"
#define HOME_PLAYS HOME_HIST("COUNT(*)", "")
#define HOME_FIN HOME_HIST("COUNT(*)", " AND h.skipped=0")
#define HOME_FIN_LAST "COALESCE(" HOME_HIST("MAX(h.played_at)", " AND h.skipped=0") ",0)"

/* Format families of the Home chips, one place (lower-case songs.format values). Modules: tracker formats; Chiptunes: SID, AY, SAP and the
 * other chip-synth formats; Music: everything else (a song with no format included). */
#define HOME_TRACKERS "'mod','xm','it','s3m','med','mmd0','mmd1','mmd2','mmd3','stk','nst','okt','digi','dbm','far','ult','mtm','669','gtk','symmod','ahx','hvl','fc14','fc13','hip','hipc','hip7','dw','fred','bp','bp3','sid1','sid2','dm1','dm2','sfx','tfx','mc','rh'"
#define HOME_CHIPS_FMT "'sid','psid','rsid','ay','ym','sap','nsf','nsfe','spc','gbs','vgm','vgz','kss','hes','gym','pt3','pt2','stc','sqt','asc','psc','fxm','pt1','stp'"

void sel_home_chip_sql(uint32_t chip, const char* col, char* out, size_t cap) {
    switch (chip) {
    case SEL_HOME_CHIP_MODULES: snprintf(out, cap, "lower(COALESCE(%s,'')) IN (" HOME_TRACKERS ")", col); break;
    case SEL_HOME_CHIP_CHIPTUNES: snprintf(out, cap, "lower(COALESCE(%s,'')) IN (" HOME_CHIPS_FMT ")", col); break;
    case SEL_HOME_CHIP_MUSIC: snprintf(out, cap, "lower(COALESCE(%s,'')) NOT IN (" HOME_TRACKERS "," HOME_CHIPS_FMT ")", col); break;
    default: snprintf(out, cap, "1"); break;
    }
}

int sel_home_items(sqlite3* db, uint32_t shelf, uint32_t chip, uint32_t limit, sel_home_item_fn fn, void* ctx) {
    static const char* const tail[SEL_HOME_SHELVES] = {
        /* quick access: Liked Songs first, then the newest by recency */
        " WHERE (k=0 OR " HOME_REC ">0) AND %s ORDER BY (k<>0)," HOME_REC " DESC, sn COLLATE NOCASE, tie LIMIT ?1",
        /* recently played items: albums, playlists, composers with a play */
        " WHERE k IN (3,4,6) AND " HOME_FIN ">0 AND %s ORDER BY " HOME_FIN_LAST " DESC, sn COLLATE NOCASE, tie LIMIT ?1",
        /* your albums, most played first */
        " WHERE k=4 AND " HOME_FIN ">0 AND %s ORDER BY " HOME_FIN " DESC, sn COLLATE NOCASE, tie LIMIT ?1",
        /* your composers, most played first (every composer, so a library with no plays still lists them; the Unknown composer group is
         * not a composer, so a library with none has no shelf) */
        " WHERE k=6 AND ik<>'" SEL_COMPOSER_UNKNOWN_CANON "' AND %s ORDER BY " HOME_PLAYS " DESC, sn COLLATE NOCASE, tie LIMIT ?1",
        /* your playlists, most played first */
        " WHERE k=3 AND %s ORDER BY " HOME_PLAYS " DESC, sn COLLATE NOCASE, tie LIMIT ?1"};
    static const uint32_t mask[SEL_HOME_SHELVES] = {
        SEL_LIB_ITEM_BIT(SEL_LIB_ITEM_LIKED) | SEL_LIB_ITEM_BIT(SEL_LIB_ITEM_PLAYLIST) | SEL_LIB_ITEM_BIT(SEL_LIB_ITEM_ALBUM) |
            SEL_LIB_ITEM_BIT(SEL_LIB_ITEM_ARTIST) | SEL_LIB_ITEM_BIT(SEL_LIB_ITEM_COMPOSER),
        SEL_LIB_ITEM_BIT(SEL_LIB_ITEM_PLAYLIST) | SEL_LIB_ITEM_BIT(SEL_LIB_ITEM_ALBUM) | SEL_LIB_ITEM_BIT(SEL_LIB_ITEM_COMPOSER),
        SEL_LIB_ITEM_BIT(SEL_LIB_ITEM_ALBUM), SEL_LIB_ITEM_BIT(SEL_LIB_ITEM_COMPOSER), SEL_LIB_ITEM_BIT(SEL_LIB_ITEM_PLAYLIST)};
    char inner[8192], sql[9400], fam[600], has[1800];
    sqlite3_stmt* st = NULL;
    uint32_t n = 0;
    int rc;

    if (shelf >= SEL_HOME_SHELVES || !build_sql(mask[shelf], inner, sizeof inner)) return SQLITE_MISUSE;
    /* The chip keeps an item with a song of the family; Quick access is not filtered (design doc section 11: the chips filter the shelves). */
    sel_home_chip_sql(shelf == SEL_HOME_QUICK ? SEL_HOME_CHIP_ALL : chip, "s.format", fam, sizeof fam);
    if (shelf == SEL_HOME_QUICK || chip == SEL_HOME_CHIP_ALL) {
        snprintf(has, sizeof has, "1");
    } else snprintf(has, sizeof has,
             "(CASE k WHEN 4 THEN EXISTS(SELECT 1 FROM songs s WHERE s.album IS nm AND s.artist IS NULLIF(sub,'') AND %s)"
             " WHEN 6 THEN EXISTS(SELECT 1 FROM song_composers c JOIN songs s ON s.key=c.song_key WHERE c.canon=ik AND %s)"
             " WHEN 3 THEN EXISTS(SELECT 1 FROM playlist_entries e JOIN songs s ON s.key=e.key WHERE e.playlist_id=CAST(ik AS INTEGER) AND %s)"
             " ELSE 1 END)",
             fam, fam, fam);
    if (shelf == SEL_HOME_RECENT) {
        /* Johan 2026-10-29: a stream or Modland play (a history key with no songs row, not a vanished library file) is a single-song card
         * in this shelf, ordered by its last play together with the items. Its format is the one stored with the play (history.format), so it files under the Modules, Chiptunes or Music chip. */
        char nofam[600];
        sel_home_chip_sql(chip, "h.format", nofam, sizeof nofam);
        snprintf(sql, sizeof sql,
                 "WITH " HOME_T " AS (%s) SELECT k,ik,nm,sub FROM ("
                 "SELECT k,ik,nm,sub," HOME_FIN_LAST " AS lpx,sn,tie FROM t WHERE k IN (3,4,6) AND " HOME_FIN ">0 AND %s"
                 " UNION ALL SELECT %u,h.key,h.key,'',MAX(h.played_at),h.key,h.key FROM history h"
                 " WHERE NOT EXISTS(SELECT 1 FROM songs s WHERE s.key=h.key) AND substr(h.key,1,6)<>'local:' AND %s GROUP BY h.key)"
                 " ORDER BY lpx DESC, sn COLLATE NOCASE, tie LIMIT ?1",
                 inner, has, (unsigned)SEL_HOME_ITEM_SONG, nofam);
    } else {
        char tl[3000];
        snprintf(tl, sizeof tl, tail[shelf], has);
        snprintf(sql, sizeof sql, "WITH " HOME_T " AS (%s) SELECT k,ik,nm,sub FROM t%s", inner, tl);
    }
    rc = sqlite3_prepare_v2(db, sql, -1, &st, NULL);
    if (rc != SQLITE_OK) return rc;
    sqlite3_bind_int64(st, 1, limit);
    while (sqlite3_step(st) == SQLITE_ROW) {
        fn(ctx, (uint32_t)sqlite3_column_int(st, 0), text(st, 1), text(st, 2), text(st, 3));
        n++;
    }
    sqlite3_finalize(st);
    return (int)n;
}
