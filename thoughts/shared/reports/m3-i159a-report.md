# I159a report: LZX, PowerPacker, archive browse

## Done
- `sources/archive_lzx.c`: Amiga LZX reader (entry header CRC, data CRC32, stored mode, LZX mode 2 with verbatim and aligned blocks, merged groups). Own code, nothing vendored.
- `sources/archive_pp20.c`: PowerPacker PP20 decrunch. Own code.
- `archive.c/.h`: detection of LZX and PP20, list-only walk (`sel_archive_list`), dispatch for zip, lha, lzx.
- Scan: LZX archives scan as folders (`archive_lzx` counter), PP20 files depack and scan as one song (`depack_pp`).
- LIBRARY_BROWSE: .zip .lha .lzh .lzx files list as folders (`is_dir == 2`; scanner descends only into `is_dir == 1`); members list at any inner folder, sub-folders are synthesised.
- Test `source-archive` (driven through selecta-cli): zip, lzh, stored lzx, compressed lzx (merged group), PP20, broken zip, browse of all four archives. Green on host.

## Licenses
`ancient` (BSD-2) was used as a format reference only. No third-party code added.

## Not done / unverified
- XPK container and the other `ancient` formats: not implemented.
- Native Amiga xad/xfd path: left out.
- Fixtures use literal-only LZX and PP20 streams. LZX match, aligned-block and PP20 match paths are not verified against real archives.
- Amiga build not run.
- Pre-existing, unrelated: `selecta` shell binary not built in this worktree, so `view-library-source` and `selecta-browse-play` fail here.
