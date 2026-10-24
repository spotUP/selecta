# I138b amiga-browse-nav: the Amiga `Selecta` under vamos browses work: -> songs -> sub and back up
# through ".." with AmigaDOS paths (join "Work:Music" + "Mods" = "Work:Music/Mods", parent of
# "Work:Music" = "Work:"). Asserts the screen dump text and the sentinel ui_frames=.
# The title shows the engine's browse_leaf, split on "/" and ":" ("songs", not "work:songs"; I138c);
# a volume root has no leaf and shows the path ("work:").
# Inputs: VAMOS AMIGA_SELECTA WORK
include("${CMAKE_CURRENT_LIST_DIR}/lib.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/songs/sub")
file(WRITE "${WORK}/songs/sub/z.mod" "x")
# Root work: lists songs only (no ".." row at a volume root). Enter songs, Down to sub, Enter sub.
set(down "Enter\nDown\nEnter\nwait 200\n")
file(WRITE "${WORK}/in_sub.keys" "${down}q\n")
# ".." from sub lands in songs on the sub row; Up + Enter on ".." from songs reaches the volume root.
file(WRITE "${WORK}/in_songs.keys" "${down}Enter\nwait 200\nq\n")
file(WRITE "${WORK}/in_root.keys" "${down}Enter\nUp\nEnter\nwait 200\nq\n")
# Down from songs lands on a file at the volume root: its path joins without a "/" ("work:root.mod",
# not "work:/root.mod"; I138c). The .keys files also live there but are hidden: no replayer claims
# them (I218i), so the fixture is a .mod, which the Amiga KPlayer claims (I219).
file(WRITE "${WORK}/root.mod" "x")
file(WRITE "${WORK}/root_file.keys" "Down\nwait 200\nq\n")
function(run name expect_title expect_rows forbid_rows)
  execute_process(COMMAND "${VAMOS}" -C 68020 -m 8192 -V work:${WORK} -- "${AMIGA_SELECTA}" --size 80x25
      --start-dir work: --keys work:${name}.keys --screen-dump work:${name}.txt
    OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "RED[I138b]: Selecta under vamos exited ${rc}\n${out}${err}")
  endif()
  m2_check(I138b "${out}${err}" "ui_frames>=1")
  # Optional fourth argument: the browse cursor path the engine reports (sel_engine_view_cursor_key).
  if(ARGC GREATER 4 AND NOT out MATCHES "cursor_key=${ARGV4}\n")
    message(FATAL_ERROR "RED[I138c]: ${name}: cursor_key=${ARGV4} missing (browse join).\n${out}")
  endif()
  file(READ "${WORK}/${name}.txt" dump)
  if(NOT dump MATCHES "Browse  ${expect_title}\n")
    message(FATAL_ERROR "RED[I138b]: ${name}: header 'Browse  ${expect_title}' missing (AmigaDOS join/parent).\n${dump}")
  endif()
  foreach(r ${expect_rows})
    if(NOT dump MATCHES "\n +${r}[/ \n]")
      message(FATAL_ERROR "RED[I138b]: ${name}: row '${r}' missing.\n${dump}")
    endif()
  endforeach()
  foreach(r ${forbid_rows})
    if(dump MATCHES "\n +${r}[/ \n]")
      message(FATAL_ERROR "RED[I138b]: ${name}: row '${r}' must not be listed.\n${dump}")
    endif()
  endforeach()
endfunction()
run(in_sub "sub" "\\.\\.;z.mod" "")
run(in_songs "songs" "\\.\\.;sub" "z.mod")
run(in_root "work:" "songs" "\\.\\.;sub")
run(root_file "work:" "songs;root\\.mod" "\\.\\." "work:root\\.mod")
message(STATUS "[OK] I138b")
