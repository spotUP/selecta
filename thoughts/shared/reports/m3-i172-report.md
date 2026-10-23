# M3 I172: NOTE and tag edits in the DB, optional DEViLBOX comment push

Plan: `thoughts/shared/plans/2026-10-25-m3-library.md` Phase 6, row I172, test T78.

## Design

- No new `SEL_LIB_REQ_*` id. `SEL_LIB_REQ_USER` (src/library/db/user.c) gained three ops: `SEL_USER_NOTE`,
  `SEL_USER_TAG_EDIT`, `SEL_USER_COUNTS`. Wrappers in db.c: `sel_library_note`, `sel_library_tag_edit`,
  `sel_library_user_counts`.
- NOTE: replaces the row in `notes` (empty text deletes it) and appends a `comment` row to `pending_push`
  (key, payload = text, created). The library never sends anything. No endpoint configuration exists, so nothing
  is ever sent; the queue only grows.
- Tag edit (fields artist, name, album, genre): stored as sticker `tag.<field>` and written to the `songs` row.
  Queries, FTS and display already read `songs`, so the edited value overrides the scanned one in one place.
  The scan upsert (scan.c `SQL_SONG`) and the catalogue import upsert (search.c) re-apply the sticker through the
  macro `SEL_SONG_TAG_KEEP` (db_internal.h), so a rescan or re-import keeps the edit. An empty value removes the
  edit (the sticker only; the songs row keeps the edited value until the next scan restores the scanned one).
- No schema migration (stickers table exists), to avoid migration-version collisions with parallel branches.
- CLI: `selecta-cli userdata --db <p> [--catalogue <tsv>] [--played s] [--note s text] [--tag s field value]
  [--wrong s note]` prints `history_rows= flags= notes= pending_push=`. `sel_cli_open_catalogue_at(tsv, db)`
  added in apps/common/catalogue.c.

## Test (ctest user-data, tests/m3/userdata.sh)

One file DB: played + wrong flag + NOTE + artist edit gives `history_rows=1 flags=1 notes=1 pending_push=1`.
A new process finds the song via `artist:zedtag`, not via the scanned `artist:lizardking`. A catalogue re-import
in a further process keeps the edit, the note and the queue (still 1, nothing sent).
Host build: `ctest -L m3` 30 of 30 pass (includes user-data and play-query).

## Open question (needs the user)

The DEViLBOX comments endpoint is not defined anywhere in the repo (design doc: server work "D9"). No DEViLBOX
client, endpoint setting, auth or payload format exists. So the push executor is NOT built; only the queue exists.
Needed: URL/path, auth, payload schema (key as song id?), retry and dequeue rule.

## Leftovers

- Engine script command `NOTE` not added (would touch commands.def and command-count tests); the library op and
  CLI exist. A script/UI wiring is open.
- scan.c MOVE/rekey path (`UPDATE songs SET key=..., name=, artist=`) does not carry tag stickers to the new key.
- Amiga build not run (portable C, no new dependencies); unverified.
