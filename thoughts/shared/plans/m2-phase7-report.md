# M2 Phase 7 report (I134, I131)

## Rows
- I134 SEL_RP_CONFIG tag list, prefs, per-song override (D-M2-8): DONE. T61 rp-config-taglist and rp-config-taglist-vamos green.
- I131 Amiga spawn test: NOT DONE. amitools 0.7.1 dos.library has no SystemTagList (grep over the installed package finds none), so vamos cannot run txt_spawn. Deviation: the test stays rig-labelled and is not added to the vamos set. Needs the rig (T56 family).

## Design (I134)
- New pref rp.config (plain string, "plugin.key=value" entries separated by commas), prefs.def.
- Engine hands replayers a host copy whose config_get answers from rp.config, then from the per-song override (sel_engine_set_song_config), which wins; otherwise falls back to the original host config_get. Stat rp_config_applied counts answered keys.
- CLI: render --rp-config and --rp-config-song; prints rp_config_applied=N.
- nuade declares SEL_RP_CONFIG nuade.gain (0..100, default 100), applied in the Selecta voice shim. openmpt declares openmpt.interpolationfilter.

## Tests
- rp-config-taglist, rp-config-taglist-vamos, amiga-build: pass.
- Regression subset (prefs, render-golden, selecta-browse-play, nuade-players, selecta-cmdline, cmdline-unit, amiga-tooltypes-cli-vamos): 17 of 17 pass.
- selecta-cli commands --unimplemented count: 21 (unchanged).

## Deviations
- T61 key changed from openmpt.interpolationfilter to nuade.gain: the test song plays through nuade, the Amiga build has no openmpt, nuade has no own options, and nuade separation did not move the hash (protracker4 sets no pan).
- No library DB in M2 (arrives with M3): the per-song override is an engine input fed by --rp-config-song, with no persistence.

## Open items
- I131 vamos test (see above); txt_spawn does not escape a double quote inside a path.
- openmpt option untested (desktop only, default likely equals 8).
- I135/T60 out of scope; nuade SEL_RP_NEEDS_FLOAT budget unchanged.
- amiga-text-ui-vamos, paula14-tables* and rig tests not rerun here.
