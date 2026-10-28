# MUI track A, 2026-10-28 (rig 3)

## I558 song menu shortcuts: NOT CONFIRMED
- Run 1 (fix 7360e72f present, ctest rig-m4-mui-open): failed earlier, `ami.py ui;click;Selecta;meme2.mod exited 1`.
- Run 2 (fix present, binary built at HEAD + fix): `RED[M4MUI]: the songmenu gadget click did not send its line`, ui_p4 line shows `songmenu=0`. With the fix present the rig check still reads songmenu=0, so the revert check cannot discriminate and the fix is not proven by the rig. Open: whether the menushort steps ran before the click, and whether the rig delivers keys 1-5 over the list.
- Revert check not run (budget).

## I637 collapsed list columns: edited, unverified
mui_list.c column weights (W=40,W=25,W=25), rig regex updated. Not rebuilt, host mui- tests not run, no visual check. Needs Amiga rebuild plus a look at the song list.

## I640 Rename Playlist exited 1: not reproduced
Runs at HEAD click Rename Playlist fine (button at 228,55, tool row 3 after 4f78309d). Hypothesis (unverified): before the row split the button sat off the 800 px window.

## Test robustness
run_rig.cmake exit poll re-sends Break each round (first Break after Settings/Help windows was swallowed once).

## Registered-MUI rerun (rig 3, build of merged HEAD + d24e96b5)
- Host `ctest -R '^mui-'`: 66 of 67 pass; only mui-relayout (I639, known) red. mui-columns was red after d24e96b5 (test expected a trailing space the last column does not have); fixed in tests/ui/mui_contract_test.c.
- rig-m4-mui-open (rig 3, real song virgill-that_meme_again.mod): FAILED, single RED: songmenu gadget click did not send its line, ui_p4 shows songmenu=0 (still, with fix 7360e72f present and MUI registered). So registration was not the cause.
- I640: prename=1, popen=1, Rename Playlist button clicked at 228,55 ten times without exit; rename step PASSES.
- I637: exit stats show `ui_list ... cols=3 format=W=40 BAR,W=25 BAR,`; no column RED. The rig test has no screenshot step, so the bars are NOT visually confirmed (Selecta has exited when the test ends; rig.py has no screenshot command).
- Revert check not run (tool budget). Open: why the songmenu click is not counted although the menu path works in host tests.
