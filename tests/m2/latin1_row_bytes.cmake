# I218d2 amiga-console-latin1-name-box-glyph: on the Replay "Björk Ärger.mod" drew o-umlaut and A-umlaut as box
# glyphs. This pins the bytes the Amiga `Selecta` sends the console for that list row, captured with --frame-dump
# under vamos (vamos shows the 68k the Latin-1 name bytes 0xF6 / 0xC4, as a real AmigaDOS volume holds them):
# the row must go out as the Latin-1 bytes 42 6a f6 72 6b 20 c4 72 67 65 72 (one byte per letter, what the Topaz
# font draws as o-umlaut and A-umlaut), never as UTF-8 (c3 b6 / c3 84) and never as '?' (U+FFFD from a raw
# Latin-1 name reaching the UTF-8 decoder). Sentinel ui_frames=.
# Inputs: VAMOS AMIGA_SELECTA WORK
include("${CMAKE_CURRENT_LIST_DIR}/lib.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/songs")
file(WRITE "${WORK}/songs/Björk Ärger.mod" "x")
file(WRITE "${WORK}/q.keys" "Down\nwait 200\nq\n") # Down ends the I355 start screen, which covers the list until a key
execute_process(COMMAND "${VAMOS}" -C 68020 -m 8192 -V work:${WORK} -- "${AMIGA_SELECTA}" --size 80x25
    --start-dir work:songs --keys work:q.keys --frame-dump work:frames.bin
  OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "RED[I218d2]: Selecta under vamos exited ${rc}\n${out}${err}")
endif()
m2_check(I218d2 "${out}${err}" "ui_frames>=1")
file(READ "${WORK}/frames.bin" hex HEX)
string(FIND "${hex}" "426af6726b20c47267" at)
if(at EQUAL -1)
  string(FIND "${hex}" "726765722e6d6f64" tail_at)
  set(row "")
  if(NOT tail_at EQUAL -1)
    math(EXPR from "${tail_at} - 40")
    if(from LESS 0)
      set(from 0)
    endif()
    string(SUBSTRING "${hex}" ${from} 56 row)
  endif()
  message(FATAL_ERROR "RED[I218d2]: the row 'Björk Ärger.mod' is not sent as Latin-1 (want 42 6a f6 72 6b 20 c4 72 67); "
    "bytes around 'rger.mod': ${row}")
endif()
message(STATUS "[OK] I218d2")
