---
status: draft
row: I1094
parent: I184
date: 2026-11-06
author: Opus design agent
---

# I1094 Hide duplicates and best of two copies

Ledger row: `thoughts/spot/todos.md` I1094. Source: `thoughts/shared/research/2026-11-05-s3btify-feature-gap.md`
(Library table, "Choose best of two copies; Hide duplicates option", S3btify 0.14.19 and 0.8.8). Line numbers are
against main `5642d19d` (2026-11-06). Design only; implementation rows I1094a.. in the ledger.

## Problem

The research row says "missing", but half of it exists. I184 (plan `2026-10-25-m3-library.md`, D-M3-14, T86) already
groups songs by md5 across sources and picks a best source:

- `src/library/db/duplicates.c`: `op_dup_next` (every group with two or more songs: md5, members, best key),
  `op_best` / `sel_library_best_source` (what OPEN plays), `hash_for_open` and the background md5 pass (I184a, I184b).
- `src/library/db/db_internal.h:173-177`: `SEL_SOURCE_RANK_SQL` (local 0, modland 3, hvsc 4, hippo 5, other 6) and
  `SEL_BEST_SOURCE_SQL`.
- `src/library/db/schema.c:267`: partial index `songs_md5 ON songs(md5) WHERE md5 <> ''`.
- CLI: `selecta-cli library duplicates` (`apps/selecta-cli/library.c:1321-1349`), proved by `tests/net/similar.cmake`
  (`dupes=3 open_best_source=1`) and `tests/m3/hash_idle_vamos.cmake` (Amiga, vamos).

What is missing: the library LISTS still show every copy (All tracks, search, albums, artists, composers, Home), the
user cannot see that a row has copies, cannot open them, and cannot override the choice. The best-source rank lives
in two macros used by two queries, and nothing persists "this row is a hidden copy", so a list cannot filter on it
without a correlated subquery per row (too slow on a 68020 for a 300k-row catalogue).

## Decisions already made (not re-litigated)

- D-M3-14: best source default local > home mirror > cache > modland > hvsc > hippo URL; ties by library order.
  Home mirror and cache are transports of a modland/hvsc key, not row sources (`duplicates.c:7-10`).
- Identity is md5 of the whole file (songs.md5, files.md5). HVSC rows get it from Songlengths.md5 (I162), catalogue
  rows from the catalogue when it publishes one, local rows from the background pass (I184a).
- RULES 4a: every feature runs on the Amiga or names an exemption. RULES 4: no allocation after start in engine.
- library.db is alpha data: migration needs no backup (memory "No backup of alpha db").

## 1. What counts as a duplicate

Two tiers. Only tier 1 is in scope of rows I1094a-h; tier 2 is gated on Decision 1.

**Tier 1, exact copy (reused identity).** Two or more songs with different keys and the same non-empty md5. This is
exactly the I184 group, so no new identity is invented. Same bytes means same format and same rip, so "best copy" in
tier 1 is purely a SOURCE choice (where to read it from), never a quality choice.

A song with no md5 yet (local file not hashed, catalogue row without one) is never a duplicate. It shows until the
background pass hashes it; then the maintenance in section 3 regroups and the row can disappear from lists on the
next refresh (I1077 write generation already drives that refresh).

The UADE key (`src/library/xxh32.h`, xxh32 of the first 256 KB) is NOT used as identity: it is a lookup key for
UADE's song database, collides by design for files that differ past 256 KB, and adds nothing md5 does not give.

**Tier 2, same tune, different file (Decision 1).** Same composer canon (`song_composers.canon`) or same artist,
same normalised title (case and punctuation folded, the FTS unicode61 fold), duration within 2 seconds, both
durations known. Examples: an MP3 and a FLAC of the same album track; a .mod and its rendered .ogg; two rips of one
SID with different headers. Here "best" is a quality choice. Tier 2 is a heuristic with false positives (remixes,
"(Live)" versions, two tunes named "Intro" by one composer), so it must never hide anything without a visible
"copies" badge, and it gets its own pref. Its rank: lossless audio > original tracker/chip module > lossy audio by
size (bytes per second as a bitrate proxy, songs.size / songs.duration_ms), then the tier 1 source rank, then id.

## 2. How the best copy is chosen (one ranking, one place)

Order, first difference wins:

1. User pick: the key in `dup_pick` for this group, when that song still exists.
2. Source rank: `SEL_SOURCE_RANK_SQL` (D-M3-14).
3. Larger `songs.size` (tier 1: equal by definition; kept so tier 2 uses the same expression).
4. Lowest `songs.id` (library order, the existing tie break).

This becomes ONE macro, `SEL_DUP_RANK_SQL` in `db_internal.h`, and every reader uses it: the maintained column
(section 3), `op_dup_next`, `SEL_BEST_SOURCE_SQL` (OPEN) and the CLI. Invariant: the copy a list shows is the copy
OPEN plays. Today OPEN and `library duplicates` agree only because both paste `SEL_SOURCE_RANK_SQL`; a user pick
added to one and not the other would break that, which is why the rank is merged first (I1094a).

