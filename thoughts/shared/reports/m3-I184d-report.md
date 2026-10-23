# I184d: local scan and play read MP3 and Ogg through a file-backed stream

Design: thoughts/shared/plans/2026-10-22-architecture-design.md section 9c.

## Changed

- `sel_byte_stream` (include/selecta/replayer.h) gained two optional trailing fields, `total` and `seek` (0/NULL for a live download).
- src/library/stream_open.c: `sel_file_stream_*` opens a local file, returns a head (16 KiB, or ID3v2 tag size + 16 KiB, capped at 1 MiB) for probing, and serves later reads from disk. Global counters `sel_file_stream_opens()` / `sel_file_stream_bytes_read()`.
- src/library/sources/local.c: `scan_streamed` probes the head; a SEL_RP_STREAMING replayer (minimp3, stb_vorbis) scans through the stream, everything else (trackers, PP20, LZX archives) keeps the whole read. Scan stat `bytes_read` is printed by `library scan`.
- src/engine/engine.c: the play path opens plain local files the same way; non-streaming replayers drop the stream and read the file whole.
- minimp3 and stb_vorbis adapters: file mode with rewind, seek, size and duration.
- New script directive `@file_stream_stats` (prints `FILESTREAM opens=N bytes_read=M`).
- Test `file-stream-local` (tests/m3/file_stream.sh): scan `bytes_read` below file size, play through the stream with PLAY + `@render`, opens >= 1, 4410 frames dumped with non-zero energy.

## Sentinel numbers

- MP3 (sine.mp3 x20 = 54320 B, built in the test because every fixture is below the 16 KiB head): scan bytes_read=19466; play bytes_read=49370.
- Ogg (long.ogg, 218781 B): scan bytes_read=147930 (includes a 64 KB tail read for the exact length); play the same.
- Revert check: scan forced to the whole read gave `FAIL mp3: bytes_read=54320 is not below file size 54320`. Play forced to the whole read gave `FILESTREAM opens=0` and `FAIL mp3: play did not open the file stream`. Both restored.

## Found

- Streamed MP3 duration is an estimate from audio bytes and the first frame bitrate (exact for CBR, approximate for VBR; sine.mp3 shows 338 ms instead of 339 ms). A frame walk would read the whole file.
- Ogg scan reads about 68 percent of long.ogg: stb_vorbis open needs the setup headers and the last 64 KB gives the exact length.
- Seek in file mode is discard-decode from the start (Vorbis) or from two frames before the target (MP3); chained Ogg gets no duration.
- Non-streaming files now cost one extra head read before the whole read (counted in bytes_read).
- Pre-existing: `@render_until_end` never emits SONG_ENDED for a streaming replayer (same with the whole-read path); `library-close-cancel` fails on main too.
- Not verified: Amiga vamos scan test (not registered in either build dir here); the Amiga build compiles with 0 warnings outside third_party.
- The engine loudness path and other readers still read whole files separately (unchanged).
