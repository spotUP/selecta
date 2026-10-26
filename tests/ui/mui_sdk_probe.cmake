# mui-sdk-probe (MUI-P0-6): compiles a 10-line file including <proto/muimaster.h> with the m68k cross compiler against
# the MUI 3.8 developer headers (MUIMASTER_VLATEST must be 19, no MUI 4/5 tags). Inputs: -DCC=<m68k-amigaos-gcc>
# -DMUI_INCLUDE=<dir holding libraries/mui.h and proto/muimaster.h> -DWORK=<scratch dir>.
if(NOT EXISTS "${CC}")
  message(FATAL_ERROR "RED[mui-sdk-probe]: cross compiler ${CC} not found")
endif()
if(NOT MUI_INCLUDE OR NOT EXISTS "${MUI_INCLUDE}/libraries/mui.h" OR NOT EXISTS "${MUI_INCLUDE}/proto/muimaster.h")
  message(FATAL_ERROR "RED[mui-sdk-probe]: no MUI 3.8 developer headers (libraries/mui.h, proto/muimaster.h) under "
    "'${MUI_INCLUDE}'; set -DSEL_MUI_INCLUDE=<dir> (the bebbo toolchain in /opt/amiga ships none)")
endif()
file(READ "${MUI_INCLUDE}/libraries/mui.h" hdr)
if(NOT hdr MATCHES "MUIMASTER_VLATEST[ \t]+19")
  message(FATAL_ERROR "RED[mui-sdk-probe]: ${MUI_INCLUDE}/libraries/mui.h is not the MUI 3.8 header (VLATEST != 19)")
endif()
file(MAKE_DIRECTORY "${WORK}")
file(WRITE "${WORK}/probe.c" "#include <exec/types.h>\n#include <libraries/mui.h>\n#include <proto/muimaster.h>\n#include <proto/exec.h>\nstruct Library* MUIMasterBase;\nint main(void) {\n  MUIMasterBase = OpenLibrary(MUIMASTER_NAME, 19);\n  if (MUIMasterBase == 0) return 1;\n  CloseLibrary(MUIMasterBase);\n  return 0;\n}\n")
execute_process(COMMAND "${CC}" -noixemul -m68020 -I "${MUI_INCLUDE}" -c "${WORK}/probe.c" -o "${WORK}/probe.o"
  RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "RED[mui-sdk-probe]: compile failed (${rc}): ${err}")
endif()