Format quality does not enter tier 1 (same bytes). Source priority is not user-configurable in v1 beyond the
per-group pick (Decision 3).

## 3. Where it lives (store layer, single source of truth)

Schema migration, next free `user_version` at implementation time (V18 on `5642d19d`, `schema.c:281-283`):

```sql
ALTER TABLE songs ADD COLUMN dup_of TEXT;      -- NULL: unique or the best copy; else the best copy's key
ALTER TABLE songs ADD COLUMN dup_members INTEGER NOT NULL DEFAULT 1;  -- group size, 1 when unique
CREATE INDEX IF NOT EXISTS songs_dup_of ON songs(dup_of) WHERE dup_of IS NOT NULL;
CREATE TABLE IF NOT EXISTS dup_pick(md5 TEXT PRIMARY KEY, key TEXT NOT NULL) WITHOUT ROWID;
```

The migration ends with one set-based rebuild (below), so an existing library comes up with its groups resolved.
No backup offer (alpha).

Maintenance in `src/library/db/duplicates.c`, two functions, both called inside the writer's open transaction:

- `sel_dup_regroup(db, md5)`: recomputes one group by index seek on `songs_md5`: best key by `SEL_DUP_RANK_SQL`,
  sets `dup_of` NULL on the best and on every member of a one-member group, the best key on the others, and
  `dup_members` on all. Called by the single-row md5 writers: `store_hash`, `op_store` (SEL_DUP_STORE), the
  side-data writer (HVSC Songlengths md5, `db.h:267-276`), `LIBRARY_DUP_PICK`, and a song delete (old md5).
- `sel_dup_rebuild(db)`: one `UPDATE ... FROM (SELECT md5, best, n ...)` over all groups. Called once at the end of a
  bulk write instead of per row: scan batch (`scan.c` upsert sets md5), catalogue sync import, TSV import, migration.

No SQLite trigger: I577 rejected per-row triggers on the bulk paths for cost, and a trigger on `songs.md5` would
fire 300k times on a catalogue sync. The bulk/single split keeps every md5 write path covered without that cost;
I1094b proves coverage with an invariant check (`dup_of` equals a fresh recompute) after each writer in a test.

Readers (the lists) only ever read `songs.dup_of IS NULL`. Nothing outside the store computes groups.

Filter switch, the `library.sources_off` pattern (I265, `search.c:405-436`, `db_internal.h:63`):

- Pref `library.hide_duplicates` in `include/selecta/prefs.def` (default per Decision 2), pushed to the worker with
  `sel_library_set_hide_duplicates(lib, on)` like `sel_library_set_sources_off`.
- Applied in SQL, not post-filtered in C: the search arms (`search.c` arm wrapper, before paging, so a page of 300
  stays 300 visible rows), the collection lists (`items.c` I678f), Home feed lines (`sel_home_items`), radio
  candidates (`radio.c:44`, next to the source-off check). Source drill pages (`sel_library_source_drill`) and
  playlists are NOT filtered: a source page shows what that source holds, and a playlist entry is what the user put
  there (OPEN already plays the best copy of it).
