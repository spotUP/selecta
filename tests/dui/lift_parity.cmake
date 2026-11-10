# dui-lift-parity (DUI-P1-5): the code lifted out of plugins/ui/text lives in selecta_uimodel only, and the desktop UI
# shows the same clock and stage strings as the text UI.
#   -DSELECTA=<exe> -DSONGS=<dir> -DWORK=<dir> -DNM=<nm> -DTEXTUI=<libsel_plugin_text_ui.a> -DTEXTGRID=<libselecta_textui.a>
#   -DUIMODEL=<libselecta_uimodel.a>
# The text UI screen goldens (tests/m1/*.screen, tests/m12/*.screen) stay byte-identical: they run in their own
# tests (selecta-browse-play, selecta-m12-reach, ...), which this item keeps green. Here: the symbol counts, taken
# from the archives (nm), not from grep, and the shared strings.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")

set(lifted txt_bindings_default txt_bindings_load txt_bindings_lookup txt_bindings_key_for txt_key_parse
  txt_key_name txt_action_known txt_cmdline_open txt_cmdline_key txt_palette_open txt_palette_key txt_theme_default
  txt_theme_parse txt_role_name txt_style_make txt_keys_feed sel_uim_clock sel_uim_play_word sel_uim_stage_status
  sel_uim_queue_title sel_uim_tag_label sel_uim_semitone sel_uim_level_glyph sel_uim_note_name)
execute_process(COMMAND "${NM}" -g --defined-only "${TEXTUI}" "${TEXTGRID}" OUTPUT_VARIABLE in_text RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "nm failed on the text UI archives")
endif()
execute_process(COMMAND "${NM}" -g --defined-only "${UIMODEL}" OUTPUT_VARIABLE in_model RESULT_VARIABLE rc)
set(missing "")
set(left "")
set(count 0)
foreach(fn IN LISTS lifted)
  if(in_model MATCHES "[ _]${fn}\n")
    math(EXPR count "${count} + 1")
  else()
    list(APPEND missing ${fn})
  endif()
  if(in_text MATCHES "[ _]${fn}\n")
    list(APPEND left ${fn})
  endif()
endforeach()
if(NOT "${left}" STREQUAL "")
  message(FATAL_ERROR "lifted functions still defined by the text UI: ${left}")
endif()
if(NOT "${missing}" STREQUAL "")
  message(FATAL_ERROR "lifted functions missing from selecta_uimodel: ${missing}")
endif()

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
file(WRITE "${WORK}/wait.keys" "wait 200\n")
set(common --keys "${WORK}/wait.keys" --exec "QUEUE_ADD ${SONGS}/selecta-test.mod" --exec PLAY
  --exec "FILTER_LED on" --exec "MIX_STEREO_WIDTH 40")
dui_run(text --ui text --screen-dump "${WORK}/text.screen" ${common})
dui_run(desk --ui desktop --backend mem --stats ${common})
file(READ "${WORK}/text.screen" screen)
# I1222g: the text UI player bar is one row at 80x24 (title, play word, clock / length, volume); the stage status only shows on the
# two-row bar, so the text UI is checked for the clock and the desktop status carries the stage settings from the shared frame.
if(NOT screen MATCHES ">  ([0-9][0-9]:[0-9][0-9]) / [^\n]*Volume 100")
  message(FATAL_ERROR "the text UI player bar lacks the clock:\n${screen}")
endif()
set(text_clock "${CMAKE_MATCH_1}")
set(stage "LED Filter On")
if(NOT desk MATCHES "desktop_ui closed [^\n]* clock=([^ \n]+) status=([^\n]*)\n")
  message(FATAL_ERROR "the desktop UI printed no status:\n${desk}")
endif()
set(desk_clock "${CMAKE_MATCH_1}")
set(desk_status "${CMAKE_MATCH_2}")
if(NOT desk_clock STREQUAL text_clock)
  message(FATAL_ERROR "clock differs: desktop '${desk_clock}', text '${text_clock}'")
endif()
if(NOT desk_status STREQUAL "Playing  ${stage}  Stereo Width 40")
  message(FATAL_ERROR "status differs: desktop '${desk_status}', want 'Playing  ${stage}  Stereo Width 40'")
endif()
