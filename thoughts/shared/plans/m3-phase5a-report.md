---
status: done
plan: 2026-10-25-m3-library.md
rows: I165, I166
---

# M3 Phase 5a report: file cache, md5 verify, companions

## Built

- `src/library/md5.[ch]`: streaming MD5 (`sel_md5_init/update/hex`).
- `src/library/net/mirrors.[ch]`: `sel_mirrors_fetch_verified` takes a verify callback. A rejected body counts in `sel_mirror_res.rejected` and the next mirror is tried. `sel_mirrors_fetch` is a wrapper with no verify. No second fetch path.
- `src/library/cache.[ch]`: cache path `<cache>/files/<source>/<path>` (`..` rejected), `.part` file, md5 check, rename; cache hit makes no request; per-source mirror sets keep their ranking across fetches; stats `cache_hits fetches companions md5_mismatches mirror_tries`.
- Companions: tag `SEL_RP_COMPANIONS` (0x101B, STR) in `include/selecta/tags.def`. Rule grammar `pattern>companion;...` over the base name, `*` is the leftover stem. nuade declares `mdat.*>smpl.*;*.tfx>*.sam`. `sel_companion_rules` collects the rules from the registry; `sel_cache_fetch_song` fetches song then companions (a missing companion is skipped).
- Engine: `LIBRARY_FETCH` implemented (`sel_library_fetch_fn` hook in `sel_engine_config`). Reply is `LIBRARY_RESULT` with the cache path and file count; all mirrors wrong md5 answers `ERROR md5_mismatch`; other failure `ERROR fetch_failed`; no hook `ERROR no_library`.
- CLI: `apps/selecta-cli/cache_fetch.c` hook for `script`; counters print at the end of a script that used it.
- Tests: T75 `cache-fetch` (`tests/net/cache_fetch.cmake`, `tests/m3/fetch.sel`, `tests/m3/fetch_md5.sel`), fixtures `bad.mirror.test` and `fast.mirror.test/mods/TFMX/test/{mdat,smpl}.song`. Ratchet `tests/ratchet/unimplemented.txt` lowered 18 to 17.

## Test results

- Host build and Amiga 68020 build: clean.
- `cache-fetch`, `mirrors-ranking`, `ftp-transport`, `ratchet`, protocol, registry-tags, abi, nuade-players, rp-config, stage-commands, library tests: all pass. `plugins --lint`: unknown_tags=0 duplicate_tags=0.
- T75 shows: file at `<cache>/files/modland/<path>` with matching md5 and size; bad mirror tried once, rejected, fast mirror serves; `cache_hits=1` on the repeat; `fetches=3 companions=1`; a warm cache run makes 0 requests and 0 transport tries; every-mirror-wrong answers `md5_mismatch` and leaves no file.
- T8 unimplemented count: 18 to 17.

## Open questions

| topic | default | alternative |
|-------|---------|-------------|
| md5 source | `#md5=<hex>` suffix in the spec | look up the md5 in the catalogue by key |
| recovered mismatch visibility | counter only | an ERROR event per rejected body |
| cache dir default | `SELECTA_CACHE_DIR`, then `XDG_CACHE_HOME`/selecta, then `~/.cache/selecta` | sibling of `sel_config_dir` |
| companion scope | rules of all replayers match by file name | only the replayer that claims the song |
| Amiga route in scripts | off (desktop mirrors) | follow `--machine` |

## Left

- The shell (`apps/selecta`) does not install the fetch hook yet; only `script` does.
- Only nuade declares companions; openmpt declares none. Other two-file formats (for example UADE sets) need rules when those replayers exist.
- Ticking I165 and I166 in `thoughts/spot/todos.md` is the caller's job.
