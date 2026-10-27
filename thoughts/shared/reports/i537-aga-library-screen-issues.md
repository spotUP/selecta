# I537: issues on the AGA Library screen (font branch, rig 3, 2026-10-27)

Source: Johan's screenshot ~/Pictures/claude-inbox/"CleanShot 2026-10-27 at 09.15.09@2x.png" of rig 3 running
the I526 font build (worktree-agent-aef30ca469ddcc5ed at 8298e73/ca770fb) at 640x256 AGA, described by a subagent.
Johan: "check screenshot and fix all issues".

1. Page title "Library" is covered by the "Clear Library" button: only "Libra" is visible. Header buttons sit on the title row.
2. Rail labels cut with "...": "Your Li...", "Now Pla...", "Song In...". The 160 px rail must fit "Your Library", "Now Playing", "Song Info" in full.
3. Banner buttons "Not now" and "Sync": labels not centred in their buttons.
4. Tab row: "History" is garbled (looks like two layers drawn over each other, dither smear), "Sources" is clipped by the right screen edge ("Sourc"). The tab row overflows 640 px.
5. Statistics strip is cut at the bottom by the transport bar: only "Statistics", "Songs in library" and a half fragment are visible; the value rows are not.
6. Transport bar: "00:30" and "Stopped" use a tiny condensed font next to the 8 px body text; the progress bar is a short line with no end time and no handle; big empty areas at both ends.
7. Low contrast: the play button (grey circle, white outline triangle) and the back/forward arrows (dark circles on dark background).
8. In the mini8 body font "Library" reads like "Libran" (unsure; check the y glyph in the baked mini8 against the source font).
