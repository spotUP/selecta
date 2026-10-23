# I215: http-client ctest fails now and then

## Cause
Step 10 (TLS) fetched `https://localhost:<tls_port>`. getaddrinfo returns ::1, 127.0.0.1, fe80::1 in that order.
sel_net_open (src/core/posix/net.c) connects to ::1 first. The fixture TLS listener binds 127.0.0.1 only.
Any other process listening on ::1 with the same port number answers instead of the fixture, and the handshake fails.
The product is right: after a successful TCP connect it does not retry other addresses (standard behaviour).
The test is racy: it depends on what else runs on the machine (many agent worktrees with fixtures).

Second defect: `cmd_start` in tests/net/fixture_server.py removed a stale pidfile without stopping the old server.
`ctest --repeat until-fail:N` reruns net-fixture-start without the cleanup test, so each iteration leaked a fixture server.

## Evidence
- Before: squatter on ::1:<tls_port>, original script: `T68 tls error: got 'total_timeout', want 'ok'` (exit 1).
  Standalone CLI run: http_error=tls_handshake_failed after 11052 ms.
- After: same squatter, fixed script: exit 0.
- Reverting the fix (original script) fails again, as the Before line shows.
- Not observed: the historical flake itself. No failure log existed. The squatter is a reproduced, plausible cause.
- Plain/chunked/range/redirect steps: 60 stress runs and 400 iterations with sha256 check, 0 failures. Timeout steps: 3001-3006 ms over 20 runs. No race found there.

## Fix
tests/net/http_client.cmake step 10: SELECTA_NET_MAP `localhost=http://127.0.0.1:<tls_port>`, SELECTA_NET_ALLOW `127.0.0.1`.
SNI, Host and certificate verification stay "localhost".
tests/net/fixture_server.py: cmd_start stops a previous server found via the pidfile.
Layer: test and fixture, because the product behaves correctly.

## Status
ctest repeat 3 passed with the fix. The 200-run repeat is started separately; the hand-off names the log.
