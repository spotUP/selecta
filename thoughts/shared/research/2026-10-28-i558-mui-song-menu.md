# I558 MUI song menu gadget (MUI-P4-6) and I637 notes

## Cause
plugins/ui/mui/mui_ui.c, mui_song_menu_items (~line 183): the Song items in the Listview context menu (which==1) had no
shortcut keys; only the window strip (which==0) had "1".."5". MUI swaps the window menu strip for the object's
MUIA_ContextMenu strip while the pointer is over that object. After the rig clicks the list row "meme2.mod" the pointer rests over
the Listview, so the active strip is the context strip and ramiga+2 / 3 / 1 reach nothing (debug trace: mui_song_menu_fire never
called; with the pointer over a door gadget the same keys fired and songmenu=1).

## Fix
Both strips carry the shortcuts 1 to 5 (same ReturnIDs CTL_SM_BASE+i). Real user bug too: shortcuts were dead over the list.

## Rig 3 result
After the fix the failure "songmenu gadget click did not send its line" is gone; the test now fails later at
"ui click Selecta Rename Playlist exited 1" (Playlists door step, MUI-P4-5), a separate issue, not investigated.
Revert check NOT run yet: rig 3 stopped on coordinator PAUSE (Johan recording). Run after RESUME: revert the 2 lines, expect songmenu=0.

## I637 (song list columns collapsed)
Not started (rig paused). Next: grep MUIA_List_Format in mui_ui.c, read the column layout from the rig as text.
