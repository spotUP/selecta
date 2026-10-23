# I198e: cap the one-letter per-row scan

- search.c collect(): a scan with no FTS term (only one-letter words) stops after job->scan_max keys and sets
  job->truncated = 1 (at least one more; the rest is not counted, count_rest() walked every row again).
- engine.c run_search(): scan_max = queue capacity (256 / 2048 / 8192 by RAM tier), the existing result limit.
- Existing surface: the engine's result_truncated log event; selecta-cli library search now prints truncated=<n>
  when it is non-zero.
- Test library-search-scan-cap (tests/m3/library_scan_cap.sh): 400 rows, a500, query q -> src_count=256, 256 keys,
  truncated=1. Revert check: scan_max = 0 -> test FAILED; restored -> passes.
- Vamos, 516000-row synthetic index, a1200: query_ms=500 (was ~100000), 256 hits, truncated=1.
- Builds: build/host and build/amiga 0 warnings outside third_party; commands --unimplemented = 8.
- Open: one unreproduced ERROR out_of_memory on the first vamos run against a fresh user.db.
