# I531 Amiga Add folder via asl.library

Cause: backend_aga.h and backend_rtg.h defined desk_sdl_open_folder_dialog(b) as 0, so desk_add_folder (desktop_ui.c) fell through to the toast "No folder dialog here" and never queued the drop that sends LIBRARY_SCAN.

Fix: desk_amiga_input_pick_folder (plugins/ui/desktop/amiga/input.c) opens asl.library 37 for the call, runs a FileRequester with ASLFR_DrawersOnly, ASLFR_Screen = the Selecta screen, and queues the chosen drawer as SEL_GUI_IN_DROP (x = 0) in the shared Amiga input queue; desk_aga_ and desk_rtg_open_folder_dialog wrap it and the stub macros now map to them. Pure half: amiga/folder_pick.c (AddPart-style join, cancel = empty drawer = nothing queued, over-long path refused).

Test: ctest dui-amiga-folder-pick (tests/dui/amiga_folder_pick_unit.c): path with spaces, volume root, trailing slash, drawer plus file, cancel (NULL and empty), overflow. Revert check: removing the '/' insertion in the join makes it fail; restored, it passes.

Compile: m68k-amigaos-gcc 68020 -std=c99 -Wall -Wextra -pedantic -Wshadow -Wstrict-prototypes -Werror compiled input.c, folder_pick.c, backend_aga.c, backend_rtg.c cleanly (objects only, not a full link; the full Amiga configure needs the host bakers).

Open: blocking requester (the screen keeps its last frame while open); not run on rig 2; no reachability test through desk_add_folder with a faked requester (the requester cannot run on host); worktree submodules were empty, I symlinked them for the host configure and restored them.
