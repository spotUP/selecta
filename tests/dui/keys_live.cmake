# keys-live (I268): real SDL key events reach the desktop shell through desk_sdl_poll; a click focuses the search field. -DSELECTA -DSONGS -DWORK -DSDL_BUILT
# The sdl backend pushes the events listed in SELECTA_SDL_TEST_EVENTS on the SDL queue at open (k<keycode>/<keymod> is a
# KEY_DOWN, t<text> a TEXT_INPUT), so the offscreen run drives the real poll, the key mapping and the shell's key
# handler. The overlay line of the stats counts what opened; without the fix every count stays zero.
# 1. KEY_DOWN Ctrl+P (keycode 0x70, LCTRL 0x40) opens the palette.
# 2. TEXT_INPUT ":" opens the command line (a Swedish layout types it with Shift+period: text, not a key).
# 3. TEXT_INPUT "?" opens the help overlay.
# 4. KEY_DOWN Escape (non-printable) closes it. A printable KEY_DOWN without Ctrl or Alt (it arrives as text) is not
#    delivered a second time.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")

# 5. A click in the search field focuses it without clearing the query (scripted, mem backend): `/` then Escape leaves the
#    field, the click takes it back, and the typed text reaches the query. Without the click the text goes nowhere.
find_program(SQLITE3 sqlite3)
if(NOT SQLITE3)
  message(FATAL_ERROR "keys-live needs the sqlite3 command line tool to load the fixture library")
endif()
set(db "${WORK}/library.db")
dui_run(init --db "${db}" --ui desktop --backend mem --view home --canvas 900x600 --clock fixed --stats)
execute_process(COMMAND "${SQLITE3}" -cmd "PRAGMA trusted_schema=ON" "${db}" INPUT_FILE "${HERE}/fixture_library.sql"
  RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "loading the fixture failed: ${err}")
endif()
function(click_run tag script)
  file(REMOVE_RECURSE "${WORK}/cfg")
  file(WRITE "${WORK}/${tag}.txt" "${script}")
  dui_run(o --db "${db}" --ui desktop --backend mem --view library --canvas 1280x800 --clock fixed --stats
    --input "${WORK}/${tag}.txt")
  set(${tag}_out "${o}" PARENT_SCOPE)
endfunction()
click_run(fc "key /\nkey m\nkey escape\ntopbar 1\nkey o\nkey d\nwait 600\n")
if(NOT "${fc_out}" MATCHES "search draws [1-9][0-9]* rows [0-9]+ query mod\n")
  message(FATAL_ERROR "a click in the search field must focus it and keep the query 'm', so typing gives 'mod':\n${fc_out}")
endif()

if(NOT SDL_BUILT)
  return()
endif()
set(ENV{SDL_VIDEODRIVER} dummy)

function(live_run tag events)
  set(ENV{SELECTA_SDL_TEST_EVENTS} "${events}")
  file(REMOVE_RECURSE "${WORK}/cfg")
  dui_run(o --ui desktop --backend sdl --view queue --canvas 1280x800 --clock fixed --stats
    --exec "QUEUE_ADD ${SONGS}/selecta-test.mod")
  foreach(f palette_opens cmdline_opens help_opens kind)
    string(REGEX MATCH "overlay[^\n]* ${f}=([0-9-]+)" _m "${o}")
    if(NOT _m)
      message(FATAL_ERROR "no ${f} in:\n${o}")
    endif()
    set(${tag}_${f} "${CMAKE_MATCH_1}" PARENT_SCOPE)
  endforeach()
endfunction()

function(expect_eq what got want)
  if(NOT "${got}" STREQUAL "${want}")
    message(FATAL_ERROR "${what}: got '${got}', expected '${want}'")
  endif()
endfunction()

live_run(p "k70/40")
expect_eq("Ctrl+P KEY_DOWN opens the palette" "${p_palette_opens}" 1)
expect_eq("palette is the open overlay" "${p_kind}" 1)
live_run(c "t:")
expect_eq("TEXT_INPUT colon opens the command line" "${c_cmdline_opens}" 1)
live_run(h "t?")
expect_eq("TEXT_INPUT question mark opens help" "${h_help_opens}" 1)
live_run(e "k70/40 k1b/0")
expect_eq("Escape KEY_DOWN closes the palette" "${e_kind}" 0)
expect_eq("the palette had opened" "${e_palette_opens}" 1)
live_run(s "k3a/1 t:")
expect_eq("a printable KEY_DOWN without Ctrl or Alt is left to the text event" "${s_kind}" 3)
