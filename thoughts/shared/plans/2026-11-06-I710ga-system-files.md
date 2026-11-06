---
date: 2026-11-06
author: spot (Opus design pass)
row: I710ga
parent: I710g (Cybertracker C64 replayer)
status: approved
topic: Required system files for replayers (ROMs, disk images), Cybertracker disk image first
design_doc: thoughts/shared/plans/2026-10-22-architecture-design.md sections 4 (replayer SDK), 10 (tag ABI), 9.3 (cache/fetch), 13 (reachability)
---

# Required system files (I710ga)

Johan's decisions in ledger row I710ga are final; this plan implements them and does not re-open them. Product
choices the row does not settle are listed in section 8 as open questions with a recommended answer.

## Checklist

- [x] I710ga1 ABI (09fb7310): `RP_SYSFILE` tags in tags.def + replayer.h contract (section 2)
- [x] I710ga2 Store: `src/library/sysfile.c` resolve / verify / install, zip unpack (section 3) Ledger: I710ga2 done 8c112bc2
- [x] I710ga3 Download: URL list, override seam, background job on the library job thread (section 4)
- [x] I710ga4 Engine: typed `system_file_missing` reason, song state, auto-download, reopen on land, commands (section 5)
- [ ] I710ga5 Cybertracker adapter + glue: declare the file, read the path from the host (section 6)
- [ ] I710ga6 Desktop UI: toast, Now Playing note line + button, fallback dialog (section 7)
- [ ] I710ga7 Reachability test through the engine top level (section 9)
- [ ] I710ga8 hippoplayer.se mirror request (sent to that session, not built here) (section 4.4)

## 1. Problem and what exists

`bbmp_cybertracker` runs the user's CyberTracker 1.01 disk image on an emulated C64. Today the image is found by
`third_party/bbmp_cybertracker/bcybertracker_glue.cpp:pick_disk()` (SELECTA_CYBERTRACKER_DISK, BBMP_CYBERTRACKER_DISK,
then `$XDG_CONFIG_HOME/selecta` or `~/.config/selecta` + `/cybertracker101.d64`). Without it `rp_open` returns NULL and
the engine reports a generic `open_failed` (`src/engine/engine.c:2745`). Nothing tells the user what is missing and
nothing fetches it.

Root-cause defect found on the way: the glue computes its own config directory from XDG/HOME while the engine has
`e->config_dir` (`--config-dir`, answered to replayers as `config.dir` by `rp_config_get`, engine.c:2284, used by the
sid ROMs, MPS-1c). Two definitions of one concept (RULES 2). This plan makes the host the only source of the path.

Searched and reused (no new mechanism where one exists):

| Need | Reused | Where |
| --- | --- | --- |
| Descriptor declaration | nested LIST tag like `RP_CONFIG` | `include/selecta/tags.def:60` |
| Plugin learns a host path | `host->config_get` answered by `rp_config_get`, like `config.dir` | engine.c:2272-2290 |
| Copy a user file into the config dir with a check | `sel_uim_rom_install` pattern (size check, copy, reason text) | `src/uimodel/rom.c` |
| HTTP transport + test seam | `sel_net.get_file` (SELECTA_NET_MAP / SELECTA_NET_ALLOW) | `include/selecta/net.h` |
| sha256 | `sel_sha256` | `src/library/entropy.h:30` |
| Zip unpack | `tinfl_decompress_mem_to_mem` + central directory parse | `src/library/zip_member.c` |
| Background thread | library job thread that runs `sel_fetch_job_run` | `src/library/db/fetch.c:107-165` |
| Note line in Now Playing | `sel_uim_np_note` + `bank_missing` song flag | `src/uimodel/actions.c:165`, `include/selecta/engine.h:148` |
| Toast | `desk_toast` | `plugins/ui/desktop/desktop_ui.c:662` |
| File chooser + test answer | `settings_rom_pick` / `rom_wait` / `--dialog-answer` | desktop_ui.c:10072-10100, 14815 |
| Local HTTP fixture | `tests/net/fixture_server.py`, `tests/net/fixtures/<host>/` | tests/CMakeLists.txt:1636 |
| Fetch real non-committable bytes by sha | `tests/m3/fetch_fixture.sh` (git-ignored cache, exit 77 skip) | tests/m3/cybertracker_decode.sh:19 |

