---
date: 2026-10-22
topic: DEViLBOX ratings/genre API and UpRough UADE-on-Amiga backend
status: research, read-only
---

# Summary

- DEViLBOX has a live REST API at `https://devilbox.uprough.net/api` (default in `src/lib/apiBase.ts:26`; overridable by `VITE_API_URL`). Store: SQLite `server/devilbox.db` via `server/src/db/database.ts`.
- "Voting" = 1-5 star ratings, `PUT /api/ratings` (JWT required), batch read `GET /api/ratings/batch` (auth optional). Items are identified by **source + item_key**, where source is `modland` or `hvsc` and item_key is the Modland `full_path` (e.g. `pub/modules/...`) or the HVSC path. Not a hash.
- Genre is a separate API: `GET /api/genre/:key`, keyed by the **uade songdb key** (12 hex: xxh32 of first 256 KB + size&0xFFFF). No auth. Same key is used by `GET /api/songdb/lookup?hash=`. Comment in genre.ts says it is shared with HippoPlayer.
- Browsing endpoints a player can reuse: `/api/modland/{search,list,formats,download}`, `/api/hvsc/{browse,search,featured,download}`. No artist/collection endpoint beyond `author`/`format` filters on modland search. Search results embed `avg_rating`, `vote_count`.
- UpRough UADE: UADE's own 68k `score.s` host + the real EaglePlayer/DeliTracker replayer binaries run natively on the Amiga (no UAE, no emulation). The replayers drive Paula themselves, no mixing. Single-song playback is proven on a stock A500 for 40 formats. Musicdisk (multi-song switching) had a live crash as of 2026-09-21.
- The README in `uprough-uade` is about LVLLVL and is irrelevant; the real UADE docs are in `thoughts/` (listed in B).

# A. DEViLBOX API

Root `/Users/spot/Code/DEViLBOX`. Routers mounted in `server/src/index.ts:81-99`. JSON body limit 10 MB (`index.ts:61`).

## A1. Ratings (voting)  `server/src/routes/ratings.ts`

| Method path | Auth | Request | Response |
|---|---|---|---|
| `PUT /api/ratings` (ratings.ts:49) | `Authorization: Bearer <jwt>` (`authenticateToken`) | JSON `{source: "modland"\|"hvsc", itemKey: string, rating: int 1-5}` | `{ok:true, avg, count, userRating}`; 400 on validation, 401 no token |
| `DELETE /api/ratings` (:83) | JWT | JSON body `{source, itemKey}` | `{ok:true, avg, count, userRating:null}` |
| `GET /api/ratings/batch?source=modland&keys=a,b,c` (:115) | optional JWT | keys comma-separated, max 200 | `{ratings:{[key]:{avg,count,userRating?}}}` (userRating only when authed) |

Caveat: keys are split on `,`, so a path containing a comma cannot be queried (unconfirmed whether any exist).

Store: table `module_ratings` (`server/src/db/database.ts:198-212`): `user_id, source, item_key, rating CHECK 1..5, created_at, updated_at`, UNIQUE(user_id, source, item_key), FK to `users`. One vote per user per item, upsert.

Auth (`server/src/routes/auth.ts`): `POST /api/auth/signup` `{username (3-20), password}` -> 201 `{token, user:{id,username,createdAt}}` (:18-70); `POST /api/auth/login` (:83) returns `{token, user}`; `GET /api/auth/me` (:127) is a stub. Rate-limited (`index.ts:70`). A player must sign up/log in to vote; reading needs nothing.

Client: `src/lib/ratingsApi.ts` - `setRating` fetch at :39 (headers `getAuthHeaders()`), `removeRating` :59, `batchGetRatings` :86. UI callers: `src/components/dj/DJModlandBrowser.tsx:39-42` (import), setRating call at :276, batch merge :213; `src/components/dialogs/FilePreviewPanel.tsx:33`.

## A2. Module identification

- Ratings / browsing: Modland `full_path` or HVSC path string (modland search joins ratings on `r.item_key = f.full_path`, modland.ts:~100-125; hvsc.ts:175-201 does the same by path). Same file on another mirror under another path is a different item.
- Genre / songdb: uade key = `hex8(xxh32(bytes[0:256K])) + hex4(len & 0xFFFF)`; client implementation `computeUadeKey` in `src/lib/moduleGenre.ts:75` (uses decompressed bytes). Server comment: `database.ts:50-51`.
- Modland also has content hashes: `POST /api/modland/lookup-hash` (modland.ts:194; client `modlandApi.ts:318`), `/samples/:song_id`, `/pattern-matches/:pattern_hash`, `/hash-stats` (request/response shape not read; unconfirmed).

## A3. Genre / metadata

