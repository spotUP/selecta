# M3 Phase 3a report: rows I152 and I153

## Rows
- I152 socket table (`src/core/posix/net.c`, `src/core/amiga/net.c`, shared parsing in `src/library/net/netpolicy.c`, API in `src/library/net/net.h`): guard (`SELECTA_NET_ALLOW`, non-loopback refused, `net_blocked` counted), host map (`SELECTA_NET_MAP`), `SELECTA_CA_FILE` getter. TLS is I154 and not included.
- I153 HTTP/1.1 client (`src/library/net/http.c`, `http.h`) and `selecta-cli fetch` (`apps/selecta-cli/fetch.c`): GET/PUT/POST/DELETE, chunked decoding, Range, up to 5 redirects, 3 s connect, 3 s first byte, 30 s total.

## Commits
- 0cd21a3 T68 http-client test and fixture.test fixtures, red.
- 2fefa4f implementation, green.

## Tests
- T67 net-guard: green. T68 http-client (plain HTTP parts): red at 0cd21a3 (no `fetch` command), green at 2fefa4f (33 s, real timeouts). net-fixture selftest and stop: green.
- Builds: `build/host` (Debug) and `build/amiga` both compile and link with -Werror.
- `selecta-cli commands --unimplemented | wc -l` is 21, unchanged (`fetch` is a CLI command, not a `commands.def` verb).

## Deviations
- Plan layout names `src/core/posix/net.c`; existing OS code lives in `src/core/macos`. Followed the plan with a new `posix` directory.
- The guard judges the mapped target, not the original host name (net-guard maps `devilbox.uprough.net` to 127.0.0.1 with ALLOW=127.0.0.1).
- `http-client` has `DEPENDS net-guard` because net-guard counts the whole fixture request log.
- Amiga `net.c` is untested at runtime (compile and link only, no TCP/IP stack under vamos). It parses dotted quads itself because `arpa/inet.h` does not compile under -std=c99 with this NDK.
- https returns `tls_unavailable` and `tls_handshakes` is fixed at 0 until I154.
- `sel_net_ca_file` exists but is unused until I154.
- Stack use per request: 4 KiB body buffer plus 8 KiB header buffer.

## Open questions
- None blocking. TLS (I154) will need to hook into `sel_net_open` handles.