Not reused, with reason: `sel_mirrors_fetch_opts` (`src/library/net/mirrors.h`) ranks mirrors by time to first byte and
builds `base + path`; system-file URLs are full URLs (CSDb `getinternalfile.php/63969/...`) and the order is fixed
(CSDb first, then our mirror). The per-URL loop is a dozen lines over `sel_net.get_file`; ranking would break the
decided order.

## 2. Replayer ABI contract (I710ga1; extends design doc section 10 and section 4)

New tags in the RP block of `include/selecta/tags.def` (next free values after `CONTENT_COMPANIONS` 0x1025):

```
/* SYSFILE (I710ga): optional, repeatable. One required system file (ROM, disk image) the replayer cannot play without.
 * Value: a tag list of SYSFILE_* below. The host checks, stores and fetches it; see replayer.h. */
SEL_TAG(RP, SYSFILE,          0x1026, LIST)
SEL_TAG(RP, SYSFILE_NAME,     0x1027, STR)  /* stored base name in the config dir, "cybertracker101.d64"; [A-Za-z0-9._-] only */
SEL_TAG(RP, SYSFILE_LABEL,    0x1028, STR)  /* what the UI calls it, "CyberTracker disk image" */
SEL_TAG(RP, SYSFILE_HINT,     0x1029, STR)  /* one or two sentences for the dialog: where a user finds it */
SEL_TAG(RP, SYSFILE_SHA256,   0x102A, STR)  /* ';'-separated lowercase hex sha256 of the accepted files (the identity) */
SEL_TAG(RP, SYSFILE_URLS,     0x102B, STR)  /* ';'-separated full URLs in try order; a URL may serve the file or a zip holding it */
SEL_TAG(RP, SYSFILE_SIZE,     0x102C, INT)  /* exact byte size, 0 = any; cheap pre-check before hashing */
```

Contract text for `include/selecta/replayer.h` (beside RP_CONTENT_COMPANIONS, same style):

```
/* RP_SYSFILE (I710ga): optional, one entry per file. A required system file is identified by its sha256, never by
 * its URL or name: any bytes whose sha256 is in SYSFILE_SHA256 are that file. The host stores the checked file as
 * <config dir>/<SYSFILE_NAME>, and before RP_OPEN of a song this replayer claimed it resolves every declared file;
 * when one is missing or fails the check it does not call RP_OPEN, reports the typed reason system_file_missing and
 * fetches the file in the background from SYSFILE_URLS (an I1076 player-pack manifest may add or replace URLs).
 * The replayer reads the stored path with host->config_get(host, "sysfile.<SYSFILE_NAME>", NULL): the full path of
 * the checked file, or NULL when the host has none (an older host, or a host without the contract: the replayer
 * then keeps its own lookup and its RP_OPEN still refuses without the file). The path is valid for the session. */
```

Versioning and compatibility (design doc section 10 rules):
- Tags are appended to the RP block; unknown tags are skipped by older hosts, so an older host loads the plugin and
  behaves as today (open fails, `open_failed`). A new host with an older plugin sees no SYSFILE and changes nothing.
- No `SEL_API_VERSION` bump and no `sel_host` field: the path travels through the existing `config_get`.
- Inside the SYSFILE list, unknown SYSFILE_* tags are skipped, so fields can be appended later (e.g. a member name).
- NAME, SHA256 and LABEL are required; an entry missing one is ignored by the registry with a WARN log line naming the
  plugin (the registry tag lint test asserts it).
