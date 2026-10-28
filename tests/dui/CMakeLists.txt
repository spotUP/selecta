# Desktop UI milestone (M3d, plan 2026-10-24-desktop-ui.md section 5). tests/dui/items.txt is "<id> <test name>" per
# item. Every item without a real test here is registered red by dui_red.cmake; the commit that implements an item
# replaces its red entry with the real test and removes its id from tests/ratchet/unimplemented.txt. Red entries also
# carry label dui-red, so a run of the finished milestones (M3 finish line) can leave them out with -LE dui-red while
# dui-ratchet keeps the red count honest.
set(SEL_DUI_REAL dui-font-baked dui-ratchet dui-build-deps dui-uim-frame dui-ui-switch dui-ui-multi dui-mode-pref dui-lift-parity dui-actions dui-layout dui-breakpoints dui-parity-matrix dui-governor dui-tokens dui-draw dui-dirty dui-text dui-artwork dui-genart dui-golden-home dui-sdl dui-widgets dui-hidpi dui-anim dui-keys dui-mouse dui-tooltips dui-gestures dui-frame dui-home dui-library dui-styles dui-grid-coverflow dui-search dui-search-view-mode dui-browse-tiles dui-nowplaying dui-queue dui-playlists dui-songinfo dui-overlays dui-nav dui-viz dui-tracker dui-drop dui-m68k-nofloat dui-hover-live dui-hover-rows dui-hover-sidebar-ink dui-hover-instant dui-hover-nowplaying dui-hover-regions dui-library-reload dui-library-unknown-composer dui-song-menu dui-playlist-page dui-library-remove dui-aga-budget dui-library-style dui-coverflow-slide dui-stats-panel dui-topbar dui-show-all)

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
  add_test(NAME dui-amiga-font COMMAND dui_amiga_font_unit)
  set_tests_properties(dui-amiga-font PROPERTIES LABELS amiga-font TIMEOUT 60) # ledger row, not a plan item: no dui label, dui-ratchet counts dui against the plan ids
  # dui-font-tall (I538): AGA atlases baked for 1:2 pixels, SDL atlases unchanged.
  add_test(NAME dui-font-tall COMMAND dui_font_tall_unit)
  set_tests_properties(dui-font-tall PROPERTIES LABELS font-tall TIMEOUT 60) # ledger row, not a plan item: no dui label, dui-ratchet counts dui against the plan ids
  # dui-font-roles (I526-P4): the role table and the 6-row fallback, unit level.
  add_test(NAME dui-font-roles COMMAND dui_font_roles_unit)
  set_tests_properties(dui-font-roles PROPERTIES LABELS font-roles TIMEOUT 60) # ledger row, not a plan item: no dui label, dui-ratchet counts dui against the plan ids
