# M3 I173 report: DEViLBOX ratings (T79 rate-devilbox)

Plan: `thoughts/shared/plans/2026-10-25-m3-library.md` Phase 6 row I173, decision D-M3-12. Design: architecture design section 9.2.

## Result

T79 `rate-devilbox` is green. `selecta-cli commands --unimplemented | wc -l` is 9 (was 10); `tests/ratchet/unimplemented.txt` lowered to 9.
Counters of the main run (`tests/m3/rate.sel`): `rating_pushes=1 rating_batches=2 rating_pending=0`; `RATINGS_READ keys=250 requests=2 stored=1`.

## What was built

- `src/library/net/devilbox.[ch]`: DEViLBOX client over the existing HTTP client. Login, token file `<config>/devilbox.token` (open with 0600 plus fchmod), `PUT/DELETE /api/ratings`, `GET /api/ratings/batch` in chunks of at most 200 keys with the Bearer header when a token exists.
- `src/library/net/http.[ch]`: DELETE now carries a body; `SEL_HTTP_URL_MAX` (1536 on Amiga, 8192 elsewhere).
- `src/library/db`: ratings ops `SEL_USER_RATING_SET/GET/PENDING/DONE` over the existing `ratings` and `pending_push` tables; `sel_url_encode` is now shared (was static in genre.c). A server rating never overwrites a local rating still pending.
- `src/engine/engine.c`: `RATE` command (now implemented), pending flush at engine create, stats `rating_pushes`, `rating_batches`, `rating_pending`.
- `apps/selecta-cli`: `devilbox login <user> [--password-stdin] [--config-dir]` (password from stdin only), script directives `@ratings` and `@ratings_read`, script option `--config-dir`.
- `tests/net/fixture_server.py`: stateful DEViLBOX login and ratings endpoints (modelled on the server routes); PUT and DELETE bodies are logged.
- Tests: `tests/m3/rate.sel`, `tests/net/rate_devilbox.cmake`, registered as `rate-devilbox`. Covers login (wrong password stores no token, mode 0600), byte-exact PUT body and Bearer header, 2 batch requests for 250 keys, DELETE for RATE 0, fixture down gives pending=1 then flush on the next run, local song never pushed.
- RULES.md Commands rows added.

## Other tests run (all green)

net-fixture-selftest, net-guard, http-client, devilbox-genre, radio, ratchet, plus the library tests matching the filter.

## Deviations and things to know

- HTTP runs on the engine command thread (in `cmd_RATE` and at engine create), not on the library worker. An unreachable network can stall that thread for the client timeouts. With no token the push returns at once without touching the network.
- Added script option `--config-dir`, because the script engine has no config dir otherwise and cannot find the token.
- Pending ratings flush at engine create, not through a separate command.
- On Amiga the batch size shrinks (URL cap 1536). The Amiga cross build was not run: no `build/amiga` directory exists in this worktree. Token file code has an Amiga branch without fchmod.
- Local songs (key not `modland:` or `hvsc:`): stored locally, no push, no pending.