- The registry exposes the parsed declarations once per plugin: `sel_registry_sysfiles(const sel_tag* hooks,
  sel_sysfile_decl* out, unsigned max)` in `include/selecta/registry.h` (host-internal shape, not plugin ABI).

## 3. Store (I710ga2)

New `src/library/sysfile.c` + `src/library/sysfile.h` (library-internal; the engine and the fetch job call it). Shape
copied from `src/uimodel/rom.c`, plus the sha256 identity:

- `int sel_sysfile_resolve(const char* config_dir, const sel_sysfile_decl* d, char* path, size_t cap)`: 1 when
  `<config_dir>/<name>` exists, has SIZE (when set) and its sha256 is accepted. The verdict is cached per process by
  (name, size, mtime), so a session hashes the 170 KB image once.
- `int sel_sysfile_install(const char* config_dir, const sel_sysfile_decl* d, const char* src, char* err, size_t cap)`:
  `src` is the file itself or a zip. A zip (PK\3\4 magic) is scanned member by member; the first member whose bytes
  match an accepted sha256 is written. Writes `<name>.part` then renames (no torn file for a concurrent open).
  0 and a one-line reason ("No file in this zip matches the CyberTracker disk image", "Not the CyberTracker disk
  image") otherwise; nothing is written. Creates the config dir with the existing `config_mkdir` (engine.h:367).
- Zip: extend `src/library/zip_member.c` with `sel_zip_local_each(const char* zip_path, cb, ctx)` that walks the
  central directory of a local zip and inflates each member up to `SEL_ZIP_MEMBER_MAX` through the same tinfl call
  `sel_zip_member_fetch` uses. One parser, two callers.
- Store location: the engine's config dir (`e->config_dir`), the directory `config.dir` already names, so all future
  sessions use it (decision 4). The whole checked .d64 is stored, never the zip.

Unit test `tests/library/sysfile_store_test.c`: synthetic 1 KB file with a known sha256; resolve on absent / wrong
bytes / right bytes; install from raw file, from a stored zip and a deflated zip (built in the test with the tinfl
counterpart already vendored, or a checked-in 300-byte synthetic zip under tests/library/fixtures), from a zip with no
match (err text, nothing written); SIZE pre-check moves the verdict.

## 4. Background download (I710ga3)

### 4.1 URL list and the I1076 seam

`unsigned sel_sysfile_urls(const sel_sysfile_decl* d, char urls[][SEL_SYSFILE_URL_MAX], unsigned max)` in
`src/library/sysfile_fetch.c` returns the try order. Built-in order: the plugin's SYSFILE_URLS (CSDb first, then
`https://hippoplayer.se/sysfiles/...`). The seam for I1076 (not built): a process-wide override table

```
typedef struct sel_sysfile_override { const char* name; const char* urls; int replace; } /* replace 0: prepend */
void sel_sysfile_set_overrides(const sel_sysfile_override* list, unsigned n);  /* called by the I1076 manifest loader */
```

consulted first by `sel_sysfile_urls`. Today only the test seam fills it: env `SELECTA_SYSFILE_URLS_<NAME>` (NAME
upper-cased, '.' and '-' as '_'), ';'-separated, replace semantics, the same style as `SELECTA_MIRRORS_<SOURCE>`.
The manifest may change URLs only, never the accepted sha256 list (identity stays in the build; see Q3).

### 4.2 The fetch

`int sel_sysfile_fetch(const sel_host* host, const char* config_dir, const sel_sysfile_decl* d, sel_sysfile_res* res)`:
for each URL in order, `net->get_file(host, url, "<config>/<name>.dl")`; a 200 body goes to `sel_sysfile_install`;
the first install that succeeds ends the loop. Result codes:
`SEL_SYSF_OK`, `SEL_SYSF_ALL_FAILED` (every URL failed on transport or status), `SEL_SYSF_NO_MATCH` (at least one
body arrived and none matched an accepted sha256). `res` counts `tries`, `requests`, `rejected`, and the URL that
served. A NULL `host->net` (no network build) returns ALL_FAILED with 0 tries. Temp `.dl` file removed on every path.

