# M3 I181 report: xxh32, UADE key, genre lookup and cache, genre: search

## Endpoint (DEViLBOX server routes/genre.ts, research 2026-10-25-m3-external-endpoints.md)
`GET <base>/api/genre/<key>?name=<url-encoded basename>&size=<bytes>` (optional `md5=` is not sent).
Key = 12 lowercase hex = hex8(xxh32(first 262144 bytes, seed 0)) + hex4(total length & 0xFFFF).
Reply 200 `{key, genre|null, source, modarchiveId}`. Fields used: `source` and `genre`. The genre counts only when
`source == "modarchive"` and `genre` is a string. `untagged`, `miss`, `ambiguous`, `unsupported`: final answer "no genre", cached
as NULL. 400, 502, 503 (not configured, queue full, budget), transport error: genre unknown, NOT cached, no error.
Base URL: `sel_devilbox_base()` (env `SELECTA_DEVILBOX_URL`, default `http://devilbox.uprough.net`), shared with the Amiga mirror route.

## Design
- `src/library/xxh32.c/.h`: `sel_xxh32`, `sel_uade_key`.
- `src/library/db/genre.c`: worker half (`SEL_LIB_REQ_GENRE`: NEXT local song, GET/PUT `genre_cache`, APPLY to `songs.genre`,
  skipped when the song has sticker `tag.genre`); caller half `sel_library_genre_sync` reads the file head, GET cache, else one
  request through `sel_http_request` (the only HTTP client), PUT cache, APPLY.
- `selecta-cli library genre --db <path>` prints `songs= genre_lookups= genre_cache_hits= genre_set= unknown= unreadable= http_requests=`.
- `genre:` search needed no change: `src/library/query.c` already maps `genre` to `songs.genre`.

## Tests (all green, host)
- `xxh32-unit` (label m3): vectors empty 0x02CC5D05, "a" 0x550D7456, "abc" 0x32D153FF, seeded empty, a 39 byte string, key layout.
- `devilbox-genre` (label m3): scan tests/m3/genre (4 retitled copies of the play1.mod fixture), run 1 `genre_lookups=4 genre_set=2 unknown=2`,
  `genre:ambient` 1 song, `genre:techno` 1, `genre:electronic` 2; run 2 `genre_cache_hits=3 genre_lookups=1` and exactly 1 request in the
  fixture log (the 503 song). `ctest -L m3`: 32 of 32 pass. Amiga `selecta-cli` target builds (68020 toolchain).

## Leftovers
- T85 songdb and composer parts (I182) not done.
- Key uses the on-disk length; decompressed length of packed files is not handled. `md5=` not sent.
- 256 KB static head buffer in genre.c; a streaming xxh32 would save RAM on the Amiga.
- The rescan upsert may clear `songs.genre` until `library genre` re-applies it from the cache (unverified).
- No automatic call: `library genre` is explicit; no trigger after scan yet.
