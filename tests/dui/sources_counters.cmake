# dui-sources-counters (I546): --stats reports how many SOURCES rows the engine queued and how many events it dropped.
# The Amiga showed an empty Sources tab; these two counters decide whether rows were queued and lost (evt_dropped > 0)
# or never requested (sources_listed 0). Entry point: the shell, `--exec SOURCES` through the real engine and desktop UI.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
dui_run(out --db "${WORK}/library.db" --ui desktop --backend mem --view library --canvas 640x256 --ydiv 2 --clock fixed --stats)
if(NOT out MATCHES "sources_listed=([0-9]+) evt_dropped=([0-9]+)")
  message(FATAL_ERROR "no sources_listed line:\n${out}")
endif()
# The launch asks SOURCES once (sync on launch): one row per registered source (9 on the host build: 8 builtin + the test plugin).
if(NOT CMAKE_MATCH_1 GREATER 0)
  message(FATAL_ERROR "the UI's launch SOURCES queued no rows: sources_listed=${CMAKE_MATCH_1}")
endif()
if(NOT CMAKE_MATCH_2 EQUAL 0)
  message(FATAL_ERROR "events were dropped: evt_dropped=${CMAKE_MATCH_2}")
endif()