- `GET /api/genre/:key?name=<filename>&size=<bytes>[&md5=<32hex>]` (`server/src/routes/genre.ts:19`). No auth; rate limiter at `index.ts:78`. 200 `{key, genre, source, modarchiveId}`; genre non-null only when source = `modarchive` (format like "Electronic - Ambient"). 400 bad params, 503 not configured / queue full / monthly ModArchive budget used, 502 upstream failure. Server asks ModArchive by filename on first sight and caches in `module_genres` (`database.ts:52-62`, columns key, modarchive_id, md5, genre, source, checked_at; source values modarchive|untagged|miss|ambiguous|unsupported). ModArchive API key stays on the server (`services/modarchiveGenre.ts`).
  Client: `src/lib/moduleGenre.ts:97` (`lookupModuleGenre`, non-throwing).
- `GET /api/songdb/lookup?hash=<12hex>` (`routes/songdb.ts:17`) returns the uade songdb record (shape from `lookupHash` in `services/songdbIndexer`, not read); `GET /api/songdb/status`. Client `src/lib/songdb.ts:55`. Tables `songdb_lengths/modinfos/metadata` (`database.ts:137-160`).

## A4. Browsing endpoints reusable by a player

- Modland (`routes/modland.ts`, client `src/lib/modlandApi.ts`): `GET /status` (:30), `POST /reindex` (:42), `GET /formats` (:58), `GET /search?q&format&author&limit(<=100)&offset` (:70; response `{results, limit, offset, query}`; rows have `id, format, author, filename, full_path, extension, avg_rating, vote_count`; FTS ordering boosted by rating), `GET /list?dir=` (:155; `{files:[{full_path,filename,extension}]}`, immediate children), `GET /download?path=pub/modules/...` (:175; proxies and caches `https://ftp.modland.com/<path>`).
- HVSC (`routes/hvsc.ts`): `GET /stats` (hardcoded), `/featured` (hardcoded list), `/browse?path=`, `/search?q&limit&offset` (response `{results,total}`, with `avg_rating`/`vote_count`), `/download?path=`. Client `src/lib/hvscApi.ts:48,75,103,118,133`.
- Not found: any genre-list / by-genre browse endpoint, artist page, or collection endpoint. Genre is lookup-only per module. `/api/playlists` and `/api/djsets` exist (shared playlists, DJ sets; not read, unconfirmed shape).

## A5. Deployment

`CADDY_DEPLOYMENT.md:14,101`: `devilbox.uprough.net` fronts Docker frontend on :8080; backend container `devilbox-backend` serves `/api` on :3001 (`/api/health`, :246). DB path in container `/data/db/devilbox.db` (:408). CORS configured at `index.ts:48` (origin allow-list not read; check before calling from a non-web native client - CORS does not bind native apps).

# B. UpRough UADE backend

Root `/Users/spot/Code/UpRough/uprough-uade` (git repo `spotUP/up-rough-demo-system`, checked-out branch `feature/uade-music-backend`, HEAD ba3f76910, 2026-09-21). `Up_Rough_Demo_System` appears to be the same project tree (same file listing; also contains `uade-3.05.tar.bz2`, `tools/uade_*.py`, `Makefile.amiga*`); not separately verified whether it is a copy or a symlink.

## B1. What it is

`amiga/uade_glue.s:1-9`: runs UADE's own sound core `amiga/vendor/uade-3.05/amigasrc/score/score.s` (4323 lines, vendored, edits marked `*--- PORT:` and gated `ifd UADE_NATIVE`) on bare metal inside the demo runtime. UADE's 68k emulator is redundant because the replayers (EaglePlayer / DeliTracker binaries in `amiga/vendor/uade-3.05/players/`, 48 files vendored) are Amiga code and score.s is already the host side. `score.s` is `include`d at the bottom of uade_glue.s (so its local labels are reachable; everything added is prefixed `uade_`/`_uade_`). A fake ExecBase (2 KB, moved out of low memory, PORT 4) and a message trap (`uade_msg_trap` :1615) emulate what players expect from DeliTracker; low memory $100-$300 is snapshotted at init and restored in `_music_shutdown`. score's play loop runs as a coroutine resumed once per VBlank from `_music_play2` (:18-24, `uade_yield` :569).

## B2. Public API (assembly, music-backend contract shared with LSP/Cinter backends)

All in `amiga/uade_glue.s`: `_music_init` (:215, called once from main_glue.s:193 / hdd_startup.s:553, bounces through Exec Supervisor), `_music_start` (:1400), `_music_stop` (:1405), `_music_set_pos` (:1415), `_music_shutdown` (:1426), `_music_play1` (:638, VBL early tick; also samples the skip button at 50 Hz), `_music_play2` (:721, VBL late tick), `_music_select` (:751/759, song switch for playlists), `_music_select_pump` (:781/839, must be called once per frame from the main loop, never the ISR; C decl `amiga/amiga_main.c:92-101`). `UPROUGH_NO_MUSIC` stubs them all to `rts` (:195-210). Other exported state: `uade_regs`, `_uade_cur_song`, `_uade_cur_title`, `_uade_cur_composer`, `_uade_pending_song`, `_uade_publish_info` (:1537, debugger locator block at $340). No C structs; layout constants in `amiga/uade_layout.i` (UADE_INFO=$340, UADE_STAGE=$400). Platform contract: build-time constants from `out/uade_sizes.i` (UADE_MODULE_BYTES, UADE_RELOC_BYTES, UADE_POOL_BYTES, UADE_ANY_BYTES, UADE_SONG_COUNT).

