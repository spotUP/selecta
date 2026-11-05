---
date: 2026-11-05
author: spot (Opus design pass)
row: I1127b
parent: I1127 (9e3b0833 local banks, d5f59551 ledger)
status: ready for build
topic: Content-derived companion files (song bytes name the bank) for cached remote songs
---

# Content companions (I1127b)

## 1. Problem and what exists

Remote songs land in the library file cache at `<cache>/files/<source>/<path>` and are opened from there, so an
adapter's `src->path` directory lookup (I1127, eupmini `read_bank`, 98fmplayer `read_bank`/`load_banks`, mdxmini PDX,
kss `KSS_autoload_mbk`) only finds a bank when the cache fetched it next to the song. The cache fetches companions
only from `SEL_RP_COMPANIONS` (include/selecta/tags.def:61-63): `pattern>companion` rules over the base name,
expanded by `sel_companion_paths` (src/library/cache.c:362) and fetched by `sel_cache_fetch_song` (cache.c:441).
The bank names of eupmini (.fmb/.pmb, header 0x6E2/0x6EA), 98fmplayer (PPC/PVI/PZI from the driver's
`work.pcmname/pcmtype`), mdxmini (PDX name after the title's 0x0D 0x0A 0x1A, mdxfile.c:115-137) and kss/MoonBlaster
(.MBK name at MBM header 0x140, 8 chars, mbm2kss.c:155) live in the song bytes; no rule can express them.

Reused, not rebuilt:
- `sel_cache_fetch_song` stays the one place songs and companions are fetched (foreground fetch, LIBRARY_FETCH,
  prefetch, smart downloads all call it: fetch.c:39, fetch.c:513).
- `sel_companion_rules` (cache.c:410) is the pattern for walking the registry: same `registry_count/registry_get`
  pair, same PLUGIN_KIND_REPLAYER + PLUGIN_TAGS lookup. The registry pair is already handed to other threads
  (scan_cfg, engine.c:4941; sync_ctx, engine.c:7642), so reading it off the engine thread has precedent.
- `mark_companions` (cache.c:567) gets a second pass beside the rule pass, same exempt/count shape.
- `stats.companions` keeps counting every delivered companion; one new counter is the sentinel.
- The adapters' existing name readers are factored so the hook and `rp_open` share one name function per adapter
  (single source of truth: the hook may never disagree with what the adapter looks for).

## 2. Where the hook lives

A new replayer tag, next to COMPANIONS, in include/selecta/tags.def after CFG_APPLIED:

```c
/* CONTENT_COMPANIONS (I1127b): optional. Names companion files that the song bytes name (banks, sample sets), for the
 * library cache to fetch beside a remote song (sel_rp_content_companions_fn). SEL_RP_COMPANIONS covers names derived
 * from the file name; this covers names inside the file. */
SEL_TAG(RP, CONTENT_COMPANIONS, 0x1025, PTR)
```

The cache does not know which replayer will win the song, so it asks every replayer that declares the hook (as
`sel_companion_rules` joins every replayer's rules). Each hook answers only for its own format (extension and magic
check) and returns 0 otherwise. Results are joined in registry order, deduplicated case-insensitively, capped at
`SEL_CACHE_COMPANIONS_MAX` (8) together with the rule companions.

## 3. C signature, ownership, lifetime

In include/selecta/replayer.h (beside the other `sel_rp_*_fn` typedefs; builder greps `sel_rp_cfg_applied_fn` for
the exact header):

```c
#define SEL_RP_COMPANION_NAME_MAX 64 /* bytes incl. NUL of one name a CONTENT_COMPANIONS hook returns */

/* name: base name of the song (no directory), never NULL. data/size: the first bytes of the song, read-only.
 * data == NULL (size 0) is the claim probe: return 1 when `name` is a format this replayer would read names from,
 * else 0, without looking at any bytes. Otherwise write up to `max` NUL-terminated base names into out[] and return
 * how many. A short `size` (truncated head) means "no names", never a read past data + size. */
typedef unsigned (*sel_rp_content_companions_fn)(const char* name, const uint8_t* data, size_t size,
                                                 char out[][SEL_RP_COMPANION_NAME_MAX], unsigned max,
                                                 const sel_host* host);
```

Rules:
- `data` is borrowed for the call only, owned by the cache; the hook keeps no pointer into it.
- `out` is caller-owned stack storage; the hook writes names, the caller copies them before the next call.
- `host` is only for scratch memory (98fmplayer needs a temporary driver state); everything allocated is freed
  before return. No file or network I/O, no library calls, no globals written: the hook runs concurrently on the
  job thread (prefetch cache) and the engine thread (foreground cache), so it must be reentrant. Hence kss parses
  the MBM header itself and never calls `KSS_autoload_mbk` (static `mbkpath[]`).
- Names are base names spelled as the song writes them (extension added where the format implies one). The hook
  does the format's own trimming (NUL/space padding); the cache does the security check (section 7), once, for all.
- `data` holds `min(file size, SEL_CACHE_CONTENT_HEAD)` bytes, `SEL_CACHE_CONTENT_HEAD = 65536` (cache.h). Enough for
  every format: eup header < 0x800, MBM header 0x148, MDX title + name, FMP/PMD songs are 16-bit sized
  (`fmp_load(..., (uint16_t)size)`).

## 4. Which thread calls it

`sel_cache_fetch_song` calls the hook, so it runs where the fetch runs:
- Prefetch and smart downloads: the library job thread (`sel_fetch_job_run`, fetch.c:106, on `lib->pf_cache`).
- Foreground play of a remote key (`read_song`, engine.c:2027-2037) and `LIBRARY_FETCH` (engine.c:8319): the engine
  thread. On the desktop that IS the UI thread (research doc 2026-11-01-ui-thread-library-waits.md section 2a row
  "Fetch | LIBRARY_FETCH", section 2b row "Play a song"; I906g still lists "LIBRARY_FETCH, remote read_song" open).

Consequence: the hook itself is pure CPU on <= 64 KB (microseconds), so it adds no wait class. The bank downloads it
triggers extend the cold foreground fetch exactly as rule companions (TFMX smpl.*) already do today; the prefetch
normally fetched the next song and its banks on the job thread first, so a queued song is a cache hit. Moving the
foreground fetch off the UI thread stays I906g's job; this design adds no new UI-thread wait and must not add a
library request (`sel_library_request`) to the fetch path.

## 5. Wiring (how names reach the cache)

The rules travel as a string because they are re-collected per fetch. The hook needs the registry pair, which is
stable after plugin load, so it is set once:

- `sel_cache` (cache.h) gains `uint32_t (*registry_count)(void); const sel_tag* (*registry_get)(uint32_t);
  const sel_host* hook_host;`. NULL = no content companions (every existing caller keeps working).
- `sel_library_set_registry(sel_library*, count, get)` (db.h beside `sel_library_set_rules`, fetch.c) stores the pair
  on the library and copies it into `lib->cache` (cache_get) and `lib->pf_cache` (fetch.c:142) when those are made;
  `hook_host = lib->host`.
- The engine calls it once where it takes the library (`e->lib = cfg->library`, engine.c:10457).
- New in cache.c: `unsigned sel_content_companion_names(const sel_cache* c, const char* path, const uint8_t* data,
  size_t size, char out[][SEL_CACHE_PATH_MAX], unsigned max)`: walks the registry like `sel_companion_rules`, calls
  each hook (claim probe first, so a non-claimed song reads no bytes), sanitises (section 7), prefixes the song's
  directory, dedupes case-insensitively. One function, used by both the fetch and the trim.

## 6. Fetch and the src->path lookup

In `sel_cache_fetch_song`, after the rule companions and before return:
1. If any hook claims the base name: read the head of the cached song (`out`) into a stack-free buffer
   (`hook_host->mem_alloc`, 64 KB, freed at the end).
2. `n = sel_content_companion_names(...)`, minus names the rule pass already delivered.
3. For each name: try the spelling as written, then all-lower, then all-upper (skip a variant equal to one already
   tried), each through `sel_cache_fetch(c, source, variant, NULL, ...)`. First OK wins. If the winning variant is not
   the written spelling, `rename()` it to the written spelling inside the same directory (a case-only rename is fine
   on case-insensitive file systems). `stats.companions++` and new `stats.content_companions++` per delivered bank.
4. `SEL_CACHE_PENDING` from a bank returns PENDING like the rule loop; the next call re-reads the head (now cached),
   recomputes the same names and resumes.
5. Cancel (`*c->cancel`) stops the loop as in the rule loop.

The bank then sits at `<cache>/files/<source>/<dir of song>/<written name>`, the directory of `src->path`, so every
adapter's existing lookup finds it, including exact-case lookups that do not try case variants (libkss .MBK).

## 7. Failure modes and sanitising

Names read from song bytes are attacker-controlled (any Modland/zxart upload). The cache accepts a name only if:
- length 1..SEL_RP_COMPANION_NAME_MAX-1 after the hook's trim;
- every byte is 0x21..0x7E except it may hold inner spaces (0x20) but not leading/trailing ones;
- no `/`, `\`, `:`, no `..` anywhere, does not start with `.` (no hidden files, no `.part` collisions), does not end
  in `.part`;
- it is not the song's own base name (case-insensitive).
A rejected name is dropped silently (counted in `stats.content_rejected`, for the unit test). After joining with the
song's directory the path still goes through `sel_cache_path`, which refuses a path that climbs out (cache.c:55):
two independent checks. Non-ASCII (Shift-JIS) names are rejected; the local eup reader already does the same.

| Failure | Behaviour |
| --- | --- |
| Bank missing on every mirror (404) | Skipped like a missing rule companion; the song plays with default voices / no PCM. Costs up to 3 variants x mirrors requests, only on a cold fetch (a hit never re-requests a present bank; an absent one is re-tried on the next cold fetch of the song). |
| Header case differs from the server | Lower and upper variants; stored under the written spelling. Mixed-case server names that are neither (e.g. `FmTone2.fmb`) are not found: unverified whether Modland has any, noted. |
| Path traversal / absolute / hidden name | Rejected by the sanitiser, then by `sel_cache_path`. |
| Truncated or corrupt song head | Hook returns 0; nothing fetched. |
| More than 8 names | Capped; extra names dropped (no format needs more than 4). |
| Zipped source (I695d/I406a) | Out of scope: banks come only through plain mirror fetches, as rule companions. |

## 8. Trim exemption (mark_companions)

`mark_companions` today maps every exempt entry through `sel_companion_paths`. Add `mark_content_companions(c, w, rl,
out)`, called from `sel_cache_trim` right after it when `c->registry_get != NULL`:
- For each exempt entry (kept, liked, or the protected file): claim probe on its base name; only claimed entries are
  read (head, 64 KB). Kept songs that are not eup/mdx/fmp/pmd/mbm read no bytes.
- `sel_content_companion_names` (the same function the fetch uses), then mark every non-exempt entry in the same
  `<source>/<dir>` whose base name matches case-insensitively; `out->companion_exempt++` per entry.
- A bank shared by several songs is exempt while any exempt song names it, and trimmable by age otherwise (same as
  rule companions).
`smart_over_budget` (fetch.c:50) passes no keep/protect, so no entry is exempt and nothing is read.

## 9. The four adapters

Each adapter factors its name reader into one static function used by `rp_open` (local lookup) and the hook.

- eupmini (plugins/replayers/eupmini/adapter.c): `eup_bank_names(data, size, out)`: requires size >= 0x6F2 and the
  `.eup` extension; trims the 8-byte fields at 0x6E2 and 0x6EA at NUL/space; appends `.fmb` / `.pmb`. `read_bank`
  takes the trimmed name from it instead of re-trimming. Claim probe: extension `.eup` (case-insensitive).
- 98fmplayer (plugins/replayers/98fmplayer/adapter.c): the names exist only after the driver loads the song. The hook
  allocates a temporary state with `host->mem_alloc` (zeroed), runs the same load step as `rp_open` (`fmp_load` /
  `pmd_load` at adapter.c:99, plus whatever fills `work.pcmname/pcmtype`; builder reads `reset`/`load_banks` around
  adapter.c:253-310 to find the minimal call with no chip output), then `bank_names_from_work(&work, out)`, the
  function `load_banks` now uses: name + `bank_ext(pcmtype)`, skipping the slot `load_banks` skips (PPC in slot 3).
  Frees the state. Claim probe: the extensions the adapter registers for FMP and PMD. The drum ROM stays out (I1127c,
  proprietary).
- mdxmini (plugins/replayers/mdxmini/adapter.c): `mdx_pdx_name(data, size, out)`: the mdxfile.c:103-137 logic over
  the byte buffer with bounds checks (scan to 0x0D 0x0A 0x1A, then the NUL-terminated name, append `.pdx` when the
  name lacks it). Local open keeps mdxmini's own lookup (third_party, not touched); the hook only feeds the cache.
  Claim probe: `.mdx`.
- kss (plugins/replayers/kss/adapter.c): MBM only (claim probe `.mbm`). Names: `<stem>.MBK` (upstream search 1) and
  the header name at 0x140 (8 chars, stop at space or NUL, + `.MBK`) when size >= 0x148. Both through the hook (the
  stem rule could be a COMPANIONS rule, but one hook keeps kss's whole search order in one place). Local open keeps
  `KSS_autoload_mbk`.

## 10. Tests

One reachability test (product entry point, sentinel, spec numbers), the rest where the behaviour lives.

R1 `remote-eup-banks` (tests/net/remote_eup_banks.cmake, template tests/net/zxart_play.cmake + cache_fetch_shell.cmake):
- Mirror fixture: `tests/net/fixtures/fast.mirror.test/mods/Euphony/Aya/Mondschein/{Mondschein.eup,fmtone2.fmb,piano.pmb}`
  as git symlinks to tests/m3/fixtures/{eup_test.eup,fmtone2.fmb,piano.pmb} (no second copy of the binaries). If
  fixture_server.py refuses symlinks leaving its root (builder checks), stage copies in the test's WORK dir is NOT
  an option (the server root is fixed): then teach net-fixture-start to copy them, and say so in the row.
- `SELECTA_MIRRORS_MODLAND=http://fast.mirror.test/mods/`, empty `SELECTA_CACHE_DIR`, `SELECTA_PLUGIN_PATH`.
- Three `selecta-cli script` runs, same script shape (`@dump`, OPEN, PLAY, `@render`, STOP), same frame count:
  (a) local `tests/m3/fixtures/eup_test.eup` with banks beside it -> E_bank;
  (b) a copy alone in an empty dir -> E_none;
  (c) `OPEN modland:Euphony/Aya/Mondschein/Mondschein.eup` -> E_remote.
- Asserts: `${cache}/files/modland/Euphony/Aya/Mondschein/` holds the song and both banks under the header spelling;
  fixture log has exactly one successful GET per bank (the sentinel: only the hook can produce these requests,
  no rule maps `Mondschein.eup` to `fmtone2.fmb`); `E_remote >= EUP_BANK_RATIO (3) x E_none` (the I1127 bar) and
  `|E_remote - E_bank| <= 1% of E_bank` (with-bank level, not just "louder").
- Red proof: drop the CONTENT_COMPANIONS tag from eupmini -> no bank GETs, E_remote at E_none level -> FAIL. Record in
  the row.

U1 cache unit test (tests/m3/content_companions_test.c, registered like the neighbouring C tests in tests/CMakeLists.txt):
a fake registry with one hook returning fixed names; asserts the sanitiser table (`../x`, `/etc/x`, `a\b`, `c:x`,
`.hidden`, `x.part`, own name, 0x80 byte, 64-byte name all rejected, `content_rejected` counts them; `Bank 1.pmb`
accepted), case-insensitive dedupe with a rule companion, cap at 8, claim probe returning 0 reads no bytes.

U2 trim: extend tests/m3/cache_budget.sh (I167a block at lines 38-50) with a kept `.eup` whose banks are in the cache
and a budget of 0: `companion_exempt` counts both banks, both survive; a non-kept `.eup`'s banks are evicted.

U3 per adapter (the hook as a pure function, in each adapter's existing decode test or a tiny C test against the
module's exported tag): eup fixture -> `fmtone2.fmb`, `piano.pmb` (header spelling); fmp_test.ovi -> `PCM03.PVI`;
mdx_test.mdx -> 0 names (its PDX field is empty, mdx_test.mdx.SOURCE); a hand-made 0x148-byte MBM header -> `x.MBK`
and the header name. kss and mdx have no remote song fixture with a bank: unit level only, said in the row.

## 11. Build steps (sonnet, in order)

Each step builds and runs only its own test (RULES 6a), type-check/compile clean, commits by file name.

1. Tag and typedef. Files: include/selecta/tags.def (CONTENT_COMPANIONS 0x1025), include/selecta/replayer.h (or the
   header holding `sel_rp_cfg_applied_fn`: `SEL_RP_COMPANION_NAME_MAX`, `sel_rp_content_companions_fn`). Test: full
   build compiles (tag tables are generated from tags.def; grep for a tag-count assertion and update it).
2. Cache core. Files: src/library/cache.h (fields, `SEL_CACHE_CONTENT_HEAD`, stats `content_companions`,
   `content_rejected`, `sel_content_companion_names` prototype), src/library/cache.c (sanitiser, names function,
   fetch_song pass with case variants + rename, section 6). Test: U1.
3. Trim. Files: src/library/cache.c (`mark_content_companions`, call in `sel_cache_trim`). Test: U2.
4. Library and engine wiring. Files: src/library/db/db.h, src/library/db/db_internal.h, src/library/db/fetch.c
   (`sel_library_set_registry`, copy into cache and pf_cache, print `content_companions=` in the stats line at
   fetch.c:538), src/engine/engine.c (call at engine.c:10457). Test: cache-fetch-shell still passes (expects
   unchanged: no hook claims song.mod / mdat.song).
5. eupmini hook. Files: plugins/replayers/eupmini/adapter.c (factor `eup_bank_names`, hook, tag). Test: eup-decode
   (local, unchanged) + U3 eup case.
6. R1 reachability test. Files: tests/net/remote_eup_banks.cmake, tests/net/fixtures/fast.mirror.test/mods/Euphony/...
   (symlinks), tests/CMakeLists.txt (register under `NOT SEL_AMIGA`, FIXTURES_REQUIRED net-fixture, RESOURCE_LOCK
   net-fixture, PLUGINS dir). Test: R1 green, then the red proof from section 10, recorded in the row.
7. 98fmplayer hook. Files: plugins/replayers/98fmplayer/adapter.c (`bank_names_from_work`, temp-state hook). Test:
   fmp98-decode (unchanged) + U3 fmp case.
8. mdxmini hook. Files: plugins/replayers/mdxmini/adapter.c. Test: mdxmini-decode + U3 mdx case.
9. kss hook. Files: plugins/replayers/kss/adapter.c. Test: kss-decode + U3 MBM case.
10. Amiga (RULES 4a): the cache code is shared; build the Amiga target once, no rig run needed for a fetch-path
    change unless Johan wants one (rig 2 hang, vtcon-05). Record the exemption or the build in the row.
11. Ledger: tick I1127b with commits, close I1127(a), note (b) PMD .ppc fixture and (c) drum ROM stay open.

## 12. Decisions

Technical defaults taken (not Johan's): one hook for all formats, registry walk over all replayers, 64 KB head,
3 case variants, store under the header spelling, sanitiser by rejection (no rewriting of names).

Johan's call (product behaviour): a song whose bank is missing on the server plays with default voices/silent PCM
and no notice, as songs with a missing TFMX smpl.* do today. Alternative: a one-line "sample bank not found" note in
the song panel. Default kept until he says otherwise.
