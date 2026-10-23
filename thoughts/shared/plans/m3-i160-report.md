# M3 I160 report: sel_source_rescan and folder watcher

## Done
- sel_source_rescan(cfg, paths, count, stats): flat per-directory rescan reusing scan_dir (src/library/sources/local.c).
- Folder watcher behind src/library/watch.h: FSEvents (src/core/macos/watch.c), inotify (src/core/linux/watch.c), Amiga stub (src/core/amiga/watch.c).
- Amiga path: sel_dir_mtime plus sel_local_track / sel_local_poll_changed / sel_local_untrack (mtime poll), and `library rescan <dir>...` as the manual entry.
- CLI: `library scan --watch <ms>`, `--poll <ms>` (desktop only), `library rescan`.
- Test source-watch (tests/m3/source_watch.sh): bounded wait for watch_ready, add a file to one subdir, assert rescanned_dirs=1, watch_events>=1, db_songs. Same for poll and manual rescan.

## T71 mapping
source-local, source-archive and source-watch. ctest -R source|scan|library|no-float on build/host: 10 of 10 pass. The no-float tests are Amiga-only (not in the host build). commands --unimplemented stays 17. Amiga target (build/amiga) builds with no errors.

## Open
- Linux inotify watcher is not compiled or run on this macOS host.
- No engine or UI periodic timer drives sel_local_poll_changed on Amiga: engine.c is off limits and no command was added. Only the manual and CLI entries exist.
- Rescan adds and updates rows. It does not remove rows for deleted files.
- Tracker caps at 64 directories. Watcher dir table caps at 64, then the root is rescanned.
- Poll mode sees directory entry changes (mtime) only, not in-place file edits.
- No edits to engine.c, view.c or plugins/ui/text.