Note: there is no standalone player API to link into another program; it is wired into the demo runtime (display, VBL vector $6c, CIA ownership). Reusing it in a music player means reusing glue + score.s + staged blobs, taking over $68/$78 (CIA-A/B) and saving/restoring them.

## B3. Build and staging

- `amiga/uade.mk`: active when `UPROUGH_MUSIC_BACKEND` contains `uade`. Chain: project `music.filename` module -> `eagleplayer.conf` prefix -> player; `tools/uade_stage.py` (858 lines) copies module + player + companions into `out/` (`current.uademod`, `current.uadeplayer`, `uade_sizes.i`, `current.uadefiles.bin/.i`). Options: `UPROUGH_UADE_PLAYER`, `_SUBSONG`, `_ON_END` (loop), `_SONGS` (playlist), `--target-cpu`, pool/any byte overrides. One object `out/uade_glue.o` includes score.s and relocator.s. Assembler vasm `-m68000`.
- Lookup: `tools/uade_lookup.py` (eagleplayer.conf prefix -> player, cross-checked with `uade123 -g`; mismatch = build error). Probe/triage rig: `amiga/uade_probe.s` (493 lines), `tools/uade_format_sweep.py`. Device build example: `make -f Makefile.amiga.hdd DEMO_JSON=<project>`. Tested on emulated stock A500 (68000, KS1.3, 512K chip).

## B4. Audio output

Paula directly. The replayers program Paula (AUDxLC/PER/VOL, DMACON) and their own interrupts (score's CIA timer for DTP_Interrupt, or the player's DTP_StartInt); there is no software mixing or resampling layer in the glue. The demo owns DMACON/INTENA; at init Paula is parked on a silence sample (uade_glue.s:454-463); switch paths mask/restore INTENA (:917). Chip RAM is the main cost; allocator splits MEMF_CHIP vs MEMF_ANY (comment near :30-40) so large tables (AHX 412 KB waveforms) can sit in fast RAM.

## B5. Formats

- Allow-list `amiga/uade_formats.txt` (86 lines; 171 players known, 59 owner-kept per the matrix doc; header says 167 keep, 116 with test module - the two counts disagree, header likely stale; unconfirmed which is current).
- Measured per-player arena sizes `amiga/uade_arena_sizes.tsv` (40 rows) -> 40 formats play on a stock 68000 per `thoughts/shared/handoffs/2026-09-21_uade-music-formats-read-this-before-adding-any.md`; earlier matrix `thoughts/shared/research/2026-09-20_uade-format-matrix.md` said 30 of 59 (28 sustain tempo, no buffer starved) - older. Ear-verified only for AHX (Cruisin) and JochenHippel (warp_ingame2.hip). Examples in the matrix: AHX, ArtOfNoise-4V, BenDaglish, DeltaMusic 1.3/2.0, plus Fred, MED, SoundMon, SidMon, FutureComposer-BSI, DIGI-Booster, PTK-Prowiz, MusicAssembler, PreTracker, Medley in vendored players.
- Vendor tarball `uade-3.05` (~173 replayers upstream).

## B6. Known limitations

- `speed_hack` formats cannot work natively: EMS-6, Oktalyzer, PreTracker, MusiclineEditor (rely on unbounded emulator CPU) - build refuses them. (Handoff.)
- 020-only: DIGI-Booster (address error on 68000; `amiga/uade_player_min_cpu.tsv`). Many UADE players assume a 020; fake ExecBase AttnFlags hardcoded `$0003` (score.s:322).
- Owner-skipped: SonicArranger-pc-all, SynthDream, SynthPack, Octa-MED, TFMX_ST (uade123 itself refuses TFMX_ST).
- Musicdisk/playlist switching (`_music_select`): live crash on switch (fault in level-6 CIA-B handler, incoming player image corrupt), unresolved at 2026-09-21; single-song path unaffected. Check `thoughts/spot/uade_ledger.md` and `thoughts/shared/handoffs/2026-09-21_SESSION_uade-musicdisk-engine.md` for later status (not read).
- Branch unlanded/ungated per handoff; no later status checked. Chip RAM per format is the A500 limiter.

## B7. Docs to read next

`thoughts/shared/handoffs/2026-09-21_uade-music-formats-read-this-before-adding-any.md`, `.../2026-09-20_SESSION_uade-backend-in-a-real-demo-and-measured-arenas.md`, `.../2026-09-21_SESSION_uade-musicdisk-engine.md`, `thoughts/shared/research/2026-09-20_uade-format-matrix.md`, `thoughts/spot/uade_ledger.md`, `docs/superpowers/specs/2026-09-21-uade-musicdisk-engine-design.md`. Repo README.md (LVLLVL) and AMIGA.md contain nothing on UADE.
