# Writes the KPlayer binary IN as a writable, word-aligned C array to OUT (the replayer writes its own
# k_baseaddr field, so the array is not const).
file(READ "${IN}" hex HEX)
string(LENGTH "${hex}" len)
math(EXPR size "${len} / 2")
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," body "${hex}")
file(WRITE "${OUT}" "/* Generated from kpl14.s by bin2c.cmake; do not edit. */\n#include <stdint.h>\n"
  "extern uint8_t sel_kpl_code[];\nextern const uint32_t sel_kpl_size;\n"
  "uint8_t sel_kpl_code[] __attribute__((aligned(4))) = {${body}};\n"
  "const uint32_t sel_kpl_size = ${size}u;\n")
