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
    "SELECT 5,r.name,r.name,'',IFNULL(sr.n,0),IFNULL(sr.ad,0),IFNULL(lr.t,0),0,0,r.name,0," \
    "IFNULL((SELECT at FROM opened WHERE kind=5 AND id=r.name),0) FROM" \
    " (SELECT artist AS name FROM songs WHERE artist<>'' UNION SELECT artist FROM albums WHERE artist<>'') r" \
    " LEFT JOIN sr ON sr.ar=r.name LEFT JOIN lr ON lr.ar=r.name"
/* Composers: the maintained table plus the Unknown composer group, like SEL_COMPOSER_LIST. */
#define PART_COMPOSER \
    "SELECT 6,c.canon,c.display,'',c.songs,0,IFNULL(lc.t,0),0,0,c.display,0," \
    "IFNULL((SELECT at FROM opened WHERE kind=6 AND id=c.canon),0) FROM composers c LEFT JOIN lc ON lc.canon=c.canon" \
    " UNION ALL SELECT 6,'" SEL_COMPOSER_UNKNOWN_CANON "','" SEL_COMPOSER_UNKNOWN_DISPLAY "','',n,0,0,0,0,'~',1," \
    "IFNULL((SELECT at FROM opened WHERE kind=6 AND id='" SEL_COMPOSER_UNKNOWN_CANON "'),0)" \
    " FROM composer_unknown WHERE n>0"
/* Sources: the Local Files pinned row already stands for source 'local'. */
#define PART_SOURCE \
    "SELECT 7,s.source,s.source,'',s.n,0,IFNULL(ls.t,0),0,0,s.source,0," \
    "IFNULL((SELECT at FROM opened WHERE kind=7 AND id=s.source),0) FROM source_songs s LEFT JOIN ls ON ls.source=s.source" \
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
            char kind[24], songs[24], added[24], last[24], art[24], pinned[24], opened[24];
            const char* f[10];

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

int sel_library_collection_op(sel_library* lib, const sel_genre_op* op, int* found, int64_t* id) {
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
        const char* f[5];

        num(year, sizeof year, st, 3);
        num(number, sizeof number, st, 4);
        f[0] = text(st, 0);
        f[1] = text(st, 1);
        f[2] = text(st, 2);
        f[3] = op->mask == SEL_COLL_SONGS ? text(st, 3) : year;
        f[4] = number;
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
/* Plays of the item: finished plays of its songs (albums by name and artist, composers by canon, playlists by entry). */
#define HOME_PLAYS \
    "(CASE k WHEN 4 THEN (SELECT COUNT(*) FROM history h JOIN songs s ON s.key=h.key WHERE s.album IS nm AND s.artist IS NULLIF(sub,''))" \
    " WHEN 6 THEN (SELECT COUNT(*) FROM history h JOIN song_composers c ON c.song_key=h.key WHERE c.canon=ik)" \
    " WHEN 3 THEN (SELECT COUNT(*) FROM history h JOIN playlist_entries e ON e.key=h.key WHERE e.playlist_id=CAST(ik AS INTEGER)) ELSE 0 END)"

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
        " WHERE k IN (3,4,6) AND lp>0 AND %s ORDER BY lp DESC, sn COLLATE NOCASE, tie LIMIT ?1",
        /* your albums, most played first */
        " WHERE k=4 AND " HOME_PLAYS ">0 AND %s ORDER BY " HOME_PLAYS " DESC, sn COLLATE NOCASE, tie LIMIT ?1",
        /* your composers, most played first (every composer, so a library with no plays still lists them) */
        " WHERE k=6 AND %s ORDER BY " HOME_PLAYS " DESC, sn COLLATE NOCASE, tie LIMIT ?1",
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
         * in this shelf, ordered by its last play together with the items. It has no format, so only the All and Music chips keep it. */
        char nofam[600];
        sel_home_chip_sql(chip, "NULL", nofam, sizeof nofam);
        snprintf(sql, sizeof sql,
                 "WITH " HOME_T " AS (%s) SELECT k,ik,nm,sub FROM ("
                 "SELECT k,ik,nm,sub,lp AS lpx,sn,tie FROM t WHERE k IN (3,4,6) AND lp>0 AND %s"
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
