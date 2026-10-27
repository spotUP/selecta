# I531 rig 2 check: Add folder on the Amiga AGA desktop

Build: worktree at main a7ac034, `cmake -S . -B build/amiga-aga -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/amiga-68020.cmake -DSEL_DESKTOP_AGA=ON` (icon/font bakers from the main host build, C flags `-m68020 -mcrt=nix20 -fomit-frame-pointer`), `ninja selecta` rc=0, 19539568 bytes. Scratch tree deleted afterwards.
Rig 2 restarted with purpose "Selecta session: I531 Amiga add folder check", `rig.py aga` (native AGA Workbench), files put with ami.py to SYS:selecta-test (Selecta, mods/ with the 4 tests/m3/genre mods, aga.script). Command (aga.script): `Stack 200000; SYS:selecta-test/Selecta --ui desktop --backend aga --format index8 --view library --stats --log RAM:aga.log --db SYS:selecta-test/library.db >RAM:aga-out.txt`, started with `Run >NIL: Execute`.

## Step 1 build and deploy: PASS
`windows`: `screen "Selecta"  640x256, 8 bit` / `window "" at 0,0 640x512 task SYS:selecta-test/Selecta [active, backdrop]`. Library empty: Songs in library 0.

## Step 2 Add folder opens ASL requester, drawers only: PASS
Mouse click on Add Folder (grab 529,29). `windows`: `window "Add folder"  at 30,40  318x356  task SYS:selecta-test/Selecta  [active]`.
`ui list`: gadgets OK, Volumes, Parent, Cancel and ONE string `"Drawer"`; no File string gadget (drawers only); list gadget id 12.
(The first click while a wasabi snoop stream ran made wasabid unresponsive for about 90 s and did not open the requester; the second click, without snoop, worked. Snoop was not needed after that.)

## Step 3 choose a drawer, scan runs, songs appear: PASS
`ui set Drawer "SYS:selecta-test/mods"`, `ui click OK`. Requester closed, a "7 sources not synced" banner appeared. Statistics still showed 0 after about 12 s, then after clicking the Files tab: `Songs in library 4` and a `mods  Folder` row (the 4 files in the drawer). The library.db on the rig exists (151552 bytes). Evidence is the on-screen count 4 (screenshots); the Selecta log could not be read (RAM:aga.log held open: "Error 202: Object in use") and the DB cannot be read while Selecta runs.
Observation, not part of I531: the Songs count did not refresh on its own after the scan finished; it updated only after a tab click. Possible stale statistics redraw; unverified cause.

## Step 4 Add folder, Cancel: PASS
Second click on Add Folder opened `window "Add folder"`; `ui click Cancel`. Afterwards `windows` shows only the Selecta backdrop window; a screenshot taken ~2 s later shows the same screen as before (Files tab, 4 songs, no toast, no "No folder dialog here"). Toast absence relies on one screenshot, a short toast could have been missed (unverified).

## Step 5 rig state left
Rig 2 RUNNING, native AGA Workbench, Selecta open full screen (screen "Selecta" 640x256) on Your Library, Files tab, 4 songs from SYS:selecta-test/mods. All snoop/debug streams ended (the 90 s snoop expired at 02:54:31). To try: click the Add Folder button at the top right of the Library page (grab coordinates 529,29), requester "Add folder" opens, pick a drawer, OK. In this session the requester was driven with `wasabi --port 1235 --key rigwasabi ui set Drawer <path>` and `ui click OK|Cancel`.