### 4.3 Thread

Runs on the library job thread (job.c, the thread `sel_fetch_job_run` uses) as a second job kind:
`sel_library_sysfile_request(lib, decl)` queues one request per name (a second request for a queued or running name
is a no-op, counted); `sel_library_sysfile_poll(lib, name)` returns NONE / RUNNING / OK / ALL_FAILED / NO_MATCH. The
engine polls it on its pump tick (no callback across threads). A sysfile request runs before smart prefetch, after a
pending user prefetch, and never blocks the UI thread (I782 no-wait rule).

Unit test `tests/net/sysfile_fetch_test` (cmake script on the net-fixture, `RESOURCE_LOCK net-fixture`): fixture hosts
`csdb.test` and `mirror.sysfile.test` mapped by SELECTA_NET_MAP; cases (a) first URL 404, second serves the synthetic
file: OK, tries=2, served=second; (b) both serve wrong bytes: NO_MATCH, rejected=2, nothing stored; (c) both 404:
ALL_FAILED; (d) first serves a zip holding the file: OK, tries=1; (e) override env replaces the list: the overriding
URL is the only one requested (fixture request log). Never the real network.

### 4.4 hippoplayer.se mirror (I710ga8: a request to that session, not an edit here)

Repo and player pack stay blob-free (Johan 2026-11-06). Request text to send to the hippoplayer.se session (per its
`thoughts/shared/server-changes-for-agents.md`):

```
Selecta I710ga: please host one static file for the system-file fallback mirror.
Path: GET https://hippoplayer.se/sysfiles/cybertracker101.d64 (plain file, Content-Type application/octet-stream,
also over plain http for non-TLS clients; no auth; not under /api/mirror since it is not a source).
File: the .d64 inside https://csdb.dk/getinternalfile.php/63969/cybertracker1_01-d64.zip
(zip sha256 cdecc06385ebb5cd1d941889cae8c1ecf33e72965116db652a4e43c50d4040fe).
sha256 of the .d64: <filled in by I710ga5 from the fetched fixture>.
Selecta checks the sha256, so the path may move later as long as the old one 404s or keeps the bytes.
Reply with the live URL and the sha256 you see on the server.
```

The path is a proposal; the hippoplayer.se session picks the final one and its answer goes into the adapter URLs.

## 5. Engine (I710ga4; extends design doc section 4 open path)

In the open path (`engine.c` near 2735, after the replayer is chosen and before `open(&src, ...)`):

1. `sel_registry_sysfiles(hooks, ...)`; for each declaration `sel_sysfile_resolve(e->config_dir, ...)`.
2. All resolved: store the paths in the slot (`sl->sysfile_path[]`) and open as today. `rp_config_get` answers
   `sysfile.<name>` with that path (beside `config.dir`, engine.c:2284; also in `pl_config_get`).
3. One missing: no RP_OPEN. `sel_engine_report_error(e, "system_file_missing", "<plugin>:<name>")` (a new typed
   reason string beside `open_failed`; the ERROR event keeps its "ss?" shape). The song state carries
   `sysfile_state` (new field in `sel_song_state`, engine.h beside `bank_missing:148`) with `SEL_SYSF_ST_NONE`,
   `FETCHING`, `FAILED` (every URL failed or no match), and `sysfile_label` (the declaration's LABEL, owned by the
   registry). The open song stays the current request (the panel shows its title) but does not play.
4. Not yet requested this session: `sel_library_sysfile_request` (state FETCHING). Already FAILED this session for this
   name: no new request (decision 5: no re-prompt per song); state FAILED at once.
