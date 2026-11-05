# EUPPlayer replayer (I701): FM Towns Euphony (.eup) songs through eupmini (Tomoaki Hayasaka's EUPPlayer with the
# Win32/SDL ports by Delmonta and Giangiacomo Zaffini, third_party/eupmini). GPL-2.0, so it is its own loadable module
# under SELECTA_GPL_PLUGINS; the MIT core never links it. Host only: C++ with a tmpfile tick buffer and no Amiga module
# path (exemption in ledger row I701, RULES 4a).
enable_language(CXX)
set(EUP_DIR "${PROJECT_SOURCE_DIR}/third_party/eupmini")
add_library(eupmini_engine STATIC EXCLUDE_FROM_ALL
  "${EUP_DIR}/eupplayer.cpp" "${EUP_DIR}/eupplayer_townsEmulator.cpp" "${EUP_DIR}/eupplayer_townsInternal.cpp" "${EUP_DIR}/sintbl.cpp")
# sdl_shim/SDL.h stubs the SDL_Delay spin-wait of the upstream ring buffer; the output goes to a FILE, never SDL.
target_include_directories(eupmini_engine PUBLIC "${EUP_DIR}")
target_include_directories(eupmini_engine PRIVATE "${EUP_DIR}/sdl_shim")
target_compile_definitions(eupmini_engine PRIVATE EUPPLAYER_LITTLE_ENDIAN=1)
target_compile_options(eupmini_engine PRIVATE -w -fvisibility=hidden)
set_target_properties(eupmini_engine PROPERTIES POSITION_INDEPENDENT_CODE ON CXX_STANDARD 14)

set(SEL_MODULE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/modules")
sel_add_plugin(eupmini 1 MODULE adapter.c shim.cpp THIRD_PARTY_INCLUDES "${EUP_DIR}")
target_include_directories(sel_module_eupmini PRIVATE "${EUP_DIR}/sdl_shim")
target_compile_definitions(sel_module_eupmini PRIVATE EUPPLAYER_LITTLE_ENDIAN=1)
target_link_libraries(sel_module_eupmini PRIVATE eupmini_engine ${CMAKE_CXX_IMPLICIT_LINK_LIBRARIES})
set_target_properties(sel_module_eupmini PROPERTIES CXX_STANDARD 14)
