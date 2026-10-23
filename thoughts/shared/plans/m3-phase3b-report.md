# M3 Phase 3b report: TLS and FTP (rows I154, I155)

Plan: `thoughts/shared/plans/2026-10-25-m3-library.md`, decisions D-M3-4 and D-M3-5.

## Rows

- I154 (TLS): vendored mbedTLS, `src/library/net/tls.{h,c}` under the HTTP client, OS CA store on desktop, `SELECTA_CA_FILE` override. Done on desktop. The Amiga build compiles and links; runtime untested.
- I155 (passive FTP): `src/library/net/ftp.{h,c}`, reachable through `selecta-cli fetch ftp://host/path`. Done on desktop; compiles for Amiga, runtime untested.

## Commits (branch worktree-agent-a15b3c45b69b31a0c)

1. `c970368` red tests: T68 TLS parts, T70 ftp-transport (before this phase's work, red).
2. `2a6c2f4` vendor mbedTLS 3.6.7.
3. `c997a74` implementation (tls, ftp, http wiring, fetch, CMake, test expectation fixes).

## Tests

- T68 TLS parts (in `http-client`): red at c970368 (tls_unavailable), green at c997a74. Handshake against the fixture TLS port with `SELECTA_CA_FILE`, `tls_handshakes=1`, body sha256 matches; without the CA file the fetch fails with `tls_verify_failed` and no body.
- T70 `ftp-transport`: red at c970368 (fetch did not know ftp://), green at c997a74. Checks the command sequence in the fixture log (USER anonymous, TYPE I, SIZE, PASV, RETR), SIZE equals file size, sha256 equal, missing file gives `no_such_file`.
- Ran only `net-guard|http-client|ftp-transport`: 5 of 5 passed. `selecta-cli commands --unimplemented | wc -l` is 21.
- Test expectation fixes made in the green commit: success error name is `ok` (not `none`); FTP prints its own `ftp_error=` line.
- No test contacts a non-loopback host.

## mbedTLS

- Version 3.6.7 (3.6 LTS), tarball SHA-256 in `third_party/mbedtls/README.md`. Upstream files unmodified; configuration is `selecta_mbedtls_config.h` (TLS 1.2 and 1.3 client; no server, DTLS, net, timing, session tickets, PSA file storage).
- Desktop: `libselecta_mbedtls.a` is 3.5 MB in the Debug build.
- Amiga: TLS is excluded (`SEL_NO_TLS`, plain HTTP per D-M3-4), so mbedTLS adds 0 bytes to the Amiga binary. Object sizes (Amiga, 68020): `tls.c.obj` 3.4 KB (stubs), `ftp.c.obj` 13.6 KB, `http.c.obj` 24 KB. No before/after binary comparison taken; the Amiga `selecta-cli` in the unstripped build is 11 MB.

## Deviations and caveats

- TLS excluded on Amiga (D-M3-4). https:// on Amiga answers `tls_unavailable`.
- Fixture: Host header alias `localhost` -> `fixture.test` so https://localhost:<port> is served from the fixture tree (the test certificate is for localhost).
- T70 fetches `changelog_2026-10-22.txt` because the fixture has no `pub/modules` path.
- mbedTLS uses malloc/calloc internally (desktop only); the "no allocation" property of the HTTP client holds for plain HTTP.
- FTP data connection uses the PASV reply address as given; a NAT'd server answering a private address fails with `data_failed`.
- mbedTLS includes the PSA crypto layer (required by TLS 1.3 in 3.6).
- Windows-style OS roots that mbedTLS cannot parse are skipped silently.

## Open questions

- Whether FTP should fall back to the control host address when PASV answers a private address (real-world NAT).
- Whether to ship an embedded CA bundle for Linux hosts without a system store.
