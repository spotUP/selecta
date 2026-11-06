# StoneTracker replayer (I710ae): .spm songs + .sps sample banks. The authors' no-OS StonePlayer_Hard.bin (a user system file, never shipped)
# runs on Musashi's 68020 (third_party/stonetracker_host, MIT) in a minimal Amiga with a software Paula. Permissive, so a core module.
# Host only for now: the Amiga build runs the same bin on the real CPU (row I710aea), so Musashi is not needed there.
set(ST_DIR "${PROJECT_SOURCE_DIR}/third_party/stonetracker_host")
set(ST_GEN "${CMAKE_CURRENT_BINARY_DIR}/musashi-gen")
file(MAKE_DIRECTORY "${ST_GEN}")
add_executable(st_m68kmake EXCLUDE_FROM_ALL "${ST_DIR}/musashi/m68kmake.c")
add_custom_command(OUTPUT "${ST_GEN}/m68kops.c" "${ST_GEN}/m68kops.h"
  COMMAND st_m68kmake "${ST_GEN}" "${ST_DIR}/musashi/m68k_in.c" > /dev/null
  DEPENDS st_m68kmake "${ST_DIR}/musashi/m68k_in.c")
add_library(stonetracker_host STATIC EXCLUDE_FROM_ALL
  "${ST_DIR}/musashi/m68kcpu.c" "${ST_GEN}/m68kops.c" "${ST_DIR}/musashi/softfloat/softfloat.c"
  "${ST_DIR}/src/amiga_host.c" "${ST_DIR}/src/paula_soft.c" "${ST_DIR}/src/st_machine.c" "${ST_DIR}/src/st_dhuf.c")
target_include_directories(stonetracker_host PUBLIC "${ST_DIR}/src" PRIVATE "${ST_GEN}" "${ST_DIR}/musashi")
target_compile_definitions(stonetracker_host PRIVATE MUSASHI_CNF="m68kconf_host.h")
target_compile_options(stonetracker_host PRIVATE -w -fvisibility=hidden)
set_target_properties(stonetracker_host PROPERTIES POSITION_INDEPENDENT_CODE ON C_STANDARD 99)

set(SEL_MODULE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/modules")
sel_add_plugin(stonetracker 1 MODULE adapter.c)
target_link_libraries(sel_module_stonetracker PRIVATE stonetracker_host)