endif()
if(NOT SEL_AMIGA)
  foreach(spec "dui-uim-frame:uim_frame" "dui-ui-switch:ui_switch" "dui-ui-multi:ui_multi" "dui-mode-pref:mode_pref" "dui-actions:actions" "dui-layout:layout" "dui-breakpoints:breakpoints" "dui-governor:governor" "dui-tokens:tokens" "dui-draw:draw" "dui-dirty:dirty" "dui-text:text" "dui-artwork:artwork" "dui-genart:genart" "dui-golden-home:golden_home" "dui-sdl:sdl" "dui-widgets:widgets" "dui-hidpi:hidpi" "dui-anim:anim" "dui-keys:keys" "dui-mouse:mouse" "dui-tooltips:tooltips" "dui-gestures:gestures" "dui-frame:frame" "dui-home:home" "dui-library:library" "dui-styles:styles" "dui-grid-coverflow:grid_coverflow" "dui-search:search" "dui-search-view-mode:search_view_mode" "dui-browse-tiles:browse_tiles" "dui-nowplaying:nowplaying" "dui-queue:queue" "dui-queue-scroll:queue_scroll" "dui-playlists:playlists" "dui-songinfo:songinfo" "dui-overlays:overlays" "dui-nav:nav" "dui-viz:viz" "dui-tracker:tracker" "dui-drop:drop" "dui-footer:footer" "dui-footer-one-row:footer_one_row" "dui-screen:screen" "dui-hover-live:hover_live" "dui-hover-rows:hover_rows" "dui-hover-sidebar-ink:hover_sidebar_ink" "dui-hover-instant:hover_instant" "dui-hover-nowplaying:hover_nowplaying" "dui-hover-regions:hover_regions" "dui-scan-ui:scan_ui" "dui-library-reload:library_reload" "dui-library-unknown-composer:library_unknown_composer" "dui-song-menu:song_menu" "dui-library-remove:library_remove" "dui-song-menu-live:song_menu_live" "dui-song-menu-lists:song_menu_lists" "dui-select-lists:select_lists" "keys-live:keys_live" "dui-aga-budget:aga_budget" "dui-library-style:library_style" "dui-stats-panel:stats_panel" "dui-home-cover-shadow:home_cover_shadow" "dui-coverflow-slide:coverflow_slide" "dui-view-exec-order:view_exec_order" "dui-zoom:zoom" "dui-list-keys:list_keys" "dui-library-sources:library_sources" "dui-library-scroll:library_scroll" "dui-library-scroll-drag:library_scroll_drag" "dui-sources-counters:sources_counters" "dui-sync-banner:sync_banner" "dui-tall-pixels:tall_pixels" "dui-aga-button-polish:aga_button_polish" "dui-aga-native-text:aga_native_text" "window-raise:window_raise" "dui-idle-turns:idle_turns" "dui-start-home:start_home" "dui-home-tile-tint:home_tile_tint" "dui-sidebar-library:sidebar_library" "dui-playlist-page:playlist_page" "dui-quit-dialog:quit_dialog" "dui-home-copper:home_copper" "dui-topbar:topbar" "dui-show-all:show_all" "dui-topbar-doors:topbar_doors" "dui-library-tools:library_tools")
    string(REPLACE ":" ";" pair "${spec}")
    list(GET pair 0 tname)
    list(GET pair 1 script)
    add_test(NAME ${tname}
      COMMAND "${CMAKE_COMMAND}" "-DSELECTA=$<TARGET_FILE:selecta>" "-DSONGS=${PROJECT_BINARY_DIR}/tests/songs"
        "-DWORK=${PROJECT_BINARY_DIR}/tests/${tname}" "-DGOLDEN=${PROJECT_SOURCE_DIR}/tests/golden"
        "-DSDL_BUILT=${SELECTA_UI_DESKTOP}" "-DGENMOD=$<TARGET_FILE:gen_testmod>" "-DUNIT=$<TARGET_FILE:dui_draw_unit>" "-DTAGGED=${PROJECT_SOURCE_DIR}/tests/m3/stream" "-DGUISRC=${PROJECT_SOURCE_DIR}/src/gui" "-DGUIHDR=${PROJECT_SOURCE_DIR}/include/selecta/gui" -P "${CMAKE_CURRENT_SOURCE_DIR}/${script}.cmake")
    # Each test owns its default control socket path ($TMPDIR/selecta-<uid>.sock): two tests sharing
    # the per-user default collide under ctest -j ("selecta: already running"). TMPDIR, not
    # SELECTA_SOCKET, because SELECTA_SOCKET also forces the socket on and keeps --exec runs alive.
    set(tmpdir "${PROJECT_BINARY_DIR}/tests/${tname}.tmp")
    file(MAKE_DIRECTORY "${tmpdir}")
    # dui-nav is ledger row I262, not a plan item: label nav, not dui or dui-extra: dui-ratchet counts tests whose label matches the regex dui against the plan ids.
    set(tlabel dui)
    if(tname STREQUAL "dui-nav" OR tname STREQUAL "dui-footer" OR tname STREQUAL "dui-footer-one-row" OR tname STREQUAL "dui-screen" OR tname STREQUAL "dui-scan-ui") # dui-footer is ledger row I312, not a plan item
      set(tlabel nav)
    elseif(tname STREQUAL "dui-song-menu-live" OR tname STREQUAL "dui-song-menu-lists") # I371: more cases of DUI-P4-16, whose one dui test is dui-song-menu; label song-menu keeps dui-ratchet at one test per plan id
      set(tlabel song-menu)
    elseif(tname STREQUAL "dui-select-lists") # I632d: multi-select on the desktop lists, a case of the multi-select plan; label select-lists keeps dui-ratchet at one test per plan id
      set(tlabel select-lists)
    elseif(tname STREQUAL "dui-queue-scroll") # I258: the wheel scroll of DUI-P4-8, whose one dui test is dui-queue; not a plan item
      set(tlabel queue-scroll)
    elseif(tname STREQUAL "dui-list-keys") # I423: list keys, a case of DUI-P3-3 (dui-keys); label list-keys keeps dui-ratchet at one test per plan id
      set(tlabel list-keys)
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
    elseif(tname STREQUAL "dui-library-sources") # I265: the Sources tab, a case of DUI-P4-3 (dui-library); label library-sources keeps dui-ratchet at one test per plan id
      set(tlabel library-sources)
    elseif(tname STREQUAL "dui-sync-banner") # I527: the sync-on-launch banner on the Library page, a case of DUI-P4-3 (dui-library)
      set(tlabel sync-banner)
    elseif(tname STREQUAL "keys-live") # I268: real SDL key events through desk_sdl_poll; not a plan item, so no dui label
      set(tlabel keys-live)
    elseif(tname STREQUAL "dui-aga-native-text" OR tname STREQUAL "dui-idle-turns" OR tname STREQUAL "dui-library-scroll" OR tname STREQUAL "dui-library-scroll-drag" OR tname STREQUAL "dui-sources-counters" OR tname STREQUAL "dui-quit-dialog") # I526 I525 I541 I549 I546: ledger rows, not plan items; their own label keeps dui-ratchet at one test per plan id
      string(REPLACE "dui-" "" tlabel "${tname}")
    elseif(tname STREQUAL "dui-view-exec-order") # I398: --view enters its page before --exec runs; ledger row, not a plan item
      set(tlabel view-exec-order)
    elseif(tname STREQUAL "dui-home-tile-tint") # I600: idle Home cards have no dark fill over the tint, a case of DUI-P4-2 (dui-home); not a plan item
      set(tlabel home-tile-tint)
    elseif(tname STREQUAL "dui-home-copper") # I628: Home background and generated covers ride the AGA copper; ledger row, not a plan item
      set(tlabel home-copper)
    elseif(tname STREQUAL "dui-topbar-doors") # I615a: Home and Search leave the sidebar where the top bar owns them; ledger row, not a plan item
      set(tlabel topbar-doors)
    elseif(tname STREQUAL "dui-library-tools") # I611: the Library page tools row filters and sorts the page rows; ledger row, not a plan item
      set(tlabel library-tools)
    elseif(tname STREQUAL "dui-sidebar-library") # I608: the sidebar Your Library list; ledger row, not a plan item
      set(tlabel sidebar-library)
    elseif(tname STREQUAL "dui-start-home") # I594: Home is drawn at start without --view; ledger row, not a plan item
      set(tlabel start-home)
    endif()
    set_tests_properties(${tname} PROPERTIES LABELS ${tlabel} TIMEOUT 120 ENVIRONMENT "TMPDIR=${tmpdir}")
  endforeach()
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

# DUI-P6-3: the Amiga cross build of src/gui and src/uimodel, counted with the m68k nm. Needs the bebbo toolchain.
if(NOT SEL_AMIGA)
  find_program(SEL_M68K_NM m68k-amigaos-nm HINTS "$ENV{AMIGA_GCC_ROOT}/bin" "$ENV{HOME}/opt/amiga/bin" /opt/amiga/bin)
  add_test(NAME dui-m68k-nofloat
    COMMAND "${CMAKE_COMMAND}" "-DSRC=${PROJECT_SOURCE_DIR}" "-DWORK=${PROJECT_BINARY_DIR}/dui-m68k" "-DGEN=${CMAKE_GENERATOR}"
      "-DCMAKE=${CMAKE_COMMAND}" "-DNM=${SEL_M68K_NM}" -P "${CMAKE_CURRENT_SOURCE_DIR}/m68k_nofloat.cmake")
  set_tests_properties(dui-m68k-nofloat PROPERTIES LABELS dui RUN_SERIAL TRUE TIMEOUT 300)
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
