# M3 I180a / I193a report

Worktree /Users/spot/Code/devilbox-selecta, branch feat/selecta-links. Not pushed.

## I180a (done, with open items)
- src/lib/share/selectaLink.ts (new): parseSelectaLink (matches share.c parse_devilbox), openSelectaLink, handleSelectaLinkOnBoot.
- src/App.tsx: import + call in the AppRouter mount effect, next to the existing ?view handler.
- song: modland -> openSong({kind:'modland'}), hvsc -> downloadHVSCFile bytes, other sources -> warning notice. sub -> openSong subsong. Playback starts ('song').
- playlist: matches DJ playlist by id or name, selects it and opens the DJ view; unknown -> notify.warning.
- view: left to the existing ?view handler.
- Test: src/lib/share/__tests__/selectaLink.test.tsx, 2 passed (AppRouter mount -> openSong called once with path and subsong; parser forms). Type-check passes (0 errors).

## Open items
- `t` is MILLISECONDS in the producer (t_ms in share.c), not seconds as the task said. Parsed as tMs.
- t seek NOT wired: DEViLBOX has no seek-by-time API (grep seekTo*: only row/pos seeks). Needs a design call, not invented.
- Test mounts AppRouter with ?embed=pattern-editor to avoid rendering the full App; the link effect still runs on that mount.
- playlist= value is "source/id" in the producer; resolved by id or name against the local DJ store (no shared playlists server-side).

## I193a (STOPPED, no store)
No lyrics data anywhere in DEViLBOX server (grep lyric in server/: zero hits; no lyrics/md5 table in server/src/db/database.ts; only song_loudness, module_genres have hashes). Route not added. Needs a decision: new table + submit route, or source (e.g. LRC from modland/deepsid).
