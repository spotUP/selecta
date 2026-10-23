# I194a and I194b: exact Ogg length, stream tags

## I194a (commit a665c5e): premise wrong, no decoder change
- `tests/m3/stream/sine.ogg` has a final granule of 6656 (ffprobe `duration_ts=6656`; last page granule read directly).
  ffmpeg pads to whole blocks. The 6615 in the ledger and old test comment was the pre-encode length.
- stb_vorbis already honours the final granule. New fixture `exact.ogg` (sine.ogg with the last page granule patched to
  6615 and the Ogg page CRC recomputed) renders exactly 6615 frames at 22050 Hz and 13230 at 44100 Hz with the
  unmodified plugin. A clamp in `pull()` was tried and reverted as redundant.
- `stream_decode.sh` now asserts exact counts, tolerance 0: sine.ogg 6656/13312, exact.ogg 6615/13230,
  sine.mp3 14976 and 7488.
- Fail-before caveat: the decoder was never wrong, so there is no genuine fail-before for a fix. Demonstrated failure: the
  tightened sine.ogg assertion at 6615 against the unchanged plugin printed `rendered_frames=6656, want 6615 +-0`
  (an assertion against a wrong expected value). `exact.ogg` is the guard that fails if the decoder ever overshoots.

## I194b: tags into song metadata
- `plugins/replayers/stream/id3.h`: header-only reader. ID3v2.3/2.4 TIT2/TPE1/TALB, encodings 0-3 (Latin-1, UTF-16 with
  BOM, UTF-16BE, UTF-8) converted to UTF-8 incl. surrogate pairs; extended header, v2.4 data-length indicator, skips
  compressed/encrypted frames; ID3v1 fills any field still empty.
- minimp3 adapter: reads the tag in `rp_open`, `rp_metadata` returns TITLE, AUTHOR, RELEASE. stb_vorbis adapter: adds
  ALBUM (TITLE and ARTIST existed).
- Album mapping: no `SEL_META_ALBUM` exists, so album is `SEL_META_RELEASE` (already read as album by the lastfm
  plugin). Carried by new `sel_scan_row.album` into `songs.album` (insert and `COALESCE` on conflict).
- `selecta-cli library scan --dump` prints `song=<path>|<name>|<artist>|<album>` from the DB after the scan.
- Test `stream-tags` (`tests/m3/stream_tags.sh`), fixtures `tagged_v23.mp3`, `tagged_v24.mp3`, `tagged_v1.mp3`,
  `tagged.ogg`. Asserts title, artist, album (non-ASCII) through `selecta-cli library scan --dump`.
- Fail-before: with both adapters at HEAD the test fails: `FAIL: tagged_v23.mp3: want |Tést Title|Ärtist Name|Albüm One in:
  song=st_dir/tagged_v23.mp3|tagged_v23.mp3||`. With the change it passes.

## Guards
`stream-decode`, `stream-tags`, `link-warnings`, `source-local`, library tests pass; `commands --unimplemented | wc -l` is 8.
`build/amiga` builds and links, no warnings in the touched files. Not run on the rig.
