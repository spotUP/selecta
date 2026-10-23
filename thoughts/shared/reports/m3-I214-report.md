# I214: Amiga config directory and invisible DOS requesters

Design reference: thoughts/shared/plans/2026-10-22-architecture-design.md, section 5 layout line "src/core/: ... config" (no ENVARC-specific section exists; the location rule lives in the src/core/config_dir.c header and M1.2 "Files and locations").

## Finding (the ledger attribution is partly wrong)

`selecta-cli plugins` and `render` never call `sel_config_dir` or `sel_config_mkdir`. Evidence: `vamos -l dos:info` traces
of `plugins --lint` and `render nofile.mod` on the 68k binary show no Lock/Open of ENVARC: or ENV:; callers of
`sel_config_dir` are only devilbox login, analyse (loudness upload), text UI, apps/selecta main and prefs save (`--config-dir`).
So the config_dir.c requester cannot be the cause of the `plugins`/`render` hang reported in m3-I194. That hang is
unexplained (unverified; vamos does not emulate requesters or the 060 FPU=0 configuration). The I194 report also had FPU=0,
which hides the NEEDS_FLOAT replayers, an equally likely cause for `render`. Real-machine check left to the rig owner.

## Fix (real defect, same class)

`sel_config_dir`/`sel_config_mkdir` did Lock/CreateDir on `ENVARC:Selecta` with requesters on. Now:
- src/core/amiga/dos_quiet.c: `sel_amiga_dos_quiet_begin/end` (save pr_WindowPtr, set -1, restore; no-op when not a DOS process)
  and `sel_amiga_name_exists(name)` (silent Lock probe). No pr_WindowPtr helper existed before (grep src apps plugins).
- src/core/config_dir.c, fallback order on Amiga: `ENVARC:Selecta` if `ENVARC:` locks, else `PROGDIR:Selecta`.
- src/core/amiga/config_mkdir.c: CreateDir/Lock run quiet.
- `ENV:` is not probed anywhere in our code (grep: only config_dir.c/config_mkdir.c mention ENVARC/ENV:). libnix `getenv`
  (used for SELECTA_* and HOME) may consult ENV:; not verified, not changed. A candidate for the real-machine check.
- New sentinel command `selecta-cli config-dir` prints `config_dir=<path>`.

## Test

`amiga-config-dir` (tests/amiga/run_config_dir.cmake): vamos, ENVARC: unassigned -> exit 0 within 60 s and
`config_dir=PROGDIR:Selecta`; with `-V cfgvol:... -a ENVARC:cfgvol:` -> `config_dir=ENVARC:Selecta`.
Fail-before: with the old unconditional line, `ENVARC: unassigned must fall back to PROGDIR:Selecta, got: config_dir=ENVARC:Selecta`.
vamos cannot reproduce the requester hang itself, so the test checks the dir (wrong dir fails), not a timeout.
Passed with amiga-roundtrip, amiga-machine (ctest -j1).

## Blocked on me

Rig check with ENVARC: unassigned: run `selecta-cli config-dir`, `plugins`, `render`. If `plugins`/`render` still hang, the cause is elsewhere (FPU=0 or libnix getenv/ENV:).
