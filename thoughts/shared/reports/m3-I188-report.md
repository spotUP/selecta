# M3 I188 report: loudness gain store, fetch and submit through the DEViLBOX endpoint

Plan: `thoughts/shared/plans/2026-10-25-m3-library.md` Phase 9 row I188, decision D-M3-9, test T90 (fetch part).
Design: `thoughts/shared/plans/2026-10-22-architecture-design.md` section 6a.

## What it does

- `src/library/devilbox_loudness.{c,h}` (in `selecta_library`, integer math only, builds for 68k):
  - `sel_dvb_loudness_file_hash` md5 of the file bytes (reuses `sel_md5`).
  - `sel_dvb_loudness_get` GET `/api/loudness/<hash>` through `sel_http_request`: HIT, UNKNOWN (404 "unknown hash"), MISSING (transport error, other status, 429, unparseable body).
  - `sel_dvb_loudness_put` PUT with `Authorization: Bearer`, body lufs, peakDb, method, analyser. Returns the HTTP status, 0 on transport failure.
  - `sel_dvb_loudness_token_read` reads `<config>/devilbox.token` (one function; merge point with I173 login).
  - `sel_dvb_loudness_gain_cdb` gain = -18 LUFS target minus lufs, clamped to +-24 dB, 0 for silence.
- `selecta-cli loudness [--db p] [--machine name] [--config-dir dir] <song>`: stored gain, else GET (stored via `sel_library_loudness_set`, `loudness_source=devilbox`), else on a desktop machine ANALYSE through the engine and one PUT when a token exists.
  - Amiga (fake machine or `SEL_NO_ANALYSE`) never analyses.
  - No token: `loudness_put=skipped_no_token`. Endpoint missing: `devilbox_missing=loudness`, no PUT.
- Fixture server: a routes.json entry may be a list, first entry whose `key_method` matches wins; PUT bodies are logged like POST.
- Test `loudness-fetch` (`tests/net/loudness_fetch.cmake`): a1200 fetch (gain 540 cdb, 1 GET, 0 PUT), a1200 unknown hash (no analysis), desktop analysis plus exactly one PUT with Bearer token, path, body fields, no-token skip, missing route, stored second run (0 requests).

## Verification

- `ctest -R 'loudness-fetch|loudness-analysis|devilbox-genre'`: all pass. Fixture selftest passes.
- `selecta-cli commands --unimplemented | wc -l` = 10.
- Amiga cross build of `selecta-cli` target: ok (no -Werror hits).

## Deviations

- New `loudness` CLI verb instead of extending `loudness-playback` (that is I189/I190 mixer territory).
- PUT is skipped when the GET was MISSING (endpoint down): submitting to a missing endpoint is pointless.
- Not done: login (I173), mixer gain/limiter/leveler (I189/I190).
