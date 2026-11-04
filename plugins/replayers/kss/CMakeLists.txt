# libkss replayer (I698): libkss (third_party/libkss, permissive licence, plain C) behind a RENDER adapter for MSX
# music: .kss, MoonBlaster .mbm, MGSDRV .mgs, MuSICA .bgm, OPLLDRV .opx, MPK .mpk. The library wraps the driver
# formats in a KSS image together with the MSX driver binary, and those binaries (third_party/libkss/modules/drivers)
# carry no redistribution grant: the plugin builds only with SELECTA_KSS_DRIVERS=ON (see plugins/CMakeLists.txt), and a
# build without it contains neither the plugin nor the binaries. Loadable module only (host); the Amiga build has no
# non-GPL replayer module path yet (exemption in ledger row I698, RULES 4a).
set(KSS_DIR "${PROJECT_SOURCE_DIR}/third_party/libkss")

# kssload.c reads gzip-compressed files through zlib; the adapter hands libkss bytes (KSS_bin2kss), so it stays out.
file(GLOB SEL_KSS_SOURCES CONFIGURE_DEPENDS "${KSS_DIR}/src/*.c" "${KSS_DIR}/src/kss/*.c" "${KSS_DIR}/src/filters/*.c"
  "${KSS_DIR}/src/rconv/*.c" "${KSS_DIR}/src/vm/*.c")
list(FILTER SEL_KSS_SOURCES EXCLUDE REGEX "kssload\\.c$|kss2vgm\\.c$")
foreach(m emu2149 emu2212 emu2413 emu76489 emu8950 kmz80)
  file(GLOB _src "${KSS_DIR}/modules/${m}/*.c")
  list(APPEND SEL_KSS_SOURCES ${_src})
endforeach()

add_library(selecta_kss STATIC ${SEL_KSS_SOURCES})
target_include_directories(selecta_kss SYSTEM PUBLIC "${KSS_DIR}/src" "${KSS_DIR}/modules")
target_compile_options(selecta_kss PRIVATE -w -fvisibility=hidden)
set_target_properties(selecta_kss PROPERTIES C_STANDARD 11 C_EXTENSIONS ON POSITION_INDEPENDENT_CODE ON)

set(SEL_MODULE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/modules")
sel_add_plugin(kss 1 MODULE adapter.c)
target_link_libraries(sel_module_kss PRIVATE selecta_kss)
