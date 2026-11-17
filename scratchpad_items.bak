/* The Your Library item list: pinned rows, playlists, albums, artists, composers and sources in one query (design doc
 * 2026-10-29-desktop-library-architecture.md section 2, I678a). The SQL is assembled from one part per kind in the caller's mask,
 * so a chip that wants only albums does not pay for the artist scan. Rows stream through op->row like SEL_COMPOSER_LIST: only the
 * window [op->offset, op->offset + op->limit) is emitted while every matching row is counted, so a 30 000 album library never
 * lands in one array and the caller still learns the total (reply [2]).
 *
 * Reused, not reinvented: the maintained tables composers (v7) and source_songs (v8) for their counts, the genre op streaming path
 * (sel_genre_call buffers the rows on the worker and replays them on the caller's thread), history.key and playlist_entries keys. */
#include "db_internal.h"

#include "../composer.h"

#include <selecta/library_item.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Columns of every part: 1 kind, 2 key, 3 name, 4 sub, 5 songs, 6 added, 7 last_played, 8 art_id, 9 pinned, 10 sort name, 11 tie. */
#define PART_LIKED \
    "SELECT 0,'liked','Liked Songs','',(SELECT COUNT(*) FROM liked),0,IFNULL((SELECT MAX(at) FROM liked),0),0,1,'',0"
#define PART_RECENT \
    "SELECT 1,'recent','Recently Played','',(SELECT COUNT(DISTINCT key) FROM history),0," \
    "IFNULL((SELECT MAX(played_at) FROM history),0),0,1,'',0"
#define PART_LOCAL \
    "SELECT 2,'local','Local Files','',IFNULL((SELECT n FROM source_songs WHERE source='local'),0),0,0,0,1,'',0"
/* Playlists in creation order (ids grow); owner "You": every saved playlist is local. added = id, as the sidebar used. */
#define PART_PLAYLIST \
    "SELECT 3,CAST(p.id AS TEXT),p.name,'You',(SELECT COUNT(*) FROM playlist_entries e WHERE e.playlist_id=p.id),p.id," \
    "IFNULL((SELECT MAX(h.played_at) FROM playlist_entries e JOIN history h ON h.key=e.key WHERE e.playlist_id=p.id),0),0,0," \
    "'',p.id FROM playlists p"
#define PART_ALBUM \
    "SELECT 4,CAST(a.id AS TEXT),a.name,IFNULL(a.artist,''),IFNULL(sa.n,0),IFNULL(sa.ad,0),IFNULL(la.t,0),a.id,0," \
    "a.name,a.id FROM albums a LEFT JOIN sa ON sa.al IS a.name AND sa.ar IS a.artist" \
    " LEFT JOIN la ON la.al IS a.name AND la.ar IS a.artist"
#define PART_ARTIST \
    "SELECT 5,r.name,r.name,'',IFNULL(sr.n,0),IFNULL(sr.ad,0),IFNULL(lr.t,0),0,0,r.name,0 FROM" \
    " (SELECT artist AS name FROM songs WHERE artist<>'' UNION SELECT artist FROM albums WHERE artist<>'') r" \
    " LEFT JOIN sr ON sr.ar=r.name LEFT JOIN lr ON lr.ar=r.name"
/* Composers: the maintained table plus the Unknown composer group, like SEL_COMPOSER_LIST. */
#define PART_COMPOSER \
    "SELECT 6,c.canon,c.display,'',c.songs,0,IFNULL(lc.t,0),0,0,c.display,0 FROM composers c LEFT JOIN lc ON lc.canon=c.canon" \
    " UNION ALL SELECT 6,'" SEL_COMPOSER_UNKNOWN_CANON "','" SEL_COMPOSER_UNKNOWN_DISPLAY "','',n,0,0,0,0,'~',1" \
    " FROM composer_unknown WHERE n>0"
/* Sources: the Local Files pinned row already stands for source 'local'. */
#define PART_SOURCE \
    "SELECT 7,s.source,s.source,'',s.n,0,IFNULL(ls.t,0),0,0,s.source,0 FROM source_songs s LEFT JOIN ls ON ls.source=s.source" \
    " WHERE s.source<>'local' AND s.n>0"

