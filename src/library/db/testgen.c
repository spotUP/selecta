/* I847a: the deterministic test catalogue every perf fixture shares (design 2026-10-31-interactive-path.md section 4). One generator, used by
 * `selecta-cli library gen`, `library bench` and the library-gen-bench test; the 300k best-source test and the drill tests keep their own
 * tiny fills only because they assert exact row identities. Mix for n songs: 86 % modland rows, 10 % local, 4 % other sources (hvsc, hippo,
 * jamendo, pins); one history row per 12 songs, one like per 300; one in ten modland and local rows shares an md5 with its twin, so
 * best-source has real groups. Artist and title words come from a built-in word list (the checked-in Modland path sample of the design is
 * not in the tree); the list carries the bench search words. */
#include "db_internal.h"

#include <stdio.h>

#define BATCH 50000

static const char* const WORDS[] = {"Purple", "Motion", "4mat",   "Chip",    "Chiptune", "Jester", "Lizardking", "Zodiak", "Moby",   "Skaven",
                                    "Necros",  "Radix",  "Dizzy",  "Elwood",  "Fred",     "Gargoyle", "Hunter",   "Ice",    "Jogeir", "Karl",
                                    "Laxity",  "Maze",   "Nightfall", "Orion", "Pixel",   "Quasar",   "Raven",    "Sonic",  "Tempest", "Uranus",
                                    "Vortex",  "Wasp",   "Xenon",  "Yoyo",    "Zeta",     "Acid",     "Beat",     "Cosmic", "Dream",  "Echo",
                                    "Funk",    "Groove", "Hyper",  "Ibiza",   "Jungle",   "Kraft",    "Lunar",    "Magic",  "Neon",   "Omega",
                                    "Piano",   "Quest",  "Retro",  "Space",   "Tracker",  "Ultra",    "Voyage",   "Wave",   "Xtreme", "Youth",
                                    "Zone",    "Charge", "Church", "Chrome"};
#define NWORDS (sizeof WORDS / sizeof WORDS[0])

#define PICK(k, m) "(SELECT word FROM gen_w WHERE n = (i * " #k ") % " #m ")"
#define ARTIST PICK(7919, 64) " || ' ' || " PICK(104729, 61)
#define TITLE PICK(31, 64) " || ' ' || " PICK(131, 59)

static int gen_exec(sqlite3* db, const char* sql) {
    if (sqlite3_exec(db, sql, NULL, NULL, NULL) == SQLITE_OK) return 1;
    fprintf(stderr, "library gen: %s\n", sqlite3_errmsg(db));
    return 0;
}

/* Inserts songs for ids [1, count] in batches; `select` is the SELECT over the recursive range n(i). */
static int gen_range(sqlite3* db, const char* select, uint32_t count) {
    char sql[2400];
    uint32_t a;

    for (a = 1; a <= count; a += BATCH) {
        uint32_t b = a + BATCH - 1 < count ? a + BATCH - 1 : count;

        snprintf(sql, sizeof sql,
                 "WITH RECURSIVE n(i) AS (SELECT %u UNION ALL SELECT i + 1 FROM n WHERE i < %u)"
                 " INSERT INTO songs(key, source, path, name, artist, album, format, added, md5, size) %s",
                 (unsigned)a, (unsigned)b, select);
        if (!gen_exec(db, sql)) return 0;
    }
    return 1;
}

int sel_library_gen(sel_library* lib, uint32_t songs) {
    sqlite3* db;
    char sql[2400];
    uint32_t i, n_mod, n_loc, n_oth;

    if (lib == NULL || songs < 100) return 0;
    db = lib->db;
    n_mod = (uint32_t)((uint64_t)songs * 86 / 100);
    n_loc = songs / 10;
    n_oth = songs - n_mod - n_loc;
    if (!gen_exec(db, "CREATE TEMP TABLE gen_w(n INTEGER PRIMARY KEY, word TEXT);")) return 0;
    for (i = 0; i < NWORDS; i++) {
        snprintf(sql, sizeof sql, "INSERT INTO gen_w VALUES(%u, '%s');", (unsigned)i, WORDS[i]);
        if (!gen_exec(db, sql)) return 0;
    }
    /* Modland: a skewed format mix (real Modland is mostly Protracker, Fasttracker 2, Impulsetracker, then a long tail). */
    if (!gen_range(db,
                   "SELECT 'modland:' || i, 'modland', '/pub/modules/' || fmt || '/' || a || '/' || t || ' ' || i, t || ' ' || i, a, '', fmt, 1,"
                   " CASE WHEN i % 20 < 2 THEN 'md5-' || (i / 20) ELSE NULL END, 1000 + i % 90000"
                   " FROM (SELECT i, " ARTIST " AS a, " TITLE " AS t,"
                   " CASE i % 10 WHEN 0 THEN 'Protracker' WHEN 1 THEN 'Fasttracker 2' WHEN 2 THEN 'Impulsetracker' WHEN 3 THEN 'Protracker'"
                   " WHEN 4 THEN 'Fasttracker 2' ELSE 'format' || (i % 60) END AS fmt FROM n);",
                   n_mod))
        return 0;
    if (!gen_range(db,
                   "SELECT '/m/' || i, 'local', '/Music/' || a || '/' || t || ' ' || i || '.mod', t || ' ' || i, a, 'Album ' || (i % 3000),"
                   " 'Protracker', 1, CASE WHEN i % 20 < 2 THEN 'md5-' || (i / 20) ELSE NULL END, 2000 + i % 50000"
                   " FROM (SELECT i, " ARTIST " AS a, " TITLE " AS t FROM n);",
                   n_loc))
        return 0;
    if (!gen_range(db,
                   "SELECT CASE s WHEN 0 THEN 'hvsc:' WHEN 1 THEN 'hippo:' WHEN 2 THEN 'jamendo:' ELSE 'pins:' END || i,"
                   " CASE s WHEN 0 THEN 'hvsc' WHEN 1 THEN 'hippo' WHEN 2 THEN 'jamendo' ELSE 'pins' END, '/o/' || i, t || ' ' || i, a, '', 'SID',"
                   " 1, NULL, NULL FROM (SELECT i, i % 4 AS s, " ARTIST " AS a, " TITLE " AS t FROM n);",
                   n_oth))
        return 0;
    snprintf(sql, sizeof sql,
             "WITH RECURSIVE n(i) AS (SELECT 1 UNION ALL SELECT i + 1 FROM n WHERE i < %u)"
             " INSERT INTO history(key, played_at, duration_ms, skipped) SELECT 'modland:' || (1 + (i * 7919) %% %u), 1700000000 + i * 60,"
             " 180000, i %% 9 = 0 FROM n;",
             (unsigned)(songs / 12), (unsigned)n_mod);
    if (!gen_exec(db, sql)) return 0;
    snprintf(sql, sizeof sql,
             "WITH RECURSIVE n(i) AS (SELECT 1 UNION ALL SELECT i + 1 FROM n WHERE i < %u)"
             " INSERT OR IGNORE INTO liked(key, at) SELECT 'modland:' || (1 + (i * 104729) %% %u), 1700000000 + i FROM n;",
             (unsigned)(songs / 300), (unsigned)n_mod);
    if (!gen_exec(db, sql)) return 0;
    if (sel_dup_rebuild(db) != SQLITE_OK) return 0; /* I1094b: bulk write, groups resolved once */
    return gen_exec(db, "DROP TABLE gen_w;ANALYZE;");
}
