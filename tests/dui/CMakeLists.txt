# Desktop UI milestone (M3d, plan 2026-10-24-desktop-ui.md section 5). tests/dui/items.txt is "<id> <test name>" per
# item. Every item without a real test here is registered red by dui_red.cmake; the commit that implements an item
# replaces its red entry with the real test and removes its id from tests/ratchet/unimplemented.txt. Red entries also
# carry label dui-red, so a run of the finished milestones (M3 finish line) can leave them out with -LE dui-red while
# dui-ratchet keeps the red count honest.
set(SEL_DUI_REAL dui-ratchet dui-build-deps dui-uim-frame dui-ui-switch dui-ui-multi dui-mode-pref dui-lift-parity dui-actions dui-layout dui-parity-matrix dui-governor dui-tokens dui-draw dui-dirty dui-text dui-artwork dui-genart dui-golden-home dui-sdl dui-widgets dui-hidpi)

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
  foreach(spec "dui-uim-frame:uim_frame" "dui-ui-switch:ui_switch" "dui-ui-multi:ui_multi" "dui-mode-pref:mode_pref" "dui-actions:actions" "dui-layout:layout" "dui-governor:governor" "dui-tokens:tokens" "dui-draw:draw" "dui-dirty:dirty" "dui-text:text" "dui-artwork:artwork" "dui-genart:genart" "dui-golden-home:golden_home" "dui-sdl:sdl" "dui-widgets:widgets" "dui-hidpi:hidpi")
    string(REPLACE ":" ";" pair "${spec}")
    list(GET pair 0 tname)
    list(GET pair 1 script)
    add_test(NAME ${tname}
      COMMAND "${CMAKE_COMMAND}" "-DSELECTA=$<TARGET_FILE:selecta>" "-DSONGS=${PROJECT_BINARY_DIR}/tests/songs"
        "-DWORK=${PROJECT_BINARY_DIR}/tests/${tname}" "-DGOLDEN=${PROJECT_SOURCE_DIR}/tests/golden"
        "-DSDL_BUILT=${SELECTA_UI_DESKTOP}" "-DUNIT=$<TARGET_FILE:dui_draw_unit>" "-DGUISRC=${PROJECT_SOURCE_DIR}/src/gui" "-DGUIHDR=${PROJECT_SOURCE_DIR}/include/selecta/gui" -P "${CMAKE_CURRENT_SOURCE_DIR}/${script}.cmake")
    # Each test owns its default control socket path ($TMPDIR/selecta-<uid>.sock): two tests sharing
    # the per-user default collide under ctest -j ("selecta: already running"). TMPDIR, not
    # SELECTA_SOCKET, because SELECTA_SOCKET also forces the socket on and keeps --exec runs alive.
    set(tmpdir "${PROJECT_BINARY_DIR}/tests/${tname}.tmp")
    file(MAKE_DIRECTORY "${tmpdir}")
    set_tests_properties(${tname} PROPERTIES LABELS dui ENVIRONMENT "TMPDIR=${tmpdir}")
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
