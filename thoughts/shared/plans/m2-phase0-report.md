# M2 Phase 0 report (rows I105, I106)

Plan: thoughts/shared/plans/2026-10-24-m2-amiga.md, Phase 0. Tests registered in tests/CMakeLists.txt (block "M2 Phase 0"), scripts in tests/m2/ and tests/amiga/run_rig.cmake. Every script fails with `RED[Tnn]: ...`.

## Tests added and red reason (run: ctest -R <name>, 20 selected incl. 3 existing; 14 failed, 3 skipped)

| Test | Name | Red reason |
|---|---|---|
| T49 | registry-machine-filter, -vamos | sentinel `filtered=` missing: `plugins --machine <tier> --stats` not implemented |
| T50 | amiga-machine-probe-vamos | sentinel `machine.gfx=` missing: `machine` has no probe output |
| T51 | paula-voice-registers-vamos | `render --output paula-shadow` exits 2 (not implemented) |
| T52 | paula14-tables, -vamos | `render --output paula14-shadow` exits 2 (not implemented) |
| T53 | mixer-led-stereo, -vamos | `render --led/--stereo-width` exits 2 (not implemented) |
| T54 | no-float-interrupt | build/amiga/m2-interrupt-objects.txt missing: no interrupt/audio-task target |
| T55 | amiga-text-ui-vamos | build/amiga/apps/selecta/selecta not built (no Amiga selecta) |
| T57 | amiga-library-browse-vamos | `library browse` exits 2 (not implemented) |
| T58 | amiga-tooltypes-cli-vamos | `--dry-run` / ToolTypes exits 2 (not implemented) |
| T61 | rp-config-taglist, -vamos | `render --rp-config` exits 2 (not implemented) |
| T56 | rig-selecta-upterm | Skipped (rig down); up-path: Amiga Selecta absent is red |
| T59 | rig-ahi-render | Skipped (rig down) |
| T60 | rig-nofpu-voice-budget | Skipped (rig down); up-path needs SEL_RIG_NOFPU=1 and a no-FPU rig |

Vamos twins register only when vamos and build/amiga/apps/selecta-cli/selecta-cli exist; T54 also needs m68k-amigaos-nm.

## Rig driver

tests/amiga/run_rig.cmake uses `ami.py` (CLI over the same wasabid as the `amiga` MCP), since a ctest script cannot call MCP tools. Rig down (python3 or ami.py missing, `rig.py status` says down, or `ami.py ping` non-zero) prints `SKIPPED rig not running`; ctest `SKIP_REGULAR_EXPRESSION` reports Skipped, never Passed. Label `rig`, `RESOURCE_LOCK rig`. The rig was down during this work; `ami.py ping` printed "auth failed: bad key". The rig-up path is UNTESTED.

## Deviations from the plan

- Extra sentinel names: reg_log_sha256, p14_max_err_lsb, p14_hash, hidden_min_cpu, hidden_needs_float, width_mono, led_cutoff_hz, prefs_hash, task_pri, ui_hash, ahi_swaps_expected, players_measured.
- Tier names a500, a1200, a1200-fpu, a060.
- T57 uses a vamos `-V songs:<dir>` volume instead of `T:songs`.
- T58 ToolTypes fixture is plain text (`--tooltypes`), not a binary `.info`.
- T54 object list comes from build/amiga/m2-interrupt-objects.txt, written by the future build from the target list.
- T55 uses `--size 80x25` (desktop selecta has no `--size` yet).
- T61 key `openmpt.interpolationfilter` and flag `--rp-config-song`.
- T56/T59/T60 CLI flags (`--stats`, `--play`, `--budget-batch`) are placeholders.
- Goldens (tests/golden/m2/*) are unpinned: absent means red by design; pin when the feature lands.

## Open questions (default)

- Exact flag names for stats, LED, stereo width, rp-config: default as used here; rename in the scripts when the M2 rows fix the names.
- T60 no-FPU rig start: default `rig.py start --a1200 --nofpu` plus env SEL_RIG_NOFPU=1.
- Rig auth failure ("bad key"): default treat as rig down (skip); fix key on the rig side.
