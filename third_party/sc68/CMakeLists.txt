# selecta_sc68: sc68 (third_party/sc68, GPL-3) as a static library for the SNDH module (row I427b). Only plugins/replayers/sndh
# links it, and only when SELECTA_GPL_PLUGINS is ON (design 2026-10-30-plugin-loader-sdk D5). Built from upstream's
# own source list (file68 + unice68 unpack + libsc68 + emu68 + io68) with the hand-written config in config/.
set(SC68_DIR "${CMAKE_CURRENT_LIST_DIR}")
file(GLOB SC68_FILE68 "${SC68_DIR}/file68/src/*.c")
file(GLOB SC68_EMU68 "${SC68_DIR}/libsc68/emu68/*.c")
file(GLOB SC68_IO68 "${SC68_DIR}/libsc68/io68/*.c")
# The two ym_*_table.c files are #included by ym_io.c / ymemul.c, not compiled alone.
list(FILTER SC68_IO68 EXCLUDE REGEX "ym_(atarist|linear)_table\\.c$")
file(GLOB SC68_LIB "${SC68_DIR}/libsc68/src/*.c" "${SC68_DIR}/libsc68/dial68/dial68.c")
list(FILTER SC68_EMU68 EXCLUDE REGEX "/(cc68|lines68)\\.c$")
add_library(selecta_sc68 STATIC ${SC68_FILE68} ${SC68_EMU68} ${SC68_IO68} ${SC68_LIB}
  "${SC68_DIR}/unice68/unice68_unpack.c" "${SC68_DIR}/unice68/unice68_version.c")
target_include_directories(selecta_sc68 PUBLIC
  "${SC68_DIR}/config" "${SC68_DIR}/config/sc68" "${SC68_DIR}/file68" "${SC68_DIR}/libsc68" "${SC68_DIR}/unice68"
  "${SC68_DIR}/libsc68/emu68" "${SC68_DIR}/libsc68/io68" "${SC68_DIR}/file68/src" "${SC68_DIR}/file68/sc68" "${SC68_DIR}/libsc68/sc68")
target_compile_definitions(selecta_sc68 PRIVATE HAVE_CONFIG_H=1)
target_compile_options(selecta_sc68 PRIVATE -w -fvisibility=hidden)
set_target_properties(selecta_sc68 PROPERTIES C_STANDARD 99 C_EXTENSIONS ON POSITION_INDEPENDENT_CODE ON)
