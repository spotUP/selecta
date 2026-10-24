# I218i: folder browser hides files with no recognised format

Cause: sel_browse_list (src/library/local.c) listed every non-hidden file; the engine browse and `library browse` showed them all.

Fix (revised, registry is the single source of truth):
- `sel_browse` (src/library/local.h) carries a name predicate `name_ok(ctx, name)` plus `name_ctx` instead of a flag. NULL = list every file
  (the library scan, which probes content). The library does not depend on the registry.
- `sel_engine_create` installs `browse_name_claimed` (engine.c), which calls `sel_registry_claims_name(registry_count, registry_get, name)`
  in new src/engine/claims.{c,h}. It walks the registered replayers' SEL_RP_EXTENSIONS (comma list) and matches the last extension or the
  Amiga prefix (part before the first dot), case-insensitively. Nothing is cached, so a plugin the machine filtered out never claims a file.
- Placement: the plan said registry.c, but the engine core (selecta_core) does not link selecta_registry (engine.h: the front end hands
  count/get in), so the matcher takes the same count/get pair and lives in selecta_core.
- The hand list `g_extra_names`, `sel_format_known_file` and its declaration are removed.
- Modland table lookup dropped, not kept: a table-only extension no loaded replayer declares would show a file that cannot play, which is the
  defect. The Modland table stays for its own uses (sel_format_by_ext).
- Playlists (sel_playlist_format_from_name) and archives (has_archive_ext) stay visible, decided in local.c is_listed_file, one place.
- No engine registry hook (count/get NULL): no file is claimed, folders/playlists/archives still show.

Not changed: `library browse --names` (I218d helper) lists unfiltered on purpose.

Test: browse-hides-unrecognised-files (tests/library/browse_filter_test.c) drives the engine (LIBRARY_BROWSE, VIEW_SHOW browse) with a
test-controlled registry: an extension only one plugin declares (zzq) shown, unknown hidden, Amiga prefix shown, folder/playlist/archive shown,
case variants, and swapping to a registry without that plugin hides those files again. Sentinel: registry count callback called.

Performance: not measured (no cache added); see result lines appended below.
Results: host + Amiga builds 0 warnings outside third_party (71 / 66 third_party lines); `commands --unimplemented | wc -l` = 8;
browse-hides-unrecognised-files, view-unit, view-tree, amiga-text-ui-vamos, amiga-latin1-name, library/folders/text-ui set: all pass.
Revert check: with name_ok left NULL in sel_engine_create the test fails (sentinel plus hidden-file checks); restored, it passes.
Per-name cost is one strchr/compare pass over each replayer's list (a handful of plugins); no slowness seen, so nothing was cached and no
timing on a large folder was taken (unmeasured).
