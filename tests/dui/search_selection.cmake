# dui-search-selection (I1243): the search field takes a text selection. Johan 2026-11-10 / 2026-11-15: "i can't select text in the
# search inputs". Real key and pointer events through the scripted --input feeder on the desktop UI (mem backend).
# 1. Cmd+A then typing replaces the query; Cmd+A, Cmd+C, Cmd+V round-trips through the clipboard (the stats line `qsel` shows anchor,
#    caret, copies and the copied text).
# 2. Shift+Left extends the selection from the caret; Backspace deletes it.
# 3. A double click on the text selects the word; a drag across the text selects a range.
# 4. Cmd+X cuts the selection.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
set(db "${WORK}/library.db")
dui_run(init --db "${db}" --ui desktop --backend mem --view home --canvas 900x600 --clock fixed --stats)
set(common --db "${db}" --ui desktop --backend mem --view library --canvas 1280x800 --clock fixed --stats)

function(run tag script)
  file(WRITE "${WORK}/${tag}.txt" "${script}")
  file(REMOVE_RECURSE "${WORK}/cfg")
  dui_run(o ${common} --input "${WORK}/${tag}.txt")
  set(out "${o}" PARENT_SCOPE)
endfunction()

# 1. select all, copy, type over, paste back
run(a "key /\nkey a\nkey b\nkey c\nkey ctrl+a\nkey ctrl+c\nkey x\nkey y\nkey ctrl+v\nwait 600\n")
if(NOT "${out}" MATCHES "query xyabc\n" AND NOT "${out}" MATCHES "query abcxy\n")
  message(FATAL_ERROR "Cmd+A, Cmd+C, typing over, Cmd+V: expected the query to hold xy then the pasted abc:\n${out}")
endif()
if(NOT "${out}" MATCHES "qsel anchor [0-9]+ caret [0-9]+ copies 1 mouse 0 clip abc\n")
  message(FATAL_ERROR "Cmd+C must copy abc once:\n${out}")
endif()

# 2. shift+left twice then backspace deletes the last two letters
run(b "key /\nkey a\nkey b\nkey c\nkey shift+left\nkey shift+left\nkey backspace\nwait 600\n")
if(NOT "${out}" MATCHES "query a\nqsel|query a\n")
  message(FATAL_ERROR "Shift+Left twice then Backspace must leave a:\n${out}")
endif()

# 3. cut
run(c "key /\nkey a\nkey b\nkey ctrl+a\nkey ctrl+x\nwait 600\n")
if(NOT "${out}" MATCHES "query \nqsel anchor 0 caret 0 copies 1 mouse 0 clip ab\n" AND NOT "${out}" MATCHES "query \n")
  message(FATAL_ERROR "Cmd+X must cut:\n${out}")
endif()
if(NOT "${out}" MATCHES "clip ab\n")
  message(FATAL_ERROR "Cmd+X must put ab on the clipboard:\n${out}")
endif()

# 4. mouse: the field rect from a first run, then a double click and a drag over the text
run(d "key /\nkey a\nkey b\nkey c\nwait 300\n")
string(REGEX MATCH "search_field (-?[0-9]+) (-?[0-9]+) (-?[0-9]+) (-?[0-9]+)" _m "${out}")
if("${CMAKE_MATCH_0}" STREQUAL "")
  message(FATAL_ERROR "no search_field in the stats:\n${out}")
endif()
math(EXPR fx "${CMAKE_MATCH_1} + 60")
math(EXPR fy "${CMAKE_MATCH_2} + ${CMAKE_MATCH_4} / 2")
run(e "key /\nkey a\nkey b\nkey c\ndclick ${fx} ${fy}\nwait 300\n")
if(NOT "${out}" MATCHES "qsel anchor 0 caret 3 copies 0 mouse [1-9]")
  message(FATAL_ERROR "a double click on the text must select the word abc:\n${out}")
endif()
math(EXPR fx2 "${fx} + 400")
run(f "key /\nkey a\nkey b\nkey c\ndrag ${fx} ${fy} ${fx2} ${fy}\nwait 300\n")
if(NOT "${out}" MATCHES "qsel anchor [0-9] caret 3 copies 0 mouse [1-9]")
  message(FATAL_ERROR "a drag across the text must select to its end:\n${out}")
endif()
message(STATUS "dui-search-selection: select all, copy, cut, replace, shift+arrows, double click and drag verified")