- Query language keys in `src/library/query.c` (table at line 36): `duplicate` (K_BOOL, `songs.dup_of IS NOT NULL`)
  and `copies:<song key>` (all members of that song's md5 group, hidden or not). The "Show All Copies" view is the
  query `copies:<key>`, so it reuses the search path, paging and meta fill; with `copies:` in the query the hide
  filter is not applied.
- Meta fill (`search.c:503`) adds `copies` (= `dup_members`) per row, so the UI shows the badge without a query.

Item song counts (albums, artists, composers, I838 maintained tables) are Decision 4.

## 4. Engine command and CLI

- `PREF_SET library.hide_duplicates on|off` (existing pref path).
- New engine command `LIBRARY_DUP_PICK <song key>`: writes `dup_pick(md5 of key, key)`, runs `sel_dup_regroup`,
  replies `ok best=<key>`. `LIBRARY_DUP_PICK <song key> clear` removes the pick.
- `selecta-cli library duplicates` keeps its output (`group=` lines, `dupes=N`), reads the maintained columns
  instead of grouping on the fly, and adds `hidden=N` (rows with `dup_of` set).
- `selecta-cli library duplicates --pick <key>` and `--clear <key>`; `selecta-cli library copies <key>` lists the
  group (`copy=<key>|<source>|<best 0/1>`).
- Search and list output of the CLI follow the pref, like every other front end.

## 5. User-facing behaviour

- Default: Decision 2 (recommended on).
- Setting, desktop Settings > Library and MUI Settings > Library: checkbox **Hide Duplicate Songs**, help line
  "Show one copy of songs that exist in several places. The best copy plays."
- Row badge on a visible row with `copies > 1`: **2 Copies** (number from meta), clickable.
- Row context menu: **Show All Copies** (opens the `copies:<key>` list, title "Copies of <title>"), and in that
  list per row **Use This Copy** (LIBRARY_DUP_PICK) with the current best marked **Best Copy**; on a picked group
  **Use Automatic Choice** (clear).
- Tier 2 (if Decision 1 says yes): separate checkbox **Also Hide Same Song in Other Formats**, default off.

## 6. Reach per platform

- Mac desktop (`plugins/ui/desktop`, uimodel): toggle, badge, context menu, copies list. Spotify reference design
  for the badge (memory "Spotify reference design").
- Amiga MUI (`plugins/ui/mui`, MUI 3.8 only): checkbox in the Library settings page (beside the sources toggles,
  `mui_p8.c` already writes `library.sources_off`), copies column/badge in the track list, context menu entries.
- Amiga AGA/ECS text UI (`plugins/ui/text`): toggle in its settings screen and a `[2]` copies marker plus a
  "Show All Copies" key; ASCII only.
- Core on Amiga: the md5 pass already runs there (`tests/m3/hash_idle_vamos.cmake`); `sel_dup_regroup` is index
  seeks, `sel_dup_rebuild` is one statement in the existing bulk transaction. No exemption needed. I1094g measures
  the rebuild on the 060 rig with the full Modland + HVSC catalogue and records the time.

## 7. Reachability test (one, through the top-level entry point)

`tests/m3/hide_duplicates.cmake`, registered in `tests/CMakeLists.txt`, drives the real `selecta-cli` (pattern of
`tests/net/similar.cmake`):

Fixture: 10 unique songs plus three planted groups with shared md5 (local files hashed by the real pass):
A = local + modland (2), B = local + modland + hvsc (3), C = modland + hvsc (2). 17 songs, 7 in groups.

Steps and asserted numbers:

1. `selecta-cli library duplicates --db` -> `dupes=3 hidden=4`; best keys A=local, B=local, C=modland.
2. Sentinel: `selecta-cli library stats` prints `dup_rebuilds=` (counter bumped inside `sel_dup_rebuild`) >= 1 and
   `dup_regroups=` >= 2 (the local hashes ran `sel_dup_regroup`), proving the maintenance code ran, not a stale
   column.
3. `selecta-cli script` with `PREF_SET library.hide_duplicates on`, then `LIBRARY_SEARCH ""` -> 13 rows; the three
   best keys present, the four hidden keys absent; row A carries `copies=2`, B `copies=3`.
4. `LIBRARY_SEARCH "copies:<A local key>"` -> 2 rows with the filter on.
5. `LIBRARY_DUP_PICK <C hvsc key>` -> `ok best=<C hvsc key>`; search -> 13 rows, C shown as the hvsc key; `OPEN` of
   the C modland key reports `open_best_source=<C hvsc key>` (list copy = played copy).
6. `PREF_SET library.hide_duplicates off` -> `LIBRARY_SEARCH ""` -> 17 rows.
7. A further local file with B's md5 added and scanned -> `hidden=5`, search still 13 (bulk path maintained).

Every other behaviour is tested where it lives: rank macro and regroup in a db unit test, invariant per writer
(I1094b), UI badge and menu in the desktop/MUI component tests.

## 8. Rows

I1094a-h in the ledger under I1094, each with done criterion and track. I1094i (tier 2) only after Decision 1.

## Decisions for Johan

1. **Tier 2 (same song, different file or format) in v1?** Recommended: **no, tier 1 exact copies first**, tier 2
   as row I1094i after tier 1 ships. Trade-off: tier 1 never hides a wrong song; tier 2 is what S3btify's
   "best of two copies" (FLAC vs MP3) really means, but its fuzzy match hides remixes and same-named tunes.
2. **Default of Hide Duplicate Songs.** Recommended: **on**. Trade-off: with Modland + HVSC + local the same SID and
   mod show two or three times in All tracks; on cleans that up and OPEN already plays the best copy. Off is safer
   for users who want to see every file, but they get the noise by default.
3. **User-configurable source order** (for example "prefer HVSC over Modland") beyond the per-group pick?
   Recommended: **no in v1**, D-M3-14 order plus per-group "Use This Copy". Trade-off: a global order is one more
   setting and one more ranking input; the per-group pick covers the individual cases.
4. **Album, artist and composer song counts with hiding on.** Recommended: **count visible songs** (counts match
   what the list shows), done by making the I838 maintained counts dup-aware in a follow-up row. Trade-off: raw
   counts need no change but a 3-song album that shows 2 rows looks broken; dup-aware counts touch the maintained
   count tables and their I577 cost budget.
5. **Hide inside source pages** (Modland or HVSC browse)? Recommended: **no**, a source page shows everything that
   source holds. Trade-off: browsing Modland then shows songs you also have locally, but hiding there would make
   a source page incomplete and the per-source song count wrong.
