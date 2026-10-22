# Reads the X-macro tables under include/selecta and exports their entry
# counts. Tests take their expected numbers from here, never from a
# retyped constant (RULES.md rule 2: counted from the structure that
# produces it).

function(sel_count_def_rows file macro out_var)
  if(EXISTS "${file}")
    file(STRINGS "${file}" rows REGEX "^${macro}\\(")
    list(LENGTH rows n)
  else()
    set(n 0)
  endif()
  set(${out_var} ${n} PARENT_SCOPE)
endfunction()

set(SEL_DEF_DIR "${PROJECT_SOURCE_DIR}/include/selecta")
sel_count_def_rows("${SEL_DEF_DIR}/commands.def" SEL_CMD SEL_COUNT_CMD)
sel_count_def_rows("${SEL_DEF_DIR}/events.def"   SEL_EVT SEL_COUNT_EVT)
sel_count_def_rows("${SEL_DEF_DIR}/tags.def"     SEL_TAG SEL_COUNT_TAG)
if(EXISTS "${SEL_DEF_DIR}/tags.def")
  file(STRINGS "${SEL_DEF_DIR}/tags.def" meta_rows REGEX "^SEL_TAG\\(META,")
  list(LENGTH meta_rows SEL_COUNT_META)
else()
  set(SEL_COUNT_META 0)
endif()

# Reconfigure when a table changes so the counts stay true.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  "${SEL_DEF_DIR}/commands.def" "${SEL_DEF_DIR}/events.def" "${SEL_DEF_DIR}/tags.def")

message(STATUS "selecta tables: commands=${SEL_COUNT_CMD} events=${SEL_COUNT_EVT} tags=${SEL_COUNT_TAG} meta=${SEL_COUNT_META}")
