# I532 rig 2 sound check

Build: main HEAD 6d30319 plus the uncommitted working-tree change in plugins/ui/desktop/desktop_ui.c (it was in the tree and is in the binary). build/amiga-aga configured fresh with -DSEL_DESKTOP_AGA=ON and host bake tools from build/sdl; `ninja -j2 selecta` rc=0, 19445372 bytes. Disk 2.8 GiB free.
Rig 2 restarted with purpose "Selecta session: I532 Amiga sound check", `rig.py aga`, files put to SYS:selecta-test (Selecta, mods/ambient.mod, mods/sine.ogg, aga.script).
Command: Selecta --ui desktop --backend aga --format index8 --view library --stats --log RAM:aga.log --db ... --exec "OPEN SYS:selecta-test/mods/ambient.mod" --exec "PLAY".

Evidence (run 1, stopped with `Break 7 C` to flush the log):
- aga-out.txt: `replayer=nuade`, `songs_opened=1`, `song=SYS:selecta-test/mods/ambient.mod`
- desktop_ui closed: `title=Ambient Drift clock=00:30` (position moved to 30 s)
- no Guru (rig.py wasabi guru: no Guru recorded); no `amiga_start: stack overflow` line in RAM:aga.log
- No freeze: Selecta stayed alive (ps: state wait, 195718 of 200000 stack free) in both runs.
NOT proven by text: the actual output swap paula -> paula14/ahi. Neither the log nor --stats prints the output id, and shell_switch_output logs nothing. Snoop shows only DOS calls (no audio.device/ahi opens). SEL_MACHINE_AUDIO on rig 2 not visible. Needs a one-line trace in shell_switch_output, or Johan's ears.
Not done: no PCM/render-kind fixture played (sine.ogg is on the rig but unplayed); `selecta-cli play --output paula` freeze not retried.
Note: the library was empty in the UI (Add Folder + OK did not fill it); the song was opened by --exec instead.
Rig 2 state: Selecta (run 2) playing ambient.mod from SYS:selecta-test/aga.script; snoop ended by itself.
