# Desktop UI milestone (M3d, plan 2026-10-24-desktop-ui.md section 5). tests/dui/items.txt is "<id> <test name>" per
# item. Every item without a real test here is registered red by dui_red.cmake; the commit that implements an item
# replaces its red entry with the real test and removes its id from tests/ratchet/unimplemented.txt. Red entries also
# carry label dui-red, so a run of the finished milestones (M3 finish line) can leave them out with -LE dui-red while
# dui-ratchet keeps the red count honest.
set(SEL_DUI_REAL dui-font-baked dui-ratchet dui-build-deps dui-uim-frame dui-ui-switch dui-ui-multi dui-mode-pref dui-lift-parity dui-actions dui-layout dui-breakpoints dui-parity-matrix dui-governor dui-tokens dui-draw dui-dirty dui-text dui-artwork dui-genart dui-golden-home dui-sdl dui-widgets dui-hidpi dui-anim dui-keys dui-mouse dui-tooltips dui-gestures dui-frame dui-home dui-library-retired dui-styles dui-grid-coverflow dui-search dui-search-view-mode dui-browse-tiles dui-nowplaying dui-queue dui-playlists dui-songinfo dui-overlays dui-nav dui-viz dui-tracker dui-drop dui-m68k-nofloat dui-hover-live dui-hover-rows dui-hover-sidebar-ink dui-hover-instant dui-hover-nowplaying dui-hover-regions dui-library-reload dui-library-unknown-composer dui-song-menu dui-playlist-page dui-library-remove dui-aga-budget dui-library-style dui-coverflow-slide dui-stats-panel dui-topbar dui-show-all dui-settings)

add_test(NAME dui-ratchet
  COMMAND "${CMAKE_COMMAND}"
    "-DPLAN=${PROJECT_SOURCE_DIR}/thoughts/shared/plans/2026-10-24-desktop-ui.md"
    "-DRATCHET=${PROJECT_SOURCE_DIR}/tests/ratchet/unimplemented.txt"
    "-DFIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/fixture_library.sql"
    "-DBUILD_DIR=${PROJECT_BINARY_DIR}" "-DCTEST=${CMAKE_CTEST_COMMAND}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/dui_ratchet.cmake")
set_tests_properties(dui-ratchet PROPERTIES LABELS dui)

# The nested configures share nothing with this build tree; serial so only one runs at a time.
add_test(NAME dui-build-deps
  COMMAND "${CMAKE_COMMAND}" "-DSRC=${PROJECT_SOURCE_DIR}" "-DWORK=${PROJECT_BINARY_DIR}/dui-build-deps"
    "-DGEN=${CMAKE_GENERATOR}" "-DCMAKE=${CMAKE_COMMAND}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/dui_build_deps.cmake")
set_tests_properties(dui-build-deps PROPERTIES LABELS dui RUN_SERIAL TRUE)

# Primitive unit test of the rasterizer, run by dui-draw (DUI-P2-1).
add_executable(dui_draw_unit draw_unit.c)
# I292b: spectrum over the whole ring, peak falloff.
add_executable(dui_viz_spectrum_unit viz_spectrum_unit.c)
target_link_libraries(dui_viz_spectrum_unit PRIVATE selecta_core m)
add_test(NAME dui-viz-spectrum-unit COMMAND dui_viz_spectrum_unit whole falloff)
set_tests_properties(dui-viz-spectrum-unit PROPERTIES LABELS viz TIMEOUT 60)
target_link_libraries(dui_draw_unit PRIVATE selecta_gui)

