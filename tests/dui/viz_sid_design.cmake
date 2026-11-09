# dui-viz-sid-design (I1230): the SID PatternScope drawn in the app design. -DSELECTA -DWORK -DGOLDEN -DSID_MODULES.
# Johan 2026-11-09: "the sid patterncope works but it looks like shit not at all like the selecta app design with its
# pixel font etc, make it use the app design modern dark fresh". The desktop now hands every viz plugin a pen
# (sel_viz_pen, include/selecta/viz.h): labels in the app font, rounded shapes with the theme radius and theme roles.
# Drives the real `selecta --ui desktop` entry point on the Visualizer page while the SID register feed fixture
# (tests/fixtures/plugin_sidfeed) plays, with the SID PatternScope selected (ctrl+8):
# 1. Sentinel `viz_pen glyphs G missing M shapes S`: the plugin drew its labels through the pen in the app font
#    (G > 0, M = 0) and its cards, bars and chips through the pen (S > 0). Fails when the desktop leaves the pen out.
# 2. The canvas hash matches tests/golden/desktop/sid-scope-rgba32-1280x800@1.hash (update with SELECTA_GOLDEN_UPDATE=1).
# 3. Without a SID tune (no feed) the scope still counts 0 primitives (the dui-viz rule) while drawing its unlit layout.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
file(WRITE "${WORK}/feed.selsid" "SELSID fixture")
set(ENV{SELECTA_PLUGIN_PATH} "${SID_MODULES}")

function(sid_run tag song frame)
  file(REMOVE_RECURSE "${WORK}/cfg" "${WORK}/${tag}.db")
  file(WRITE "${WORK}/${tag}.txt" "wait 300\nkey ctrl+8\nwait 200\n")
  dui_run(o --db "${WORK}/${tag}.db" --ui desktop --backend mem --view viz --canvas 1280x800 --clock fixed --stats
    --exec "OPEN ${song}" --ticks 12 --input "${WORK}/${tag}.txt" --frame-dump "${frame}" ${ARGN})
  if(NOT "${o}" MATCHES "viz frames ([0-9]+) song_ms [0-9]+ plugin ([a-z_]+) .* prims ([0-9]+)")
    message(FATAL_ERROR "${tag}: no viz sentinel in:\n${o}")
  endif()
  set(${tag}_plugin "${CMAKE_MATCH_2}" PARENT_SCOPE)
  set(${tag}_prims "${CMAKE_MATCH_3}" PARENT_SCOPE)
  if(NOT "${o}" MATCHES "viz_pen glyphs ([0-9]+) missing ([0-9]+) shapes ([0-9]+)")
    message(FATAL_ERROR "${tag}: no viz_pen sentinel in:\n${o}")
  endif()
  set(${tag}_glyphs "${CMAKE_MATCH_1}" PARENT_SCOPE)
  set(${tag}_missing "${CMAKE_MATCH_2}" PARENT_SCOPE)
  set(${tag}_shapes "${CMAKE_MATCH_3}" PARENT_SCOPE)
  dui_stat(h "${o}" desktop canvas_hash)
  set(${tag}_hash "${h}" PARENT_SCOPE)
endfunction()

# 1. + 2. Fed, playing.
sid_run(fed "${WORK}/feed.selsid" "${WORK}/sid-scope.ppm" --exec PLAY)
if(NOT fed_plugin STREQUAL "viz_sidregs" OR fed_prims EQUAL 0)
  message(FATAL_ERROR "the SID PatternScope did not draw: plugin ${fed_plugin}, ${fed_prims} primitives")
endif()
if(fed_glyphs EQUAL 0 OR NOT fed_missing EQUAL 0 OR fed_shapes EQUAL 0)
  message(FATAL_ERROR "the SID PatternScope did not draw through the app pen: glyphs ${fed_glyphs} missing ${fed_missing} shapes ${fed_shapes}")
endif()
set(gfile "${GOLDEN}/desktop/sid-scope-rgba32-1280x800@1.hash")
if("$ENV{SELECTA_GOLDEN_UPDATE}" STREQUAL "1")
  file(WRITE "${gfile}" "${fed_hash}\n")
endif()
if(NOT EXISTS "${gfile}")
  message(FATAL_ERROR "missing golden ${gfile} (hash ${fed_hash}); create it with SELECTA_GOLDEN_UPDATE=1")
endif()
file(STRINGS "${gfile}" want LIMIT_COUNT 1)
if(NOT fed_hash STREQUAL want)
  message(FATAL_ERROR "SID scope hash ${fed_hash} differs from golden ${want} (${gfile})")
endif()

# 3. No feed: a MOD plays nothing into the SID registers.
sid_run(idle "${HERE}/../m3/lyrics/ambient.mod" "${WORK}/idle.ppm")
if(NOT idle_plugin STREQUAL "viz_sidregs" OR NOT idle_prims EQUAL 0 OR idle_glyphs EQUAL 0)
  message(FATAL_ERROR "SID PatternScope without a feed: plugin ${idle_plugin}, ${idle_prims} primitives (want 0), glyphs ${idle_glyphs} (want the unlit labels)")
endif()
message(STATUS "dui-viz-sid-design ok: ${fed_prims} primitives, ${fed_glyphs} app-font glyphs, ${fed_shapes} shapes, hash ${fed_hash}")
