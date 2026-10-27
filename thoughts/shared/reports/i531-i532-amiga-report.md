# I531 and I532: Selecta on Amiga AGA desktop (rig 2)

Status: causes found from source and rig 2 stale logs; NO code fix, NO regression test, NO rig proof yet (tool budget used on diagnosis and a rig 2 hang).

## I531 Add Folder does nothing

- Cause: `plugins/ui/desktop/amiga/backend_aga.h:68` and `backend_rtg.h:63` define `desk_sdl_open_folder_dialog(b)` as `((void)(b), 0)`. `desk_add_folder` in `plugins/ui/desktop/desktop_ui.c` (~7796) then has no dialog answer and shows the toast "No folder dialog here". No LIBRARY_SCAN is sent (`scans=0`).
- Violated invariant: Add Folder offers a folder chooser on every desktop backend (design section 7: "file requesters use asl"). Layer: the Amiga backends.
- Planned fix: ASL directory requester (`ASLFR_DrawersOnly`) in shared `plugins/ui/desktop/amiga/input.c` (asl.library opened explicitly, base renamed like the intuition base there), result queued as `SEL_GUI_IN_DROP` into `desk_amiga_input.queue`, exactly as `backend_sdl.c` (~348, `folder_chosen`) does. Replace both macros with real functions in `backend_aga.c` and `backend_rtg.c`. CMake: `plugins/ui/desktop/CMakeLists.txt` 84-115, also the `selecta-aga-first-light` probe.
- Test plan: host unit test of the queued-DROP helper (path in, DROP event out) split from the Amiga-only call, registered in ctest, plus rig test next to `rig-m4-aga-first-light` (`tests/CMakeLists.txt` ~1051-1081). Revert-check required.

## I532 silent playback

- Cause: the default output on the Amiga is `paula` (`sel_registry_default_output`, `src/engine/registry.c:173`, machine AUDIO list `paula,ahi`; chosen at `apps/selecta/main.c:363`). `paula` is a voice sink: `out_pump` in `plugins/outputs/paula/paula_out.c:69-92` pulls PCM into a scratch buffer and discards it. Johan's earlier run used `replayer=nuade` (PCM render replayer, `engine.c:1309`, `rp_render` branch), `hw_starts=0`, so every PCM frame goes into `s->buf` and nothing reaches a speaker. Position and clock still move (65589 uim updates, 75 frames), which matches "plays but silent".
- Violated invariant (design section 5): "VOICE player and open voice sink -> Paula direct. Otherwise mixer -> the PCM sink." A render or mixed-path song must never land on a voice-only sink. Layer: output selection / routing in the shell+engine (`shell_handover` in `apps/selecta/main.c:109` is the existing hook to swap outputs; `engine.c:1340` decides route_direct).
- Planned fix: when the song needs PCM (rp_render, or mixer route) and the open output has `SEL_OUT_VOICES`, the shell opens the machine's first PCM-capable output (`ahi`, else `paula14`) for that song and returns to `paula` for voice songs, reusing the I219 handover. Test: host test with `--machine` AUDIO=`paula,ahi`, nuade/openmpt song, assert frames reach a PCM sink (`ahi-shadow`) and fail when reverted; register in ctest.
- Rig 2 evidence: `selecta-cli machine` => AUDIO=paula,ahi; AHI devices present (paula.audio, filesave.audio, replay.audio). Not yet measured: `--output ahi` stats with sound. Rig FS-UAE config (`~/Code/UP-Term/vtcon/build/rig2/vtcon-rig.fs-uae`): `uae_sound_output=exact`, `volume=100`, so audio is not muted at rig level (audibility itself unverifiable from text).

## Found

- `selecta-cli play sys:selecta-test/mods/techno.mod --seconds 3 --output paula` on rig 2 froze the entire Amiga (wasabid ping timeout, fs-uae ~96% CPU, no recovery in 2 min); rig 2 was restarted with purpose "Selecta session: I531 add folder + I532 silent playback" and answers `ami.py ping` again. Direct chip-register poking via the real `paula` output while the OS owns audio is a separate bug candidate (should probably claim audio.device / allocate channels).
- Earlier AGA run log ended with "amiga_start: stack overflow, canary at the stack bottom overwritten (stack too small)" in `ram:aga.log`; `aga.script` sets Stack 200000.
- Binaries on the rig (`SYS:selecta-test`) come from an earlier AGA build; no `build/amiga-aga` tree exists, a fix needs a fresh AGA build to be proven.
