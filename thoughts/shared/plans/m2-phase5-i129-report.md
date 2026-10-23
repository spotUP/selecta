# M2 Phase 5 I129 report

## Done
- Commands FILTER_LED (on|off|toggle) and MIX_STEREO_WIDTH (0..100, +n/-n relative) in commands.def, implemented in src/engine/engine.c. Both call sel_engine_set_stage, set the pref, emit PREFS_CHANGED, save prefs.
- Prefs: new mix.stereo_width (default 100). LED reuses output.led. Engine applies both from prefs at create time (after prefs load).
- sel_state gained led_on and stereo_width.
- Text UI: default keys L (LED Filter toggle), ( and ) (Stereo Width -10/+10); status line shows "LED Filter On" / "Stereo Width N" when non-default.
- selecta-cli render.c: led=/width= sentinels print whenever the stage is active, not only for the flags.
- Test stage-commands (tests/m2/stage_commands.*): script path equals flag path hash, differs from plain, led_frames>0 and width_frames>0.

## Tests
- Host: ctest affected set 100% passed out of 28 (rig-selecta-upterm skipped); final re-run of mixer-led-stereo and stage-commands passed.
- T8 ratchet: unimplemented count 21 before and after (new rows were added already implemented).
- build/amiga builds clean. Vamos T53 not run separately (unverified).

## Deviations
- Verbs are FILTER_LED / MIX_STEREO_WIDTH, not dotted filter.led / mix.stereo_width (wire verbs are the uppercase ids).
- LED pref reuses output.led, no new key.
- Status line shows the settings only when non-default (always showing broke many golden screens).
- Keys: x/X unavailable (keybind-unit asserts x unbound), so L, (, ).
- stage-commands test is desktop only; T53 covers 68k.

## Open questions
- Is a vamos run of T53 needed for this row?
- Caller to update plan deviation block / RULES.md if wanted.
