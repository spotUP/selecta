---
date: 2026-10-23
topic: Modland real upstream format and modland_sync.c bugs (I161a attempt 1)
status: findings only, no code
---

# I161a attempt 1: findings

The first I161a agent wrote no code. It ran out of its tool budget on sandbox refusals. These facts were checked against the live server on 2026-10-23, and the two bugs were confirmed by reading `src/library/sources/modland_sync.c` on main (d2acc71).

## Real Modland layout

- `allmods_md5.zip` and `allmods_md5.txt` return 404 at `/`, `/pub/` and `/pub/modules/`.
- The real listing is `http://ftp.modland.com/allmods.zip`. Its single member is `allmods.txt` (30.7 MB unpacked).
- Each line is `<size>\t<path below pub/modules>`. There is no md5 column, so the listing cannot supply md5s.
- Changelogs live at `/pub/changelogs/changelog_YYYY-MM-DD.txt`, with an HTML index at `/pub/changelogs/`. Some dates return 404.
- Real changelog lines are indented, for example `  added: path` and `     to: dir`.

## Bugs in modland_sync.c (confirmed)

1. `apply_changelog` compares the text before `:` with `strcmp(line, "added")` and never skips leading whitespace. Every indented real changelog line is ignored.
2. `apply_move` calls `remove_path(from)` and then `add_path(dest)`, and `add_path` takes no md5. A moved row loses its md5. The ledger row requires that moved rows keep their md5.

## Design proposed by the agent (not built)

- New `modland_net.c/.h` fetches the listing and the changelogs.
- `cmd_LIBRARY_SYNC` accepts a URL.
- The md5 is carried through `sel_remove_batch`, with COALESCE upserts.
- A `meta` request stores `new_this_week`.
- Fixtures `modland1.test` and `modland2.test`, plus a ctest named `source-modland-net`.
