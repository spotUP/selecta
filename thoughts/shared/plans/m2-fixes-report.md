# M2 fixes report

Source: deviations tail of `2026-10-24-m2-amiga.md` (Phase 6, Phase 4/7).

1. txt_spawn quoting: new `txt_quote_arg` (plugins/ui/text/quote.c, all platforms) wraps a path in quotes and escapes `"` and `*` with the AmigaDOS `*` escape; `amiga/spawn.c` uses it. posix/spawn.c builds an argv, so it had no quoting bug. Regression in `cmdline-unit` (fails with the escaping removed, verified).
2. Start dir: `sel_path_is_absolute[_style]` in src/library/local.[ch] (the existing path helper home); `apps/selecta/main.c` uses it and defaults to `PROGDIR:` on Amiga. Test `library-path-unit`. `amiga-text-ui-vamos` (T55) and `selecta-browse-play` green. Note: the browse code (`sel_browse_join`, `..`) is still `/` only, so navigating inside `Work:Music` on a real Amiga is unverified and probably wrong; separate item.
3. paula14 flush: `out_close` zero-pads and submits the partial buffer through new `sel_p14_submit_n` (counts only real frames). Regression in `paula14-unit` using a counting bus (frames out 1000 == in 1000; fails with the flush disabled, verified). The real shadow plugin's bus only counts writes, so the test uses its own kind over the same `p14_out.c`. Unverified: on hardware `sel_p14_close` stops DMA right after the flush, so the last buffer may not be heard.
4. RULES.md `## Commands`: two rows added (`--rp-config`/`--rp-config-song` with `rp_config_applied=`; paula14-shadow `p14_frames= p14_hash=`), flag spelling checked in apps/selecta-cli/render.c.

`selecta-cli commands --unimplemented | wc -l` = 21. Tests run: cmdline-unit, library-path-unit, paula14*, amiga-text-ui-vamos, selecta-browse-play, all pass. No full sweep.
