# T23: the nuade adapter's registry walk reports every player in the tree.
# Variables: CLI NUADE_DIR
# N is counted here from the players/*/nuade_player.cmake glob at test time,
# never typed; the adapter's count comes from nuade_registry_players.
file(GLOB found "${NUADE_DIR}/players/*/nuade_player.cmake")
list(LENGTH found expected)
if(expected EQUAL 0)
  message(FATAL_ERROR "no nuade players found under ${NUADE_DIR}/players")
endif()
execute_process(COMMAND "${CLI}" plugins --formats nuade
  OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "selecta-cli plugins --formats nuade exited ${rc}\nstdout:\n${out}\nstderr:\n${err}")
endif()
if(NOT out MATCHES "(^|\n)nuade_players=([0-9]+)\n")
  message(FATAL_ERROR "no 'nuade_players=N' line:\n${out}")
endif()
if(NOT CMAKE_MATCH_2 EQUAL expected)
  message(FATAL_ERROR "nuade_players=${CMAKE_MATCH_2}, glob of players/*/nuade_player.cmake has ${expected}")
endif()
# One line per player: "player <id>". The glob names must all be listed.
foreach(f IN LISTS found)
  get_filename_component(dir "${f}" DIRECTORY)
  get_filename_component(id "${dir}" NAME)
  if(NOT out MATCHES "(^|\n)player ${id}\n")
    message(FATAL_ERROR "player '${id}' missing from the listing")
  endif()
endforeach()
foreach(id pokeynoise davidhanney majortom)
  if(NOT out MATCHES "(^|\n)player ${id}\n")
    message(FATAL_ERROR "required player '${id}' missing")
  endif()
endforeach()