5. Pump tick: poll; on OK, if the waiting request is still the current one, re-OPEN it (same subsong, from start) and
   raise event `SYSFILE_READY "s"` (label) in `events.def` for the toast; on ALL_FAILED / NO_MATCH set FAILED and raise
   `SYSFILE_FAILED "ss"` (label, reason). The user moved on to another song: the file is still stored, the event still
   fires, nothing re-opens.
6. Commands in `commands.def`: `SYSFILE_INSTALL <name> <path>` (Pick file: runs `sel_sysfile_install` on the command
   thread; success re-opens like step 5; failure raises `SYSFILE_FAILED` with the install reason) and
   `SYSFILE_RETRY <name>` (clears FAILED for that name and re-runs step 4).
7. Stats (`sel_engine_stats`, `--stats` line `sysfile`): `sysfile_missing_reports`, `sysfile_fetches`,
   `sysfile_installed`, `sysfile_reopens`, `sysfile_state`. These are the reachability sentinels.

Unit tests: `tests/engine/sysfile_engine_test.c` with a test-only replayer module (`tests/plugins/sysfile_probe/`,
declares one SYSFILE with a synthetic sha256, renders a sine once its file resolves; the test module builds only with
tests). Cases: missing -> reason + no RP_OPEN call (call count 0); request once for two songs of the same plugin; FAILED
does not re-request; RETRY re-requests; INSTALL with a wrong file keeps FAILED and the reason; `config_get` path equals
the stored file.

## 6. Cybertracker adapter (I710ga5)

- `plugins/replayers/bbmp_cybertracker/adapter.c`: add the SYSFILE list (NAME `cybertracker101.d64`, LABEL
  `CyberTracker disk image`, HINT "CyberTracker 1.01 by Cyberbrain is on CSDb as cybertracker1_01-d64.zip. Pick the
  .d64 or the zip.", SHA256 of the d64 (computed from the fixture `fetch_fixture.sh` already fetches; also accept any
  other known-good dump Johan names, Q4), SIZE 174848 if the fetched image has that size, URLS
  `https://csdb.dk/getinternalfile.php/63969/cybertracker1_01-d64.zip;<hippoplayer.se URL from I710ga8>`).
