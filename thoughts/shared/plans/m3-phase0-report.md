# M3 Phase 0 report (I139, I140, I141)

Rows
- I139 endpoint facts: `thoughts/shared/research/2026-10-25-m3-external-endpoints.md`.
- I140 fixtures: `tests/net/fixtures/` (about 14 KB) plus `PROVENANCE.md`. All real, none synthetic.
- I141 fixture server and net guard: `tests/net/fixture_server.py`, `fixture_ctl.cmake`, `gen_test_ca.cmake`, `net_guard.cmake`, ctest block in `tests/CMakeLists.txt`.

Tests (ctest -R net-)
- net-fixture-start, net-fixture-stop, net-fixture-selftest (10 checks): pass.
- net-guard (T67, label m3): RED by design. Fails with "selecta-cli: unknown command fetch". Turns green when I152/I153 add `fetch` plus the SELECTA_NET_ALLOW / SELECTA_NET_MAP / SELECTA_CA_FILE guard.
- `selecta-cli commands --unimplemented | wc -l` = 21 (unchanged).

Deviations
- `allmods_md5.zip` does not exist upstream (404). `allmods.zip` holds `allmods.txt` with `<size>\t<path>`, no md5.
- `/api/hip/votes` exists live (400 `ERR bad`, parameters unknown) but is absent from DEViLBOX source.
- TLS and FTP listeners in the fixture server have no ctest consumer yet.

Missing from servers (degrade local-only, D-M3-13): hip_vote (params unknown), loudness (404), lyrics, comments, mirror (nearest: /api/modland/download, /api/hvsc/download), song and playlist share-URL layout.
