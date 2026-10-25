# Desktop UI milestone (M3d, plan 2026-10-24-desktop-ui.md section 5). tests/dui/items.txt is "<id> <test name>" per
# item. Every item without a real test here is registered red by dui_red.cmake; the commit that implements an item
# replaces its red entry with the real test and removes its id from tests/ratchet/unimplemented.txt. Red entries also
# carry label dui-red, so a run of the finished milestones (M3 finish line) can leave them out with -LE dui-red while
# dui-ratchet keeps the red count honest.
set(SEL_DUI_REAL dui-ratchet dui-build-deps dui-uim-frame dui-ui-switch dui-ui-multi dui-mode-pref dui-lift-parity dui-actions dui-layout dui-parity-matrix dui-governor dui-tokens dui-draw dui-dirty dui-text dui-artwork dui-genart dui-golden-home dui-sdl dui-widgets dui-hidpi dui-anim dui-keys dui-mouse dui-tooltips dui-gestures dui-frame dui-home dui-library dui-styles dui-grid-coverflow dui-search dui-nowplaying dui-queue dui-playlists dui-songinfo dui-overlays dui-nav dui-viz dui-tracker dui-drop dui-m68k-nofloat)

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
  foreach(spec "dui-uim-frame:uim_frame" "dui-ui-switch:ui_switch" "dui-ui-multi:ui_multi" "dui-mode-pref:mode_pref" "dui-actions:actions" "dui-layout:layout" "dui-governor:governor" "dui-tokens:tokens" "dui-draw:draw" "dui-dirty:dirty" "dui-text:text" "dui-artwork:artwork" "dui-genart:genart" "dui-golden-home:golden_home" "dui-sdl:sdl" "dui-widgets:widgets" "dui-hidpi:hidpi" "dui-anim:anim" "dui-keys:keys" "dui-mouse:mouse" "dui-tooltips:tooltips" "dui-gestures:gestures" "dui-frame:frame" "dui-home:home" "dui-library:library" "dui-styles:styles" "dui-grid-coverflow:grid_coverflow" "dui-search:search" "dui-nowplaying:nowplaying" "dui-queue:queue" "dui-playlists:playlists" "dui-songinfo:songinfo" "dui-overlays:overlays" "dui-nav:nav" "dui-viz:viz" "dui-tracker:tracker" "dui-drop:drop" "dui-footer:footer" "keys-live:keys_live")
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
    if(tname STREQUAL "dui-nav" OR tname STREQUAL "dui-footer") # dui-footer is ledger row I312, not a plan item
      set(tlabel nav)
    elseif(tname STREQUAL "keys-live") # I268: real SDL key events through desk_sdl_poll; not a plan item, so no dui label
      set(tlabel keys-live)
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

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/items.txt" SEL_DUI_ITEMS)
foreach(item IN LISTS SEL_DUI_ITEMS)
  string(REPLACE " " ";" parts "${item}")
  list(GET parts 0 id)
  list(GET parts 1 name)
  if(NOT name IN_LIST SEL_DUI_REAL)
    add_test(NAME ${name}
      COMMAND "${CMAKE_COMMAND}" "-DID=${id}" "-DNAME=${name}" -P "${CMAKE_CURRENT_SOURCE_DIR}/dui_red.cmake")
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

# DUI-P6-3: the Amiga cross build of src/gui and src/uimodel, counted with the m68k nm. Needs the bebbo toolchain.
if(NOT SEL_AMIGA)
  find_program(SEL_M68K_NM m68k-amigaos-nm HINTS "$ENV{AMIGA_GCC_ROOT}/bin" "$ENV{HOME}/opt/amiga/bin" /opt/amiga/bin)
  add_test(NAME dui-m68k-nofloat
    COMMAND "${CMAKE_COMMAND}" "-DSRC=${PROJECT_SOURCE_DIR}" "-DWORK=${PROJECT_BINARY_DIR}/dui-m68k" "-DGEN=${CMAKE_GENERATOR}"
      "-DCMAKE=${CMAKE_COMMAND}" "-DNM=${SEL_M68K_NM}" -P "${CMAKE_CURRENT_SOURCE_DIR}/m68k_nofloat.cmake")
  set_tests_properties(dui-m68k-nofloat PROPERTIES LABELS dui RUN_SERIAL TRUE TIMEOUT 300)
endif()