# The shell-driven tests (DUI-P1-1 to P1-4) run the real selecta binary on the generated modules.
if(NOT SEL_AMIGA)
  add_test(NAME dui-font-baked
    COMMAND "${CMAKE_COMMAND}" "-DSELECTA=$<TARGET_FILE:selecta>" "-DSONGS=${PROJECT_BINARY_DIR}/tests/songs"
      "-DWORK=${PROJECT_BINARY_DIR}/tests/dui-font-baked" "-DBAKED_UNIT=$<TARGET_FILE:dui_font_baked_unit>"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/font_baked.cmake")
  file(MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/tests/dui-font-baked.tmp")
  set_tests_properties(dui-font-baked PROPERTIES LABELS dui TIMEOUT 120 ENVIRONMENT "TMPDIR=${PROJECT_BINARY_DIR}/tests/dui-font-baked.tmp")
endif()
if(NOT SEL_AMIGA)
  # ui-stat-tags (I603): no two SEL_UI_STAT_* tags share a value (PT_MS_OVERLAY and LIST_VIEW did).
  add_test(NAME ui-stat-tags COMMAND "${CMAKE_COMMAND}" "-DTAGS_DEF=${PROJECT_SOURCE_DIR}/include/selecta/tags.def"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/stat_tags.cmake")
  set_tests_properties(ui-stat-tags PROPERTIES LABELS ui-stat-tags TIMEOUT 30)
endif()
if(NOT SEL_AMIGA)
  # dui-amiga-font (I526-P1): the bitmap .font reader, unit level (host only).
  # cover_unit (I691a): platform family resolver, tones and AGA palette of the platform covers, unit level.
  add_executable(cover_unit cover_unit.c)
  target_link_libraries(cover_unit PRIVATE selecta_gui selecta_uimodel selecta_library)
  add_test(NAME cover_unit COMMAND cover_unit)
  set_tests_properties(cover_unit PROPERTIES LABELS cover TIMEOUT 60) # ledger row, not a plan item: no dui label
  add_test(NAME dui-amiga-font COMMAND dui_amiga_font_unit)
  set_tests_properties(dui-amiga-font PROPERTIES LABELS amiga-font TIMEOUT 60) # ledger row, not a plan item: no dui label, dui-ratchet counts dui against the plan ids
  # dui-font-tall (I538): AGA atlases baked for 1:2 pixels, SDL atlases unchanged.
  add_test(NAME dui-font-tall COMMAND dui_font_tall_unit)
  set_tests_properties(dui-font-tall PROPERTIES LABELS font-tall TIMEOUT 60) # ledger row, not a plan item: no dui label, dui-ratchet counts dui against the plan ids
  # dui-font-roles (I526-P4): the role table and the 6-row fallback, unit level.
  add_test(NAME dui-font-roles COMMAND dui_font_roles_unit)
  set_tests_properties(dui-font-roles PROPERTIES LABELS font-roles TIMEOUT 60) # ledger row, not a plan item: no dui label, dui-ratchet counts dui against the plan ids
  # dui-art-async (I906c): art_load does not join; a busy library worker costs the UI thread no time.
  add_test(NAME dui-art-async COMMAND dui_art_async_unit)
  set_tests_properties(dui-art-async PROPERTIES LABELS lib-ui-art TIMEOUT 60 PASS_REGULAR_EXPRESSION "art_async_ok") # ledger row, not a plan item: no dui label
endif()
if(NOT SEL_AMIGA)
  foreach(spec "dui-default-ui:default_ui" "dui-uim-frame:uim_frame" "dui-ui-switch:ui_switch" "dui-ui-multi:ui_multi" "dui-mode-pref:mode_pref" "dui-actions:actions" "dui-layout:layout" "dui-breakpoints:breakpoints" "dui-governor:governor" "dui-tokens:tokens" "dui-draw:draw" "dui-dirty:dirty" "dui-text:text" "dui-artwork:artwork" "dui-artwork-late-album:artwork_late_album" "dui-cover-tiles:cover_tiles" "dui-platform-covers:platform_covers" "dui-genart:genart" "dui-golden-home:golden_home" "dui-sdl:sdl" "dui-widgets:widgets" "dui-hidpi:hidpi" "dui-anim:anim" "dui-keys:keys" "dui-mouse:mouse" "dui-tooltips:tooltips" "dui-gestures:gestures" "dui-frame:frame" "dui-home:home" "dui-library-retired:library_retired" "dui-styles:styles" "dui-grid-coverflow:grid_coverflow" "dui-search:search" "dui-search-view-mode:search_view_mode" "dui-browse-tiles:browse_tiles" "dui-browse-modland-tile:browse_modland_tile" "dui-search-catalogue:search_catalogue" "dui-nowplaying:nowplaying" "dui-queue:queue" "dui-queue-scroll:queue_scroll" "dui-playlists:playlists" "dui-songinfo:songinfo" "dui-overlays:overlays" "dui-nav:nav" "dui-viz:viz" "dui-tracker:tracker" "dui-drop:drop" "dui-footer:footer" "dui-footer-one-row:footer_one_row" "dui-screen:screen" "dui-hover-live:hover_live" "dui-hover-rows:hover_rows" "dui-hover-sidebar-ink:hover_sidebar_ink" "dui-hover-instant:hover_instant" "dui-hover-nowplaying:hover_nowplaying" "dui-hover-regions:hover_regions" "dui-scan-ui:scan_ui" "dui-library-reload:library_reload" "dui-library-follow-keeps-place:library_follow_keeps_place" "dui-library-unknown-composer:library_unknown_composer" "dui-song-menu:song_menu" "dui-library-remove:library_remove" "dui-song-menu-live:song_menu_live" "dui-song-menu-lists:song_menu_lists" "dui-select-lists:select_lists" "dui-song-menu-multi-playlist:song_menu_multi_playlist" "keys-live:keys_live" "dui-aga-budget:aga_budget" "dui-library-style:library_style" "dui-stats-panel:stats_panel" "dui-home-cover-shadow:home_cover_shadow" "dui-coverflow-slide:coverflow_slide" "dui-view-exec-order:view_exec_order" "dui-zoom:zoom" "dui-list-keys:list_keys" "dui-library-scroll:library_scroll" "dui-library-scroll-drag:library_scroll_drag" "dui-sources-counters:sources_counters" "dui-sync-banner:sync_banner" "dui-sync-offer-keys:sync_offer_keys" "dui-sync-nowait:sync_nowait" "dui-sync-card-front:sync_card_front" "dui-sync-bar:sync_bar" "dui-sync-offer-idle:sync_offer_idle" "dui-tall-pixels:tall_pixels" "dui-aga-button-polish:aga_button_polish" "dui-aga-native-text:aga_native_text" "window-raise:window_raise" "dui-idle-turns:idle_turns" "dui-start-home:start_home" "dui-home-tile-tint:home_tile_tint" "dui-sidebar-library:sidebar_library" "dui-sidebar-mixed:sidebar_mixed" "dui-sidebar-recents:sidebar_recents" "dui-sidebar-chips:sidebar_chips" "dui-sidebar-density:sidebar_density" "dui-playlist-page:playlist_page" "dui-quit-dialog:quit_dialog" "dui-home-copper:home_copper" "dui-topbar:topbar" "dui-show-all:show_all" "dui-topbar-doors:topbar_doors" "dui-nowplaying-panel:nowplaying_panel" "dui-collection-page:collection_page" "dui-play-context:play_context" "dui-kind-pages:kind_pages" "dui-source-drill:source_drill" "dui-click-map:click_map" "dui-playing-mark:playing_mark" "dui-hover-motion:hover_motion" "dui-home-shelves:home_shelves" "dui-home-item-art:home_item_art" "dui-aga-library:aga_library" "dui-search-dblclick:search_dblclick" "dui-search-artist-click:search_artist_click" "dui-search-enter-focus:search_enter_focus" "dui-sidebar-async:sidebar_async" "dui-sources-first-paint:sources_first_paint" "dui-sources-repaint:sources_repaint" "dui-frame-cost:frame_cost" "dui-lib-ui-guard:lib_ui_guard" "dui-files-tab-first-frame:files_tab_first_frame" "dui-collection-open-touch:collection_open_touch" "dui-collection-titles-touch:collection_titles_touch" "dui-song-panel-art-async:song_panel_art_async" "dui-song-total-async:song_total_async" "dui-stats-nowait:stats_nowait" "dui-direct-calls-async:direct_calls_async" "dui-composer-settle:composer_settle" "dui-nav-turn-cost:nav_turn_cost" "dui-paste:paste" "dui-output-device:output_device" "dui-toast-fit:toast_fit" "dui-row-text-widths:row_text_widths" "dui-live-refresh-source:live_refresh_source" "dui-list-key-frames:list_key_frames" "dui-viz-tip-flicker:viz_tip_flicker" "dui-bar-nowplaying:bar_nowplaying" "dui-viz-falloff-live:viz_falloff_live" "dui-sidebar-chips-wide:sidebar_chips_wide")
    string(REPLACE ":" ";" pair "${spec}")
    list(GET pair 0 tname)
    list(GET pair 1 script)
    add_test(NAME ${tname}
      COMMAND "${CMAKE_COMMAND}" "-DSELECTA=$<TARGET_FILE:selecta>" "-DSONGS=${PROJECT_BINARY_DIR}/tests/songs" "-DBUILD_TYPE=${CMAKE_BUILD_TYPE}"
        "-DWORK=${PROJECT_BINARY_DIR}/tests/${tname}" "-DGOLDEN=${PROJECT_SOURCE_DIR}/tests/golden" "-DSQLITE3=$<TARGET_FILE:sel_sqlite3_shell>"
        "-DSDL_BUILT=${SELECTA_UI_DESKTOP}" "-DSID_MODULES=${PROJECT_BINARY_DIR}/tests/fixtures/plugin_sidfeed/mod" "-DGENMOD=$<TARGET_FILE:gen_testmod>" "-DUNIT=$<TARGET_FILE:dui_draw_unit>" "-DTAGGED=${PROJECT_SOURCE_DIR}/tests/m3/stream" "-DGUISRC=${PROJECT_SOURCE_DIR}/src/gui" "-DGUIHDR=${PROJECT_SOURCE_DIR}/include/selecta/gui" -P "${CMAKE_CURRENT_SOURCE_DIR}/${script}.cmake")
    # Each test owns its default control socket path ($TMPDIR/selecta-<uid>.sock): two tests sharing
    # the per-user default collide under ctest -j ("selecta: already running"). TMPDIR, not
    # SELECTA_SOCKET, because SELECTA_SOCKET also forces the socket on and keeps --exec runs alive.
    # I962: dui_require_sdl() (dui_run.cmake) prints SELECTA_SKIP on a build without the desktop UI (SELECTA_UI_DESKTOP=OFF, e.g. build/host): ctest reports Skipped, not Passed. SDL build dir: build/sdl (RULES.md ## Commands).
    set_property(TEST ${tname} PROPERTY SKIP_REGULAR_EXPRESSION "SELECTA_SKIP")
    set(tmpdir "${PROJECT_BINARY_DIR}/tests/${tname}.tmp")
    file(MAKE_DIRECTORY "${tmpdir}")
    # dui-nav is ledger row I262, not a plan item: label nav, not dui or dui-extra: dui-ratchet counts tests whose label matches the regex dui against the plan ids.
    set(tlabel dui)
    if(tname STREQUAL "dui-nav" OR tname STREQUAL "dui-footer" OR tname STREQUAL "dui-footer-one-row" OR tname STREQUAL "dui-screen" OR tname STREQUAL "dui-scan-ui") # dui-footer is ledger row I312, not a plan item
      set(tlabel nav)
    elseif(tname STREQUAL "dui-song-menu-live" OR tname STREQUAL "dui-song-menu-lists") # I371: more cases of DUI-P4-16, whose one dui test is dui-song-menu; label song-menu keeps dui-ratchet at one test per plan id
      set(tlabel song-menu)
    elseif(tname STREQUAL "dui-select-lists" OR tname STREQUAL "dui-song-menu-multi-playlist") # I632d: multi-select on the desktop lists, a case of the multi-select plan; label select-lists keeps dui-ratchet at one test per plan id
      set(tlabel select-lists)
    elseif(tname STREQUAL "dui-search-dblclick") # I714: double-click plays a Search song row, not a plan item; label search-dblclick keeps dui-ratchet at one test per plan id
      set(tlabel search-dblclick)
    elseif(tname STREQUAL "dui-search-artist-click") # I1207: a click on a Search Artists row opens the artist page; ledger row, not a plan item
      set(tlabel search-artist-click)
    elseif(tname STREQUAL "dui-default-ui") # I1023: the machine default UI of a launch with no --ui, not a plan item
      set(tlabel default-ui)
    elseif(tname STREQUAL "dui-queue-scroll") # I258: the wheel scroll of DUI-P4-8, whose one dui test is dui-queue; not a plan item
      set(tlabel queue-scroll)
    elseif(tname STREQUAL "dui-list-keys") # I423: list keys, a case of DUI-P3-3 (dui-keys); label list-keys keeps dui-ratchet at one test per plan id
      set(tlabel list-keys)
    elseif(tname STREQUAL "dui-click-map") # I678h: the route table of the desktop UI (design doc 2026-10-29 section 9), not a plan item
      set(tlabel click-map)
    elseif(tname STREQUAL "dui-tall-pixels") # I468 I457: AGA 640x256 tall-pixel layout, not a plan item
      set(tlabel tall-pixels)
    elseif(tname STREQUAL "dui-aga-button-polish") # I588: AGA transport glyph and button edge, not a plan item
      set(tlabel aga-button-polish)
    elseif(tname STREQUAL "dui-home-cover-shadow") # I507: a case of DUI-P4-2 (dui-home); label home-cover-shadow keeps dui-ratchet at one test per plan id
      set(tlabel home-cover-shadow)
    elseif(tname STREQUAL "window-raise") # I501: the SDL window raises on open; not a plan item, so no dui label
      set(tlabel window-raise)
    elseif(tname STREQUAL "dui-coverflow-slide") # I400: the cover flow glide, a case of DUI-P4-5 (dui-grid-coverflow)
      set(tlabel coverflow-slide)
    elseif(tname STREQUAL "dui-library-retired") # I678i: the Library page retired; stands in for plan item DUI-P4-3, so it keeps the dui label
      set(tlabel dui)
    elseif(tname STREQUAL "dui-topbar" OR tname STREQUAL "dui-search-view-mode" OR tname STREQUAL "dui-browse-tiles" OR tname STREQUAL "dui-browse-modland-tile" OR tname STREQUAL "dui-show-all" OR tname STREQUAL "dui-playlist-page" OR tname STREQUAL "dui-artwork-late-album" OR tname STREQUAL "dui-cover-tiles" OR tname STREQUAL "dui-platform-covers" OR tname STREQUAL "dui-home-shelves" OR tname STREQUAL "dui-home-item-art" OR tname STREQUAL "dui-frame-cost" OR tname STREQUAL "dui-paste" OR tname STREQUAL "dui-search-enter-focus") # I615 I615b I615c I617 I633 I689 I692 I691b I678j I999 I1009: ledger rows, not plan items; their own label keeps dui-ratchet at one test per plan id
      string(REPLACE "dui-" "" tlabel "${tname}")
    elseif(tname STREQUAL "dui-sync-banner") # I527: the sync-on-launch banner on the Library page, a case of DUI-P4-3 (dui-library)
      set(tlabel sync-banner)
    elseif(tname STREQUAL "keys-live") # I268: real SDL key events through desk_sdl_poll; not a plan item, so no dui label
      set(tlabel keys-live)
    elseif(tname STREQUAL "dui-aga-native-text" OR tname STREQUAL "dui-list-key-frames" OR tname STREQUAL "dui-idle-turns" OR tname STREQUAL "dui-library-scroll" OR tname STREQUAL "dui-library-scroll-drag" OR tname STREQUAL "dui-sources-counters" OR tname STREQUAL "dui-sync-offer-keys" OR tname STREQUAL "dui-quit-dialog") # I526 I525 I541 I549 I546: ledger rows, not plan items; their own label keeps dui-ratchet at one test per plan id
      string(REPLACE "dui-" "" tlabel "${tname}")
    elseif(tname STREQUAL "dui-view-exec-order") # I398: --view enters its page before --exec runs; ledger row, not a plan item
      set(tlabel view-exec-order)
    elseif(tname STREQUAL "dui-home-tile-tint") # I600: idle Home cards have no dark fill over the tint, a case of DUI-P4-2 (dui-home); not a plan item
      set(tlabel home-tile-tint)
    elseif(tname STREQUAL "dui-home-copper") # I628: Home background and generated covers ride the AGA copper; ledger row, not a plan item
      set(tlabel home-copper)
    elseif(tname STREQUAL "dui-topbar-doors") # I615a: Home and Search leave the sidebar where the top bar owns them; ledger row, not a plan item
      set(tlabel topbar-doors)
    elseif(tname STREQUAL "dui-nowplaying-panel") # I619: the Now Playing panel's context name, add button and hide toggle; ledger row, not a plan item
      set(tlabel nowplaying-panel)
    elseif(tname STREQUAL "dui-sidebar-mixed") # I678b: the sidebar draws the mixed Your Library list with pinned rows; ledger row, not a plan item
      set(tlabel sidebar-mixed)
    elseif(tname STREQUAL "dui-sidebar-density") # I678d: four densities, expanded table, first-playlist card below the pinned rows; ledger row, not a plan item
      set(tlabel sidebar-density)
    elseif(tname STREQUAL "dui-aga-library") # I678k: the bitmap tier library (kind marks, K chip cycle, compact only, 64 px header art); ledger row, not a plan item
      set(tlabel aga-library)
    elseif(tname STREQUAL "dui-kind-pages") # I678f: album, artist and composer open as the collection page with their kind; ledger row, not a plan item
      set(tlabel kind-pages)
    elseif(tname STREQUAL "dui-sources-repaint") # I826: a SOURCES answer after the page switch repaints the page with no input; ledger row, not a plan item
      set(tlabel sources-repaint)
    elseif(tname STREQUAL "dui-hover-motion") # I1067: a pointer move onto a song row hovers it with no wheel turn; ledger row, not a plan item
      set(tlabel hover-motion)
    elseif(tname STREQUAL "dui-playing-mark") # I690: a song list marks the playing row and the mark follows Next; ledger row, not a plan item
      set(tlabel playing-mark)
    elseif(tname STREQUAL "dui-collection-page") # I678e: a playlist opens as the collection page through COLLECTION_OPEN; ledger row, not a plan item
      set(tlabel collection-page)
    elseif(tname STREQUAL "dui-play-context") # I994e: a collection-page row click sends PLAY_CONTEXT; ledger row, not a plan item
      set(tlabel play-context)
    elseif(tname STREQUAL "dui-search-catalogue") # I1053a: archive sources answer Search from the catalogue FTS, Not synced plus Sync; ledger row, not a plan item
      set(tlabel search-catalogue)
    elseif(tname STREQUAL "dui-output-device") # I1039: the devices button and picker; ledger row, not a plan item
      set(tlabel output-device)
    elseif(tname STREQUAL "dui-sidebar-recents") # I678c: Recents order follows last play or page open through LIBRARY_TOUCH; ledger row, not a plan item
      set(tlabel sidebar-recents)
    elseif(tname STREQUAL "dui-sidebar-chips") # I678b: the six kind chips filter through the kind mask, X clears; ledger row, not a plan item
      set(tlabel sidebar-chips)
    elseif(tname STREQUAL "dui-sidebar-chips-wide") # I1208: the expanded sidebar shows the chip row; ledger row, not a plan item
      set(tlabel sidebar-chips-wide)
    elseif(tname STREQUAL "dui-sidebar-library") # I608: the sidebar Your Library list; ledger row, not a plan item
      set(tlabel sidebar-library)
    elseif(tname STREQUAL "dui-sidebar-async") # I787: the sidebar list read runs off the drawing thread; ledger row, not a plan item
      set(tlabel sidebar-async)
    elseif(tname STREQUAL "dui-sync-card-front") # I806: the sync card stays above region repaints; ledger row, not a plan item
      set(tlabel sync-card-front)
    elseif(tname STREQUAL "dui-sync-bar") # I1026: the sync card's progress bar; ledger row, not a plan item
      set(tlabel sync-bar)
    elseif(tname STREQUAL "dui-sync-offer-idle") # I789: the sync offer paints in an idle live window; ledger row, not a plan item
      set(tlabel sync-offer-idle)
    elseif(tname STREQUAL "dui-sync-nowait") # I782: the sync card and Cancel while a slowed sync runs; ledger row, not a plan item
      set(tlabel sync-nowait)
    elseif(tname STREQUAL "dui-row-text-widths") # I1079c: collection page row text takes its column share of the real row width; ledger row, not a plan item
      set(tlabel row-text-widths)
    elseif(tname STREQUAL "dui-library-follow-keeps-place") # I1086: a library change refreshes in place and never navigates; ledger row, not a plan item
      set(tlabel library-follow-keeps-place)
    elseif(tname STREQUAL "dui-live-refresh-source") # I1077a: an open source page follows a sync it did not start; ledger row, not a plan item, so no dui label (dui-ratchet counts dui against the plan ids)
      set(tlabel live-refresh-source)
    elseif(tname STREQUAL "dui-bar-nowplaying") # I960d: a click on the player bar cover or title opens the Now Playing page; ledger row, not a plan item
      set(tlabel bar-nowplaying)
    elseif(tname STREQUAL "dui-toast-fit") # I1046: the toast box sizes to its text and wraps; ledger row, not a plan item
      set(tlabel toast-fit)
    elseif(tname STREQUAL "dui-source-drill") # I790: the source page opens its drill level by level, rows built stay in the window; ledger row, not a plan item
      set(tlabel source-drill)
    elseif(tname STREQUAL "dui-start-home") # I594: Home is drawn at start without --view; ledger row, not a plan item
      set(tlabel start-home)
    elseif(tname STREQUAL "dui-lib-ui-guard") # I906a: the debug UI-wait guard counts the shell thread's library waits and pump_ms_max is measured; ledger row, not a plan item
      set(tlabel lib-ui-guard)
    elseif(tname STREQUAL "dui-files-tab-first-frame") # I910: the Files tab draws its list on the first frame and the pump never waits for the collection read; ledger row, not a plan item
      set(tlabel files-tab-first-frame)
    elseif(tname STREQUAL "dui-collection-open-touch") # I910b: COLLECTION_OPEN touches Recents on the worker, no 0 songs header, source title is the name; ledger row, not a plan item
      set(tlabel collection-open-touch)
    elseif(tname STREQUAL "dui-collection-titles-touch") # I1198: touch-only library changes never throw the page titles away; ledger row, not a plan item
      set(tlabel collection-titles-touch)
    elseif(tname STREQUAL "dui-composer-settle") # I948: a SEL_VIEW_FOLDERS page re-reads the sidebar only when the saved playlists change; ledger row, not a plan item
      set(tlabel composer-settle)
    elseif(tname STREQUAL "dui-song-panel-art-async") # I906d: the song panel read and view art ids ride posted read slots, the pump never waits; ledger row, not a plan item
      set(tlabel song-panel-art-async)
    elseif(tname STREQUAL "dui-direct-calls-async") # I906e: moved library call sites answer through posted slots; ledger row, not a plan item, so no dui label (dui-ratchet counts dui against the plan ids)
      set(tlabel direct-calls-async)
    elseif(tname STREQUAL "dui-nav-turn-cost") # I959: page navigation turn cost; ledger row, not a plan item, so no dui label (dui-ratchet counts dui against the plan ids)
      set(tlabel nav-turn-cost)
    elseif(tname STREQUAL "dui-song-total-async") # I936: the catalogue song count is read off the pump thread; ledger row, not a plan item, so no dui label (dui-ratchet counts dui against the plan ids)
      set(tlabel song-total-async)
    elseif(tname STREQUAL "dui-stats-nowait") # I958: stats rating_pending comes from the writer counter; ledger row, not a plan item, so no dui label (dui-ratchet counts dui against the plan ids)
      set(tlabel stats-nowait)
    elseif(tname STREQUAL "dui-sources-first-paint") # I825: a chip click repaints while a sidebar read is in flight; ledger row, not a plan item, so no dui label (dui-ratchet counts dui against the plan ids)
      set(tlabel sources-first-paint)
    endif()
    # I989b: a script that compares a blessed frame hash (it honours SELECTA_GOLDEN_UPDATE) carries the golden label too.
    # The goldens are blessed on the maintainer's Mac: CI excludes the label (-LE golden), the pre-push step runs it (RULES.md).
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/${script}.cmake" script_text)
    if(script_text MATCHES "SELECTA_GOLDEN_UPDATE")
      set(tlabel "${tlabel};golden")
    endif()
    set(tenv "TMPDIR=${tmpdir}")
    if(script_text MATCHES "testsrc") # I1025: the test-only source plugin is a module, loaded only where a test names it
      list(APPEND tenv "SELECTA_PLUGIN_PATH=${SEL_TESTSRC_PLUGIN_DIR}")
    endif()
    set_tests_properties(${tname} PROPERTIES LABELS "${tlabel}" TIMEOUT 120 ENVIRONMENT "${tenv}")
  endforeach()
  # I906e: eight live SDL runs (six moved sites plus setup scans) take about 105 s; the shared 120 s leaves no margin.
  set_tests_properties(dui-direct-calls-async PROPERTIES TIMEOUT 300)
  set_tests_properties(dui-nav-turn-cost PROPERTIES TIMEOUT 300) # I959: three live SDL scenarios, each retried up to 3 times on a loaded machine
endif()

# dui-settings (I799a): the Settings entry in the top bar, one drawn row per shared pref row, an enum click written to prefs.conf.
if(NOT SEL_AMIGA)
  add_test(NAME dui-settings
    COMMAND "${CMAKE_COMMAND}" "-DSELECTA=$<TARGET_FILE:selecta>" "-DSONGS=${PROJECT_BINARY_DIR}/tests/songs"
      "-DWORK=${PROJECT_BINARY_DIR}/tests/dui-settings" -P "${CMAKE_CURRENT_SOURCE_DIR}/settings.cmake")
  set_tests_properties(dui-settings PROPERTIES LABELS "dui;settings" TIMEOUT 120) # I989e: the test of plan item DUI-P4-14, so it keeps the dui label that dui-ratchet counts
endif()

# bundle-sunvox (I706d): the shipped app plays a modern SunVox song through the staged SunVox Library and credits it in About.
if(NOT SEL_AMIGA AND TARGET sel_module_sunvox_lib AND TARGET selecta AND SELECTA_PLUGIN_LINK STREQUAL "MODULE")
  add_test(NAME bundle-sunvox
    COMMAND "${CMAKE_COMMAND}" "-DSELECTA=$<TARGET_FILE:selecta>" "-DFIXTURES=${PROJECT_SOURCE_DIR}/tests/m3/sunvox_fixtures.sh"
      "-DWORK=${PROJECT_BINARY_DIR}/tests/bundle-sunvox" -P "${CMAKE_CURRENT_SOURCE_DIR}/bundle_sunvox.cmake")
  set_tests_properties(bundle-sunvox PROPERTIES LABELS builtin-modules TIMEOUT 180 PASS_REGULAR_EXPRESSION "bundle_sunvox_ok" SKIP_REGULAR_EXPRESSION "bundle_sunvox_skip")
endif()

# dui-engine-thread-stall (I1082a): reachability test of the engine thread (plan 2026-11-05-I1082-engine-thread.md). RED on main until I1082f:
# a blocking open stops the UI. Labelled dui-red so the finished-milestone run (-LE dui-red) and the pre-push gate leave it out; no dui label,
# so dui-ratchet (one test per plan id) does not count it.
if(NOT SEL_AMIGA)
  add_test(NAME dui-engine-thread-stall
    COMMAND "${CMAKE_COMMAND}" "-DSELECTA=$<TARGET_FILE:selecta>" "-DWORK=${PROJECT_BINARY_DIR}/tests/dui-engine-thread-stall" "-DSDL_BUILT=${SELECTA_UI_DESKTOP}"
      "-DSLOW=${PROJECT_SOURCE_DIR}/tests/net/slow_server.py" "-DMOD=${PROJECT_SOURCE_DIR}/tests/net/fixtures/slow.mirror.test/mods/Protracker/test/song.mod"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/engine_thread_stall.cmake")
  set_tests_properties(dui-engine-thread-stall PROPERTIES LABELS "dui-red;engine-thread-stall" TIMEOUT 120)
  set_property(TEST dui-engine-thread-stall PROPERTY SKIP_REGULAR_EXPRESSION "SELECTA_SKIP")
endif()

# dui-search-online (I406a): the Search page's Modules.pl scope, row click, remembered scope and the radio button on the source page (I1019h), against the net fixture.
if(NOT SEL_AMIGA)
  add_test(NAME dui-search-online
    COMMAND "${CMAKE_COMMAND}" "-DSELECTA=$<TARGET_FILE:selecta>" "-DSONGS=${PROJECT_BINARY_DIR}/tests/songs"
      "-DWORK=${PROJECT_BINARY_DIR}/tests/dui-search-online" "-DNETWORK=${SEL_NET_WORK}"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/search_online.cmake")
  set_tests_properties(dui-search-online PROPERTIES LABELS search-online TIMEOUT 120 FIXTURES_REQUIRED net-fixture RESOURCE_LOCK net-fixture
    ENVIRONMENT "TMPDIR=${PROJECT_BINARY_DIR}/tests/dui-search-online.tmp")
  file(MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/tests/dui-search-online.tmp")
  # I1019e: no source chip picked: the fan-out line, one From section per searchable source in the shared row loop.
  add_test(NAME dui-search-all
    COMMAND "${CMAKE_COMMAND}" "-DSELECTA=$<TARGET_FILE:selecta>" "-DSONGS=${PROJECT_BINARY_DIR}/tests/songs"
      "-DWORK=${PROJECT_BINARY_DIR}/tests/dui-search-all" "-DNETWORK=${SEL_NET_WORK}" "-DSQLITE3=$<TARGET_FILE:sel_sqlite3_shell>"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/search_all.cmake")
  set_tests_properties(dui-search-all PROPERTIES LABELS search-online TIMEOUT 120 FIXTURES_REQUIRED net-fixture RESOURCE_LOCK net-fixture
    ENVIRONMENT "TMPDIR=${PROJECT_BINARY_DIR}/tests/dui-search-all.tmp")
  file(MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/tests/dui-search-all.tmp")
  # I1038: double-clicking a Radio Browser station in the All-sources results plays it; the pin it causes keeps the results and scroll.
  add_test(NAME dui-search-station-click
    COMMAND "${CMAKE_COMMAND}" "-DSELECTA=$<TARGET_FILE:selecta>" "-DSONGS=${PROJECT_BINARY_DIR}/tests/songs"
      "-DWORK=${PROJECT_BINARY_DIR}/tests/dui-search-station-click" "-DNETWORK=${SEL_NET_WORK}"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/search_station_click.cmake")
  set_tests_properties(dui-search-station-click PROPERTIES LABELS search-online TIMEOUT 120 FIXTURES_REQUIRED net-fixture RESOURCE_LOCK net-fixture
    ENVIRONMENT "TMPDIR=${PROJECT_BINARY_DIR}/tests/dui-search-station-click.tmp")
  file(MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/tests/dui-search-station-click.tmp")
  # I1020: every Search result row draws a source badge (Library, Modules.pl); the sentinel counts them.
  add_test(NAME dui-search-badge
    COMMAND "${CMAKE_COMMAND}" "-DSELECTA=$<TARGET_FILE:selecta>" "-DSONGS=${PROJECT_BINARY_DIR}/tests/songs"
      "-DWORK=${PROJECT_BINARY_DIR}/tests/dui-search-badge" "-DNETWORK=${SEL_NET_WORK}" "-DSQLITE3=$<TARGET_FILE:sel_sqlite3_shell>"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/search_badge.cmake")
  set_tests_properties(dui-search-badge PROPERTIES LABELS search-online TIMEOUT 120 FIXTURES_REQUIRED net-fixture RESOURCE_LOCK net-fixture
    ENVIRONMENT "TMPDIR=${PROJECT_BINARY_DIR}/tests/dui-search-badge.tmp")
  file(MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/tests/dui-search-badge.tmp")
  # I1073: the Search source chips scroll sideways with chevrons; at 1280 px Modules.pl is scrolled in and picked.
  add_test(NAME dui-search-chip-scroll
    COMMAND "${CMAKE_COMMAND}" "-DSELECTA=$<TARGET_FILE:selecta>"
      "-DWORK=${PROJECT_BINARY_DIR}/tests/dui-search-chip-scroll" "-DNETWORK=${SEL_NET_WORK}"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/search_chip_scroll.cmake")
  set_tests_properties(dui-search-chip-scroll PROPERTIES LABELS search-online TIMEOUT 120 FIXTURES_REQUIRED net-fixture RESOURCE_LOCK net-fixture
    ENVIRONMENT "TMPDIR=${PROJECT_BINARY_DIR}/tests/dui-search-chip-scroll.tmp")
  file(MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/tests/dui-search-chip-scroll.tmp")
  # I832: quitting during the fixture's 4 s first-byte delay returns well under the delay (the search task's net wait sees the cancel).
  add_test(NAME dui-search-online-quit
    COMMAND "${CMAKE_COMMAND}" "-DSELECTA=$<TARGET_FILE:selecta>" "-DSDL_BUILT=${SELECTA_UI_DESKTOP}"
      "-DWORK=${PROJECT_BINARY_DIR}/tests/dui-search-online-quit" "-DNETWORK=${SEL_NET_WORK}"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/search_online_quit.cmake")
  set_property(TEST dui-search-online-quit PROPERTY SKIP_REGULAR_EXPRESSION "SELECTA_SKIP") # I962
  set_tests_properties(dui-search-online-quit PROPERTIES LABELS search-online TIMEOUT 120 FIXTURES_REQUIRED net-fixture RESOURCE_LOCK net-fixture
    ENVIRONMENT "TMPDIR=${PROJECT_BINARY_DIR}/tests/dui-search-online-quit.tmp")
  file(MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/tests/dui-search-online-quit.tmp")
  # I817: the same search on the live SDL loop: the answer is drawn with no input after Enter.
  add_test(NAME dui-search-online-live
    COMMAND "${CMAKE_COMMAND}" "-DSELECTA=$<TARGET_FILE:selecta>" "-DSDL_BUILT=${SELECTA_UI_DESKTOP}"
      "-DWORK=${PROJECT_BINARY_DIR}/tests/dui-search-online-live" "-DNETWORK=${SEL_NET_WORK}"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/search_online_live.cmake")
  set_property(TEST dui-search-online-live PROPERTY SKIP_REGULAR_EXPRESSION "SELECTA_SKIP") # I962
  set_tests_properties(dui-search-online-live PROPERTIES LABELS search-online TIMEOUT 120 FIXTURES_REQUIRED net-fixture RESOURCE_LOCK net-fixture
    ENVIRONMENT "TMPDIR=${PROJECT_BINARY_DIR}/tests/dui-search-online-live.tmp")
  file(MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/tests/dui-search-online-live.tmp")
  # I828: the top bar field click from a source page gives the field the keyboard, so typing searches.
  add_test(NAME dui-topbar-search-from-source
    COMMAND "${CMAKE_COMMAND}" "-DSELECTA=$<TARGET_FILE:selecta>" "-DSONGS=${PROJECT_BINARY_DIR}/tests/songs"
      "-DWORK=${PROJECT_BINARY_DIR}/tests/dui-topbar-search-from-source" "-DNETWORK=${SEL_NET_WORK}"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/topbar_search_from_source.cmake")
  set_tests_properties(dui-topbar-search-from-source PROPERTIES LABELS search-online TIMEOUT 120 FIXTURES_REQUIRED net-fixture RESOURCE_LOCK net-fixture
    ENVIRONMENT "TMPDIR=${PROJECT_BINARY_DIR}/tests/dui-topbar-search-from-source.tmp")
  file(MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/tests/dui-topbar-search-from-source.tmp")
endif()

# DUI-P1-5: the lift out of plugins/ui/text, counted from the archives with nm.
if(NOT SEL_AMIGA)
  find_program(SEL_NM nm)
  add_test(NAME dui-lift-parity
    COMMAND "${CMAKE_COMMAND}" "-DSELECTA=$<TARGET_FILE:selecta>" "-DSONGS=${PROJECT_BINARY_DIR}/tests/songs"
      "-DWORK=${PROJECT_BINARY_DIR}/tests/dui-lift-parity" "-DNM=${SEL_NM}" "-DTEXTUI=$<TARGET_FILE:sel_plugin_text_ui>"
      "-DTEXTGRID=$<TARGET_FILE:selecta_textui>" "-DUIMODEL=$<TARGET_FILE:selecta_uimodel>"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/lift_parity.cmake")
  set_tests_properties(dui-lift-parity PROPERTIES LABELS dui)
endif()

# DUI-P1-8: the parity matrix against plan section 8.2 (text only, no binary).
add_test(NAME dui-parity-matrix
  COMMAND "${CMAKE_COMMAND}" "-DPLAN=${PROJECT_SOURCE_DIR}/thoughts/shared/plans/2026-10-24-desktop-ui.md"
    "-DMATRIX=${CMAKE_CURRENT_SOURCE_DIR}/parity_matrix.tsv" -P "${CMAKE_CURRENT_SOURCE_DIR}/parity_matrix.cmake")
set_tests_properties(dui-parity-matrix PROPERTIES LABELS dui)

# MUI-P0-7: the same matrix against the MUI plan; red while any row has NO ROW or names a MUI ID the plan lacks.
add_test(NAME mui-parity
  COMMAND "${CMAKE_COMMAND}" "-DPLAN=${PROJECT_SOURCE_DIR}/thoughts/shared/plans/2026-10-24-desktop-ui.md"
    "-DMUIPLAN=${PROJECT_SOURCE_DIR}/thoughts/shared/plans/2026-10-25-amiga-mui.md"
    "-DMATRIX=${CMAKE_CURRENT_SOURCE_DIR}/parity_matrix.tsv" -P "${CMAKE_CURRENT_SOURCE_DIR}/parity_matrix.cmake")
set_tests_properties(mui-parity PROPERTIES LABELS mui)

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/items.txt" SEL_DUI_ITEMS)
foreach(item IN LISTS SEL_DUI_ITEMS)
  string(REPLACE " " ";" parts "${item}")
  list(GET parts 0 id)
  list(GET parts 1 name)
  # Words after the test name are the reason a red test gives: the behaviour the plan's test asserts and what is missing.
  list(LENGTH parts nparts)
  set(why "")
  if(nparts GREATER 2)
    list(SUBLIST parts 2 -1 whyparts)
    string(REPLACE ";" " " why "${whyparts}")
  endif()
  if(NOT name IN_LIST SEL_DUI_REAL)
    add_test(NAME ${name}
      COMMAND "${CMAKE_COMMAND}" "-DID=${id}" "-DNAME=${name}" "-DWHY=${why}" -P "${CMAKE_CURRENT_SOURCE_DIR}/dui_red.cmake")
    set(labels dui dui-red)
    if(name STREQUAL "dui-perf")
      list(APPEND labels dui-perf)
    endif()
    set_tests_properties(${name} PROPERTIES LABELS "${labels}")
  endif()
endforeach()

# Pure unit test of the raw pointer to sel_gui_input mapping the sdl backend uses (I242).
add_executable(dui_input_map_unit input_map_unit.c)
target_link_libraries(dui_input_map_unit PRIVATE selecta_gui)
add_test(NAME gui-input-map COMMAND dui_input_map_unit)
set_tests_properties(gui-input-map PROPERTIES TIMEOUT 60)

# I522: the default format of the aga backend (index8) and of every other backend (rgba32). No dui label: the ratchet count stays.
add_executable(dui_default_format_unit default_format_unit.c)
target_link_libraries(dui_default_format_unit PRIVATE selecta_uimodel)
add_test(NAME dui-default-format COMMAND dui_default_format_unit)
set_tests_properties(dui-default-format PROPERTIES TIMEOUT 60 PASS_REGULAR_EXPRESSION "default_format_ok")

# I678h: the route table and the page history (lookups, push/back/forward, truncation, cap). No dui label: the ratchet count stays.
add_executable(dui_route_unit route_unit.c)
target_link_libraries(dui_route_unit PRIVATE selecta_uimodel)
add_test(NAME dui-route-unit COMMAND dui_route_unit)
set_tests_properties(dui-route-unit PROPERTIES TIMEOUT 60 PASS_REGULAR_EXPRESSION "route_unit_ok")

# DUI-P6-3: the Amiga cross build of src/gui and src/uimodel, counted with the m68k nm. Needs the bebbo toolchain.
if(NOT SEL_AMIGA)
  find_program(SEL_M68K_NM m68k-amigaos-nm HINTS "$ENV{AMIGA_GCC_ROOT}/bin" "$ENV{HOME}/opt/amiga/bin" /opt/amiga/bin)
  add_test(NAME dui-m68k-nofloat
    COMMAND "${CMAKE_COMMAND}" "-DSRC=${PROJECT_SOURCE_DIR}" "-DWORK=${PROJECT_BINARY_DIR}/dui-m68k" "-DGEN=${CMAKE_GENERATOR}"
      "-DCMAKE=${CMAKE_COMMAND}" "-DNM=${SEL_M68K_NM}" -P "${CMAKE_CURRENT_SOURCE_DIR}/m68k_nofloat.cmake")
  set_tests_properties(dui-m68k-nofloat PROPERTIES LABELS "dui;m68k-gcc" RUN_SERIAL TRUE TIMEOUT 300)
endif()

# The Amiga art worker runs the job inline (I369): the host builds amiga/art_thread.c on its own, it needs no Amiga headers.
add_executable(dui_art_thread_amiga_unit art_thread_amiga_unit.c ${PROJECT_SOURCE_DIR}/plugins/ui/desktop/amiga/art_thread.c)
target_include_directories(dui_art_thread_amiga_unit PRIVATE ${PROJECT_SOURCE_DIR}/plugins/ui/desktop)
target_link_libraries(dui_art_thread_amiga_unit PRIVATE selecta_gui)
add_test(NAME dui-art-thread-amiga COMMAND dui_art_thread_amiga_unit)
set_tests_properties(dui-art-thread-amiga PROPERTIES TIMEOUT 60)

# I439 (M4-P6-1): the aga backend's pure helpers (row bands, reference c2p and its inverse, chip budget, LoadRGB32
# table). The host builds amiga/aga_planar.c on its own, it needs no Amiga headers.
add_executable(dui_aga_planar_unit aga_planar_unit.c ${PROJECT_SOURCE_DIR}/plugins/ui/desktop/amiga/aga_planar.c)
target_include_directories(dui_aga_planar_unit PRIVATE ${PROJECT_SOURCE_DIR}/plugins/ui/desktop)
target_link_libraries(dui_aga_planar_unit PRIVATE selecta_gui)
target_compile_definitions(dui_aga_planar_unit PRIVATE SELECTA_AGA_BACKEND_SRC="${PROJECT_SOURCE_DIR}/plugins/ui/desktop/amiga/backend_aga.c")
add_test(NAME dui-aga-planar COMMAND dui_aga_planar_unit)
set_tests_properties(dui-aga-planar PROPERTIES TIMEOUT 60)

# I439 (M4-P6-1): Kalms' 040 c2p against the C reference on a 68040, under vamos. Like the other vamos tests it needs vamos
# and the Amiga build tree (selecta-aga-c2p-kalms only exists when vasmm68k_mot was found there); the amiga-build fixture
# rebuilds it first and the vamos lock keeps two instances from fighting over the temp RAM: volume. PASS is the line
# c2p_kalms040=match. No dui label: the dui-ratchet count must not move.
set(SEL_AGA_C2P_KALMS "${PROJECT_SOURCE_DIR}/build/amiga/plugins/ui/desktop/selecta-aga-c2p-kalms")
if(SEL_VAMOS AND EXISTS "${SEL_AMIGA_CLI}" AND EXISTS "${SEL_AGA_C2P_KALMS}")
  add_test(NAME dui-aga-c2p-kalms COMMAND "${SEL_VAMOS}" -C 68040 -m 8192 -- "${SEL_AGA_C2P_KALMS}")
  set_tests_properties(dui-aga-c2p-kalms PROPERTIES FIXTURES_REQUIRED amiga_cli RESOURCE_LOCK vamos TIMEOUT 120
    PASS_REGULAR_EXPRESSION "c2p_kalms040=match" FAIL_REGULAR_EXPRESSION "mismatch")
endif()

# I369: the scaler must run on a small stack (the Amiga task stack is 128 KB); a canvas-sized local crashes it.
if(NOT SEL_AMIGA)
  find_package(Threads REQUIRED)
  add_executable(dui_image_scale_stack_unit image_scale_stack_unit.c)
  target_link_libraries(dui_image_scale_stack_unit PRIVATE selecta_gui Threads::Threads)
  add_test(NAME dui-image-scale-stack COMMAND dui_image_scale_stack_unit)
  set_tests_properties(dui-image-scale-stack PROPERTIES TIMEOUT 60)
endif()

# I531: the Amiga Add folder dialog's platform independent half (requester answer to SEL_GUI_IN_DROP, cancel, spaces). The
# host builds amiga/folder_pick.c on its own, it needs no Amiga headers.
add_executable(dui_amiga_folder_pick_unit amiga_folder_pick_unit.c ${PROJECT_SOURCE_DIR}/plugins/ui/desktop/amiga/folder_pick.c)
target_include_directories(dui_amiga_folder_pick_unit PRIVATE ${PROJECT_SOURCE_DIR}/plugins/ui/desktop)
target_link_libraries(dui_amiga_folder_pick_unit PRIVATE selecta_gui)
add_test(NAME dui-amiga-folder-pick COMMAND dui_amiga_folder_pick_unit)
set_tests_properties(dui-amiga-folder-pick PROPERTIES TIMEOUT 60)

# I441: copper gradients: the gradient-to-copper-line table builder and the AGA register words (pure; host).
add_executable(dui_copper_gradient_unit copper_gradient_unit.c ${PROJECT_SOURCE_DIR}/plugins/ui/desktop/amiga/aga_planar.c)
target_include_directories(dui_copper_gradient_unit PRIVATE ${PROJECT_SOURCE_DIR}/plugins/ui/desktop)
target_link_libraries(dui_copper_gradient_unit PRIVATE selecta_gui)
add_test(NAME dui-copper-gradient COMMAND dui_copper_gradient_unit)
set_tests_properties(dui-copper-gradient PROPERTIES TIMEOUT 60)

# I548: a stopped transport reads 00:00 (the frozen slot position is not shown).
add_executable(dui_stopped_clock_unit stopped_clock_unit.c)
target_link_libraries(dui_stopped_clock_unit PRIVATE selecta_uimodel)
add_test(NAME dui-stopped-clock COMMAND dui_stopped_clock_unit)

# I595: INDEX8 anti-aliased edges blend against the destination real colour: art band cube over artwork, the row colour over a copper pen.
add_executable(dui_index8_blend_unit index8_blend_unit.c)
target_link_libraries(dui_index8_blend_unit PRIVATE selecta_gui)
add_test(NAME dui-index8-blend COMMAND dui_index8_blend_unit)
set_tests_properties(dui-index8-blend PROPERTIES TIMEOUT 60)


sel_isolate_config() # I989b
