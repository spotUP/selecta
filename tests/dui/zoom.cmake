# dui-zoom (I458, design doc 10): the fit scale and UI Zoom, driven through the selecta entry point on the mem backend.
# -DSELECTA -DSONGS -DWORK -DGOLDEN
# 1. --zoom fit --canvas 800x600 gives ui_scale_q16=51200 (0.78125) and the logical size 1024x768.
# 2. --zoom 125 --canvas 1280x800 gives ui_scale_q16=81920 and the logical size 1024x640.
# 3. The default on the mem backend is zoom 100 (65536); a zoom outside the set is refused.
# 4. Ctrl+Minus then Ctrl+0 (scripted key) steps the zoom and returns to the default, with a relayout.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")

function(check_zoom name zoom canvas q16 logical)
  dui_run(out --ui desktop --backend mem --view home --clock fixed --stats --zoom ${zoom} --canvas ${canvas})
  if(NOT out MATCHES "ui_scale_q16=${q16} ui_zoom=${zoom} ui_logical=${logical}[ \n]")
    message(FATAL_ERROR "${name}: expected ui_scale_q16=${q16} ui_zoom=${zoom} ui_logical=${logical} in:\n${out}")
  endif()
endfunction()

check_zoom(fit fit 800x600 51200 1024x768)
check_zoom(fit-small fit 640x480 40960 1024x768)
check_zoom(fit-large fit 1280x800 65536 1280x800)
check_zoom(zoom-125 125 1280x800 81920 1024x640)
check_zoom(zoom-75 75 1280x800 49152 1706x1066)

# I460: a fit scale asks for icon sizes that are not baked (13, 19, 25 at 0.78125); each is area-resampled, not
# borrowed from the 16 px masks. Integer scales use the baked masks only.
dui_run(fit_icons --ui desktop --backend mem --view home --clock fixed --stats --zoom fit --canvas 800x600)
if(NOT fit_icons MATCHES "icon_resampled=3[^0-9]")
  message(FATAL_ERROR "fit 800x600 must resample 3 icon sizes:\n${fit_icons}")
endif()
dui_run(def --ui desktop --backend mem --view home --clock fixed --stats --canvas 1280x800)
if(NOT def MATCHES "ui_scale_q16=65536 ui_zoom=100 ui_logical=1280x800[ \n]")
  message(FATAL_ERROR "mem default zoom must be 100:\n${def}")
endif()

# I566: desk_open reads ui.desktop.zoom (and ui.desktop.overscan on AGA) from prefs.conf. Prefs are answered by the engine's
# plugin host only; the raw host desk_open got always returned the default, so a stored zoom (and Fill Screen) never applied.
file(WRITE "${WORK}/cfg/prefs.conf" "library.sync_stale_days=0\nui.desktop.zoom=125\n")
dui_run(pref --ui desktop --backend mem --view home --clock fixed --stats --canvas 1280x800)
file(WRITE "${WORK}/cfg/prefs.conf" "library.sync_stale_days=0\n")
if(NOT pref MATCHES "ui_scale_q16=81920 ui_zoom=125 ui_logical=1024x640[ \n]")
  message(FATAL_ERROR "ui.desktop.zoom=125 in prefs.conf must open at zoom 125:\n${pref}")
endif()

execute_process(COMMAND "${SELECTA}" --ui desktop --backend mem --zoom 90 --canvas 800x600 RESULT_VARIABLE rc OUTPUT_QUIET ERROR_VARIABLE err)
if(rc EQUAL 0 OR NOT err MATCHES "zoom '90' is not")
  message(FATAL_ERROR "zoom 90 must be refused (rc=${rc}): ${err}")
endif()

# Ctrl+Plus steps 100 to 125, Ctrl+Minus back, Ctrl+0 restores the default; the layout follows.
file(WRITE "${WORK}/keys.txt" "key ctrl+plus\nkey ctrl++\nkey ctrl+-\nkey ctrl+-\nkey ctrl+0\nkey ctrl++\n")
dui_run(keyed --ui desktop --backend mem --view home --clock fixed --stats --canvas 1280x800 --input "${WORK}/keys.txt")
if(NOT keyed MATCHES "ui_scale_q16=81920 ui_zoom=125 ui_logical=1024x640[ \n]")
  message(FATAL_ERROR "ctrl+plus twice then minus twice then 0 then plus must end at 125:\n${keyed}")
endif()
if(NOT def MATCHES "icon_resampled=0[^0-9]")
  message(FATAL_ERROR "100% must use baked icons only:\n${def}")
endif()

# I587: on the Amiga tier (--baked-zoom stands for aga and rtg) zoom and fit take only the baked scales 0.625 (40960) and
# 1.0 (65536): 75 -> 0.625, 100/125/150 -> 1.0, fit -> the largest baked scale that fits (640x256 and the RTG 800x600
# both give 0.625; 1280x800 gives 1.0), and Ctrl+Plus cannot leave the baked set.
foreach(case "75;1280x800;40960" "100;1280x800;65536" "125;1280x800;65536" "150;1280x800;65536" "fit;640x256;40960" "fit;800x600;40960" "fit;1280x800;65536")
  list(GET case 0 z)
  list(GET case 1 cv)
  list(GET case 2 q)
  dui_run(baked --ui desktop --backend mem --view home --clock fixed --stats --baked-zoom --zoom ${z} --canvas ${cv})
  if(NOT baked MATCHES "ui_scale_q16=${q} ui_zoom=${z} ")
    message(FATAL_ERROR "baked tier zoom ${z} on ${cv} must give ui_scale_q16=${q}:\n${baked}")
  endif()
endforeach()
file(WRITE "${WORK}/keys.txt" "key ctrl++\nkey ctrl++\nkey ctrl++\n")
dui_run(bakedkeys --ui desktop --backend mem --view home --clock fixed --stats --baked-zoom --zoom 100 --canvas 1280x800 --input "${WORK}/keys.txt")
if(NOT bakedkeys MATCHES "ui_scale_q16=65536 ui_zoom=100 ")
  message(FATAL_ERROR "baked tier Ctrl+Plus must stay on 1.0:\n${bakedkeys}")
endif()
