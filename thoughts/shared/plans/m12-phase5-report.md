# M1.2 Phase 5 report (I76, I77, I78)

Branch worktree-agent-a28a2d9a977164be8.

## Commits
- 017d9b8 key bindings table, keys.conf, --bindings (I76, T34)
- 9f82af0 command line, Tab completion, history, :! (I77, T36)
- I78 commit: command palette and help (T37)
- final commit: link fix (selecta-cli no longer lists selecta_core twice) and this report

## Evidence (red then green)
- T34 keybind-unit: red "Not Run, keybind.h not found"; green. selecta-bindings reachability uses unchanged browse_play.screen (T19 golden unchanged).
- T36 cmdline-unit: red "Not Run", link failure; selecta-cmdline red (Volume 100 vs 90), then green.
- T37 palette-unit: red "Not Run, palette.h not found"; green after the area string fix. selecta-palette: red against the old golden (Playing vs Paused), green with the Paused golden.
- Full suite excluding coreaudio: 61 of 61 passed.

## Ratchet
tests/ratchet/unimplemented.txt stays 29 (measured with selecta-cli commands --unimplemented).

## Deviations
- Extra default bindings: ? help, u UNDO, C-r REDO, m mark.
- A scripted --keys run ignores keys.conf unless --bindings is given.
- selecta_textui links selecta_core; F1..F12 decoding added to keys.c; the keys file knows the Ctrl-P token.
- Command line history is in memory only: the host API has no file write. Load and dump functions are tested.
- Selection for :! is a new set of marks (m, browse files), else the file under the cursor.
- The palette reads the area from sel_cmd_area, the same table that feeds the COMMANDS area= field, instead of parsing COMMANDS output at runtime.
- commands.def rows are SEL_CMD(id, args, area); the CLI commands output has an area= column. No existing golden holds COMMANDS output (T5/T6 abi goldens are layout only), so none changed.
- Palette Enter sends the bare verb; verbs needing arguments are typed on the command line.
- Amiga build configured and built clean; no_float count unchanged (no new engine/protocol/library objects).
- No Co-Authored-By lines, per the user global rule.
