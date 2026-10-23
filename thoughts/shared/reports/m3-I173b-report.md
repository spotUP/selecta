# I173b: one DEViLBOX token reader

Status: code and test changed, NOT built or run (see Blocked).

Changes
- apps/selecta-cli/analyse.c: the loudness PUT path calls `sel_devilbox_set_config_dir(config)` then `sel_devilbox_token()` (I173 store, src/library/net/devilbox.c).
- Deleted `sel_dvb_loudness_token_read` (declaration in devilbox_loudness.h, definition, its own path logic and trim loop in devilbox_loudness.c).
- tests/net/loudness_fetch.cmake: the token is now written by `selecta-cli devilbox login loudness --password-stdin --config-dir <cfg>` (fixture jwt `fixture-jwt-loudness`), and step 3 asserts `Authorization: Bearer fixture-jwt-loudness`. This covers "loudness fetch uses the token written by login". It would also pass before the change (both readers read the same file), so it is a coupling test, not a fail-before regression; a true fail-before case does not exist because the duplicate read the same path.

One-reader count: header declarations matching `*_token(` / `token_read` in src/, include/, apps/ headers = 1 (`sel_devilbox_token`, src/library/net/devilbox.h:25). `git grep sel_dvb_loudness_token_read` over src apps tests include = 0.

Blocked: the worktree's submodules third_party/nuade and third_party/libopenmpt are empty, so cmake configure fails (nuade_players.cmake missing). Symlinking them to the main checkout was denied by the permission classifier and third_party is off limits. Not run: loudness-fetch, rate-devilbox, amiga build. Only `cc -fsyntax-only` on devilbox_loudness.c passed. Needed: initialise submodules in this worktree (or run the build in the main checkout after merge), then `ctest -R "loudness-fetch|rate-devilbox"`.
