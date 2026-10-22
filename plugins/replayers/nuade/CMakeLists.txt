# selecta_nuade: the nuade framework (third_party/nuade, pinned submodule) with
# only the players M0b needs. Built as C11 with nuade's own warning set; the
# project-wide -Werror -pedantic flags are for Selecta code, not vendored code.
# Not linked: nuade_mixer.c, nuade_song.c, nuade_io_default.c (Selecta supplies
# the mixer, the song source and the I/O; see design doc section 4).
# Amiga cross build: Phase 5 (D-M2-1) ports the adapter; nothing is built here.
if(SEL_AMIGA)
  return()
endif()

set(NUADE_DIR "${PROJECT_SOURCE_DIR}/third_party/nuade")
set(NUADE_PLAYERS "protracker4")
include("${NUADE_DIR}/cmake/nuade_players.cmake")

add_library(selecta_nuade STATIC
  "${NUADE_DIR}/src/nuade_registry.c"
  "${NUADE_DIR}/src/nuade_paula_channel.c"
  "${NUADE_DIR}/src/nuade_tracker.c"
  "${NUADE_DIR}/src/nuade_file_index.c"
  "${NUADE_DIR}/src/nuade_path.c"
  "${NUADE_DIR}/src/nuade_grid.c"
  "${NUADE_DIR}/src/nuade_viz.c"
  "${NUADE_DIR}/src/nuade_scheduler.c")
nuade_resolve_players(SEL_NUADE_PLAYER_DIRS)
nuade_add_players(selecta_nuade "${SEL_NUADE_PLAYER_DIRS}")
target_include_directories(selecta_nuade
  PUBLIC "${NUADE_DIR}/include"
  PRIVATE "${NUADE_DIR}/src")
set_target_properties(selecta_nuade PROPERTIES
  C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF
  COMPILE_OPTIONS "-Wall;-Wextra")
if(UNIX)
  target_link_libraries(selecta_nuade PUBLIC m)
endif()

# The adapter sees include/selecta plus nuade's public headers, nothing else
# (T7: the sdk-isolation target compiles it with exactly those two paths).
sel_add_plugin(nuade 1 adapter.c voices_shim.c io_shim.c
  THIRD_PARTY_INCLUDES "${NUADE_DIR}/include")
target_link_libraries(sel_plugin_nuade PUBLIC selecta_nuade)
