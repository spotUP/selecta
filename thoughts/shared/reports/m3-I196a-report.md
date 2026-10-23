# I196a + I196c: ICY metadata and directory recording

Design doc: `thoughts/shared/plans/2026-10-22-architecture-design.md`, bullet "Record while playing (F36, I196a/I196b/I196c)".

## What changed

- `OPTION record_to <dir>` names a directory. Each stream OPEN writes a new file there; each changed ICY `StreamTitle` closes the file and starts another (one file per song).
- The download request sends `Icy-MetaData: 1`. `http.c` parses `icy-metaint` and `Content-Type` (and accepts a Shoutcast v1 `ICY 200 OK` status line). `ring_sink` in `src/library/stream_open.c` runs an ICY state machine that removes the metadata blocks from both the decoder ring and the recorded bytes.
- The StreamTitle is the song title: `sel_live_title` (seqlock copy) feeds the engine song snapshot, so `@title` shows it.
- `SEL_OPTION_VALUE_MAX` is 256 (was 64) for all options. Design doc updated (255 characters).
- `selecta-cli play <url> --output <id> --record <dir>` sends `OPTION record_to` before OPEN. `@stream_live` prints a new `record_files=N` counter.
- `selecta-cli record <url> <file>` still writes one file, now with metadata removed, no split, no tag.

## F6 naming: none existed

No F6 record naming or tag-writing code exists in the tree (searched src, include, apps). `src/library/record_name.{c,h}` is the one shared helper: `sel_record_name` (sanitised title as file name, " (2)", " (3)" suffix so nothing is overwritten, directory created one level), `sel_record_ext` (content type, then URL extension, default mp3), `sel_record_id3` (tag block).

## Tag scheme and limits

- MP3 files of an ICY stream begin with a reserved 512-byte ID3v2.3 block with one TIT2 frame. The first file is named after the URL basename until the first title arrives, then renamed and its tag patched.
- No tag for non-ICY streams or non-mp3 content (those files hold the raw bytes only).
- The split is at the metadata block byte position; an MP3 frame can straddle two files.
- Amiga: still a stub for the directory API and title; the Amiga build compiles.
- `play --record` is exercised only on macOS with an output device (exit 77 skips that part; the script part runs everywhere).

## Tests (through selecta-cli)

New `stream-icy` (`tests/net/stream_icy.cmake`, `tests/net/icy_server.py`): request carried `Icy-MetaData: 1`; two files named by the titles, repeated title does not split; audio after the 512-byte tag equals the served audio minus metadata (byte compare); TIT2 holds the title; `@title` shows the ICY title; a second OPEN writes "(2)" files (4 total, `record_files=4`); a 78-character directory works; `play --record` writes both files. `stream-record-play` updated to a directory.

Fail-before, observed: without the `Icy-MetaData` header the test fails ("StreamTitle did not become the song title"); with `SEL_OPTION_VALUE_MAX` 64 it fails ("missing recording").

Ran: `stream-|record|http-client|option` (14 tests) all pass. `commands --unimplemented | wc -l` = 8. `build/amiga` builds.
