# I161c: stream the Modland listing unpack

Plan: thoughts/shared/plans/2026-10-25-m3-library.md Phase 4 (I161, I161a, T72 network part).

## Changed
- `sel_archive_extract_file` (src/library/sources/archive.c/.h): opens the zip FILE with a miniz read callback
  (miniz here is built with MINIZ_NO_STDIO, so `mz_zip_reader_init_file` does not exist), locates the member by base
  name, and inflates it with `mz_zip_reader_extract_to_callback` straight into the output FILE. No archive or member
  buffer. Respects `max_member`. All miniz allocations go through `host->mem_alloc` with a size header, so the peak is
  measured.
- `unpack_listing` in modland_net.c now calls it; `member_sink`, `member_ctx` and the whole-file read are gone.
- Sentinel: `unpack_peak_heap` in `sel_modland_net_stats`, `net_unpack_peak_heap` in `sel_sync_info`, printed by
  `selecta-cli library sync modland <url>` next to `net_zip_bytes` and `net_listing_bytes`. Fixture run: 34209 bytes for
  a 3845 byte member (the miniz inflate dictionary and read buffers; independent of member size).

## Tests
- `source-modland-net` (tests/net/modland_net.cmake): asserts `net_unpack_peak_heap` in 1..262143 and that the cached
  listing is md5-identical to allmods.txt unzipped from the fixture zip (`cmake -E tar`).
- `archive-stream` (tests/archive_stream_test.c, new): writes an 8 MB member zip at test time, asserts byte-identical
  output, peak heap below 512 KiB, `max_member` rejection and missing member failure.
- Revert check: restoring the old whole-buffer `unpack_listing` fails
  `modland_net.cmake`: `T72 net: net_unpack_peak_heap='0' (want 1..262143, streaming path)`. Restored afterwards.

## Validation
- Host and Amiga (build/amiga, 68020 toolchain) both build with 0 warnings outside third_party. modland_net.c was
  already in selecta_library for the Amiga build and compiles cleanly there; nothing excluded or added.
- `selecta-cli commands --unimplemented | wc -l` = 8.
- `ctest -R modland` 5/5, `ctest -R archive` 2/2 (includes the new archive-stream test).

## Not done
- The Amiga build has SEL_BUILD_TESTS=OFF, so the streaming path is not run on the Amiga tier; only compiled.
- `file_read` seeks with `long` offsets, fine below 2 GB (allmods.zip is a few MB).
