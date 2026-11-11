# Host-built bake tools for cross builds (I1315). The Amiga cross build cannot run a tool it builds, so the bake tools
# that write generated C (icon masks, text atlases) are compiled with the host C compiler as a build step, the same
# pattern src/CMakeLists.txt uses for gen_sinc_table and friends. A fresh Amiga configure therefore needs no prebuilt
# host tree; -DSELECTA_ICONBAKE_EXE / -DSELECTA_FONTBAKE_EXE still override with a prebuilt tool.
#
# Before I1315 the desktop plugin required those two paths and stopped configure without them, which broke the
# documented Amiga configure once SEL_DESKTOP_AGA defaulted to ON (I441).
#
# Each function adds the custom command in the calling directory and returns the tool path in <out_var>; callers list
# that path in the DEPENDS of the command that runs it.
find_program(SEL_HOST_CC NAMES cc clang gcc REQUIRED)

# gui_iconbake: Lucide SVGs -> run-length packed alpha masks (desktop UI) or MUI bodies (--mui, MCC package).
function(sel_host_iconbake out_var)
  set(exe "${CMAKE_CURRENT_BINARY_DIR}/gui_iconbake_host")
  set(srcs "${PROJECT_SOURCE_DIR}/tools/gui_iconbake.c" "${PROJECT_SOURCE_DIR}/src/gui/widgets/gui_icon_pack.c")
  add_custom_command(
    OUTPUT "${exe}"
    COMMAND "${SEL_HOST_CC}" -O1 -I "${PROJECT_SOURCE_DIR}/include" -o "${exe}" ${srcs} -lm
    DEPENDS ${srcs}
    COMMENT "Building the host icon baker")
  set(${out_var} "${exe}" PARENT_SCOPE)
endfunction()

# gui_fontbake: Inter and the Amiga bitmap fonts -> deflated text atlases (desktop UI, I463/I587). Same sources as the
# host gui_fontbake target; miniz is compiled in directly because selecta_archive is a cross-compiled library here.
function(sel_host_fontbake out_var)
  set(exe "${CMAKE_CURRENT_BINARY_DIR}/gui_fontbake_host")
  set(srcs "${PROJECT_SOURCE_DIR}/tools/gui_fontbake.c" "${PROJECT_SOURCE_DIR}/plugins/ui/desktop/atlas_build.c"
    "${PROJECT_SOURCE_DIR}/plugins/ui/desktop/amiga_font.c" "${PROJECT_SOURCE_DIR}/src/gui/widgets/gui_icon_pack.c"
    "${PROJECT_SOURCE_DIR}/third_party/miniz/miniz.c")
  add_custom_command(
    OUTPUT "${exe}"
    COMMAND "${SEL_HOST_CC}" -O1 -w -DMINIZ_NO_ZLIB_COMPATIBLE_NAMES -I "${PROJECT_SOURCE_DIR}/include"
      -I "${PROJECT_SOURCE_DIR}/plugins/ui/desktop" -I "${PROJECT_SOURCE_DIR}/third_party/stb"
      -I "${PROJECT_SOURCE_DIR}/third_party/miniz" -o "${exe}" ${srcs} -lm
    DEPENDS ${srcs} "${PROJECT_SOURCE_DIR}/plugins/ui/desktop/atlas_build.h"
    COMMENT "Building the host font baker")
  set(${out_var} "${exe}" PARENT_SCOPE)
endfunction()
