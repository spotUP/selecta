---
date: 2026-10-23
topic: M1.2 validation (T28 to T38 and T8)
tags: [validation, m1.2, playlists, views, control]
status: in-progress
plan: thoughts/shared/plans/2026-10-23-m1-2-playlists-views-control.md
---

# M1.2 validation

## Full run (2026-10-23, Debug, macOS arm64, landing M1.2)

Reason for the full run: landing the milestone (plan Verification). Main at merge `e677438`.
`cmake --build build/host -j4` green; `cmake --build build/amiga -j4` green;
`nice -n 19 ctest --test-dir build/host -j2 -E coreaudio`: `100% tests passed out of 67`.
The one excluded test is `coreaudio` (needs an audio device; excluded in every unattended run).

## Reconciled counts (each from its source)

- ctest total: 68 (`ctest --test-dir build/host -N`, "Total Tests: 68"); 67 run, coreaudio excluded.
- Ratchet unimplemented count: 27 (`selecta-cli commands --unimplemented | wc -l`, equals `tests/ratchet/unimplemented.txt`); M1.2 target 27.
- no_float objects: 22 (`cmake/no_float.cmake` asserts `nobjs EQUAL 22`).

## Per test

| Test | ctest name | Result | Red evidence (ledger) |
|------|-----------|--------|-----------------------|
| T28 | selecta-m12-reach | Passed | `Test #51: selecta-m12-reach ***Failed` before wiring (5761822) |
| T29 | playlist-codecs | Passed | I69 |
| T30 | queue-history | Passed | I70 |
| T31 | queue-scope-album | Passed | I72 |
| T32 | view-tree | Passed | segfault before view.c grouping (fd69a7a) |
| T33 | title-format | Passed | I68 |
| T34 | keybind-unit | Passed | I76 |
| T35 | control-socket | Passed | `control-socket ***Failed` (21c13d7) |
| T36 | cmdline-unit | Passed | I77 |
| T37 | palette-unit | Passed | I78 |
| T38 | integration-nowplaying | Passed | `integration-nowplaying ***Failed` (21c13d7); expects `duration_ms=0` (see Deviations, Q20) |
| T8 | ratchet | Passed | 27 |

T28 counters observed: `cmdline_runs=3 palette_opens=1 bindings=<default+1> playlist_entries=3 undo_depth=1 redo_depth=1 view_style=tree`; last screen equals `tests/m12/reach.screen`.

## Open

- I81: the Linux MPRIS plugin is not compiled on this host (needs libsystemd); unverified until Linux CI (I44).
- Manual checks only the user can do: macOS Control Center (play, pause from Control Center, shell pauses); view keys, Ctrl-P palette, `?` help and `:` command line in a real terminal.
- Questions Q11 to Q23 in `thoughts/spot/questions-for-tomorrow.md` (local).