static const char* const k_part[SEL_LIB_ITEM_KINDS] = {PART_LIKED, PART_RECENT, PART_LOCAL,    PART_PLAYLIST,
                                                       PART_ALBUM, PART_ARTIST, PART_COMPOSER, PART_SOURCE};

/* Helper tables the parts join (a CTE nobody reads costs nothing). */
#define CTE_SA "sa AS (SELECT album AS al, artist AS ar, COUNT(*) AS n, MAX(added) AS ad FROM songs GROUP BY album, artist)"
#define CTE_LA \
    "la AS (SELECT s.album AS al, s.artist AS ar, MAX(h.played_at) AS t FROM history h JOIN songs s ON s.key=h.key" \
    " GROUP BY s.album, s.artist)"
#define CTE_SR "sr AS (SELECT artist AS ar, COUNT(*) AS n, MAX(added) AS ad FROM songs WHERE artist<>'' GROUP BY artist)"
#define CTE_LR \
    "lr AS (SELECT s.artist AS ar, MAX(h.played_at) AS t FROM history h JOIN songs s ON s.key=h.key WHERE s.artist<>''" \
    " GROUP BY s.artist)"
#define CTE_LC \
    "lc AS (SELECT c.canon AS canon, MAX(h.played_at) AS t FROM history h JOIN song_composers c ON c.song_key=h.key GROUP BY c.canon)"
#define CTE_LS \
    "ls AS (SELECT s.source AS source, MAX(h.played_at) AS t FROM history h JOIN songs s ON s.key=h.key GROUP BY s.source)"

#define CTE_COUNT 6u

static size_t put(char* sql, size_t cap, size_t n, const char* text) {
    size_t len = strlen(text);

    if (n + len + 1u > cap) return cap; /* overflow marks the statement unusable */
    memcpy(sql + n, text, len + 1u);
    return n + len;
}

/* Builds the statement for `mask`; 0 when it does not fit or the mask names no kind. */
static int build_sql(uint32_t mask, char* sql, size_t cap) {
    static const char* const cte[CTE_COUNT] = {CTE_SA, CTE_LA, CTE_SR, CTE_LR, CTE_LC, CTE_LS};
    /* bit i = cte[i]: albums read sa+la, artists sr+lr, composers lc, sources ls. */
    static const uint32_t need[SEL_LIB_ITEM_KINDS] = {0, 0, 0, 0, 0x03u, 0x0Cu, 0x10u, 0x20u};
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
            n = put(sql, cap, n, cte[i]);
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

int sel_library_items_op(sel_library* lib, const sel_genre_op* op, int* found, int64_t* id) {
    char sql[4096];
    sqlite3_stmt* st = NULL;
    int64_t total = 0;
    uint32_t emitted = 0;
    int rc;

    *found = 0;
    *id = 0;
    if (op->row == NULL || !build_sql(op->mask, sql, sizeof sql)) return SQLITE_MISUSE;
    rc = sqlite3_prepare_v2(lib->db, sql, -1, &st, NULL);
    if (rc != SQLITE_OK) return rc;
    while ((rc = sqlite3_step(st)) == SQLITE_ROW) {
        if (total >= (int64_t)op->offset && emitted < op->limit) {
            char kind[24], songs[24], added[24], last[24], art[24], pinned[24];
            const char* f[9];

            num(kind, sizeof kind, st, 0);
            num(songs, sizeof songs, st, 4);
            num(added, sizeof added, st, 5);
            num(last, sizeof last, st, 6);
            num(art, sizeof art, st, 7);
            num(pinned, sizeof pinned, st, 8);
            f[0] = kind;
            f[1] = text(st, 1);
            f[2] = text(st, 2);
            f[3] = text(st, 3);
            f[4] = songs;
            f[5] = added;
            f[6] = last;
            f[7] = art;
            f[8] = pinned;
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