- `rp_open` reads `host->config_get(host, "sysfile.cybertracker101.d64", NULL)` and passes the path to the glue through
  a new `disk_path` argument of `bbcybertracker_render` (NULL: the glue's own order). `pick_disk()` keeps
  SELECTA_CYBERTRACKER_DISK / BBMP_CYBERTRACKER_DISK first (developer override, the decode test uses it), then the
  host path, and drops its XDG/HOME directory guess (the defect in section 1).
- Update the adapter header comment and `tests/m3/cybertracker_decode.sh` ("without the disk image the song is not
  claimed" becomes "reports system_file_missing").
- Platform: Mac desktop host build. Amiga and web exempt (RULES 4a): the plugin is GPL and ships only in the GPL host
  build (I1075/I1076 player pack), which has no Amiga or web target; the engine side (sections 3-5) is portable C99
  and builds on the Amiga, where no plugin declares a SYSFILE yet. No rig run for this row set.

## 7. Desktop UI (I710ga6; Mac desktop only)

Rule 1: the UI renders state and sends commands; all decisions above are the engine's.

- Toast: on `SYSFILE_READY` the desktop calls `desk_toast(d, "<label> downloaded")` ("CyberTracker disk image
  downloaded", decision 3). Sentinel `sysfile_toasts`.
- Note line: `sel_uim_np_note` (actions.c:165) returns `"<label> needed"` when `sysfile_state == FAILED`
  ("CyberTracker disk image needed", decision 5); the I1127c bank note keeps its place when both apply (Q2 on
  order). The note line gains one button, `Get File...` label TBD by Q1, drawn by both Now Playing panels through the
  same uimodel helper (`sel_uim_np_note_action(song)`: NULL or the button text), hit-tested like the other np buttons;
  click opens the dialog. Sentinel `np_note_buttons`.
- Dialog (fallback only, decision 3): opens by itself once when the state turns FAILED for the current song, and from
  the note button. Text: label + reason ("Could not download the CyberTracker disk image" or "The downloaded file is
  not the CyberTracker disk image") + HINT. Buttons: `Pick File...` (reuses `desk_sdl_open_file_dialog` and the
  `rom_wait` drop pattern as `sysfile_wait`; `--dialog-answer` answers it in tests; sends `SYSFILE_INSTALL`), `Retry
  Download` (sends `SYSFILE_RETRY`), `Cancel` (closes; the note line stays, no re-prompt for the session). Uses the
  existing confirm-dialog card (`dlg_open`, DUI-P4-18) with a new id; sentinels `dlg_opens` / `dlg_cancels` already
  exist, plus `sysfile_picks`, `sysfile_retries`.
- Dump line for tests (`--ui desktop` dump): `sysfile state=<n> note="<text>" toasts=<n> dlg=<id>`.
- MUI and text UI: exempt for this row set (Amiga has no SYSFILE plugin, see section 6). The text UI shows the note
  through `sel_uim_np_note` for free; no dialog there.

Test `tests/net/sysfile_desktop.cmake` (`-DUI=desktop`, mem backend, fixed clock, net-fixture): (a) both URLs 404 ->
note `CyberTracker`-free synthetic label `Test system file needed`, dlg opened once; `--dialog-answer` to the synthetic
file -> playback starts, toast count unchanged (no download); (b) mirror serves -> toast text `... downloaded`, no
dialog, playback. Uses the sysfile_probe module, so it never needs the real image.

## 8. Open questions for Johan (recommended answer first)

Answered 2026-11-06: Q1 `Get File...`, Q2 show nothing (toast reports), Q4 CSDb image only, Q5 once per session for the current song (Johan, AskUserQuestion). Q3 No and Q6 skip 77 taken as the recommended defaults (implementation choices, stated in chat).

- Q1 Note-line button label: **`Get File...`** (recommended; generic for later ROM players) vs `Choose Disk Image...`
  (names the thing, label then comes from the plugin) vs `Fix...`.
- Q2 Note while the silent download runs (first play, before it lands): **show nothing** (recommended; matches
  "silent", the toast reports the result) vs `Downloading CyberTracker disk image` in the note line.
- Q3 May the I1076 manifest also add accepted sha256 values (e.g. a second known-good dump) without a build?
  **No** (recommended: identity stays in the reviewed build; URLs are the only thing that rots) vs yes.
- Q4 Accept more than the CSDb 1.01 d64 (other dumps of 1.01 with different bytes)? **Only the CSDb image for now**
  (recommended; add a sha when a user reports a working other dump) vs collect known dumps up front.
- Q5 Dialog auto-open on FAILED: **once per session for the current song** (recommended, follows decision 5) vs
  never auto-open (note line only).
- Q6 The `.ct` leg of the reachability test needs the real image, which the repo must not hold: **skip with exit 77
  when `fetch_fixture.sh` cannot supply it** (recommended; same as cybertracker-decode today; the synthetic-module
  leg always runs) vs require it on Johan's machine (test fails without the cache).

## 9. Reachability (I710ga7; design doc section 13)

`tests/net/sysfile_cybertracker.cmake`, registered beside `remote-eup-missing-bank-net` (FIXTURES_REQUIRED net-fixture,
RESOURCE_LOCK net-fixture, SKIP_RETURN_CODE 77):

1. Prep (not the engine): `fetch_fixture.sh` supplies the CSDb zip from the git-ignored cache (exit 77 when absent and
   offline). The test copies it into the fixture server's served tree at run time (work dir, never committed) as
   `csdb.test/getinternalfile.php/63969/cybertracker1_01-d64.zip`; `SELECTA_SYSFILE_URLS_CYBERTRACKER101_D64` points
   the list at `http://csdb.test/...;http://mirror.sysfile.test/missing` so the first URL is the fixture and every
   request stays on 127.0.0.1 (SELECTA_NET_ALLOW).
2. `selecta --output null --config-dir <empty> --stats` plays a real `.ct` song (fixture from
   bbmp_cybertracker.SOURCE) through the top-level entry point.
3. Asserts: an ERROR `system_file_missing bbmp_cybertracker:cybertracker101.d64` line; `sysfile` stats line with
   `missing_reports=1 fetches=1 installed=1 reopens=1` (the sentinel: the download job ran and the engine re-opened);
   the fixture request log shows exactly one GET of the zip; `<config>/cybertracker101.d64` exists and its sha256 is
   the declared one; the replayer is `bbmp_cybertracker` and rendered frames > 0 with non-silent energy (same floor
   as cybertracker_decode.sh).
4. Second run with the same config dir: `missing_reports=0 fetches=0` (store reused by future sessions, decision 4).
5. Store-call leg (row text "supplies image via the store call"): fresh config dir, URL list overridden to two 404s,
   `SYSFILE_INSTALL` with the cached zip through `--input`; asserts state FAILED before, playback after, installed=1.

Red proofs (each must fail, then pass): drop the re-open in step 5 of section 5 (reopens=0, no frames); accept any
sha (wrong-bytes case in the fetch test passes rejected=0 -> fails); skip the store resolve (second run fetches again).

Knobs and exemptions (every declared knob moves output or is exempt):

| Knob | Moves | Proven in |
| --- | --- | --- |
| SYSFILE_NAME | stored file name, config_get key | store test, reachability step 3 |
| SYSFILE_SHA256 | accept / reject | store test, fetch test (b) |
| SYSFILE_URLS order | which URL serves, tries | fetch test (a) |
| SYSFILE_SIZE | pre-check verdict | store test |
| SYSFILE_LABEL | toast, note, dialog text | desktop test |
| SYSFILE_HINT | dialog text | desktop test dump of dialog text |
| override seam (env today) | URL list | fetch test (e) |
| I1076 manifest loader | exempt: not built (seam only, I1076 row) | - |

## 10. Build rows (sonnet-sized)

| Row | Files | Test | Section |
| --- | --- | --- | --- |
| I710ga1 | include/selecta/tags.def, include/selecta/replayer.h, include/selecta/registry.h, src/engine/registry.c | registry tag lint + `tests/engine/sysfile_tags_test.c` | 2 |
| I710ga2 | src/library/sysfile.c/.h (new), src/library/zip_member.c/.h, src/library/CMakeLists.txt | `tests/library/sysfile_store_test.c` | 3 |
| I710ga3 | src/library/sysfile_fetch.c (new), src/library/db/fetch.c + job.c (job kind), tests/net/fixtures | `tests/net/sysfile_fetch.cmake` | 4 |
| I710ga4 | src/engine/engine.c, include/selecta/engine.h, commands.def, events.def, tests/plugins/sysfile_probe | `tests/engine/sysfile_engine_test.c` | 5 |
| I710ga5 | plugins/replayers/bbmp_cybertracker/adapter.c, third_party/bbmp_cybertracker/bcybertracker_glue.cpp, tests/m3/cybertracker_decode.sh | cybertracker-decode | 6 |
| I710ga6 | src/uimodel/actions.c, include/selecta/uimodel.h, plugins/ui/desktop/desktop_ui.c | `tests/net/sysfile_desktop.cmake` | 7 |
| I710ga7 | tests/net/sysfile_cybertracker.cmake, tests/CMakeLists.txt | itself (reachability) | 9 |
| I710ga8 | none in this repo (message to hippoplayer.se session) | live URL answers with matching sha256 | 4.4 |

Order: 1, 2, 3, 4 (needs 1-3), 5 (needs 4 and the d64 sha from the fixture), 6 (needs 4), 7 last. I710ga8 can be sent
at once; its URL is filled into the adapter in I710ga5 or a follow-up edit when the answer arrives.
