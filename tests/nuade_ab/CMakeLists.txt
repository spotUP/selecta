# T15 nuade-ab. Stock nuade (own float mixer, host only) is built here as
# its own library, apart from selecta_nuade, so symbols never collide. Only
# the protracker4 player is compiled in.
set(NUADE_DIR "${PROJECT_SOURCE_DIR}/third_party/nuade")
set(NUADE_PLAYERS "protracker4")
include("${NUADE_DIR}/cmake/nuade_players.cmake")
add_library(nuade_stock STATIC
  "${NUADE_DIR}/src/nuade_file_index.c" "${NUADE_DIR}/src/nuade_grid.c"
  "${NUADE_DIR}/src/nuade_io_default.c" "${NUADE_DIR}/src/nuade_mixer.c"
  "${NUADE_DIR}/src/nuade_paula_channel.c" "${NUADE_DIR}/src/nuade_path.c"
  "${NUADE_DIR}/src/nuade_registry.c" "${NUADE_DIR}/src/nuade_scheduler.c"
  "${NUADE_DIR}/src/nuade_song.c" "${NUADE_DIR}/src/nuade_trace.c"
  "${NUADE_DIR}/src/nuade_tracker.c" "${NUADE_DIR}/src/nuade_viz.c")
nuade_resolve_players(NUADE_AB_PLAYER_DIRS)
nuade_add_players(nuade_stock "${NUADE_AB_PLAYER_DIRS}")
target_include_directories(nuade_stock PUBLIC "${NUADE_DIR}/include" PRIVATE "${NUADE_DIR}/src")
set_target_properties(nuade_stock PROPERTIES C_STANDARD 11 C_EXTENSIONS OFF)
target_compile_options(nuade_stock PRIVATE -w)
target_link_libraries(nuade_stock PUBLIC m)

add_executable(nuade_ab nuade_ab.c)
target_link_libraries(nuade_ab PRIVATE nuade_stock)

add_test(NAME nuade-ab
  COMMAND "${CMAKE_COMMAND}" "-DCLI=${SEL_CLI}" "-DAB=$<TARGET_FILE:nuade_ab>"
    "-DMOD=${SEL_TESTMOD}" "-DOUT=${SEL_RENDER_OUT}/nuade-ab.linear.wav"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/nuade_ab.cmake")
