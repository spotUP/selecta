# Turns a binary file into a C array: cmake -DIN=<file> -DOUT=<file.c> -DSYM=<name> -P embed_blob.cmake.
# Emits `const unsigned char <SYM>[]` and `const unsigned long <SYM>_len`. Used for the Inter fonts (DUI-P2-3).
if(NOT DEFINED IN OR NOT DEFINED OUT OR NOT DEFINED SYM)
  message(FATAL_ERROR "embed_blob.cmake: -DIN, -DOUT and -DSYM are required")
endif()
file(READ "${IN}" hex HEX)
string(LENGTH "${hex}" hexlen)
math(EXPR bytes "${hexlen} / 2")
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," hex "${hex}")
string(REGEX REPLACE "((0x..,){24})" "\\1\n" hex "${hex}")
file(WRITE "${OUT}" "/* Generated from ${IN} by cmake/embed_blob.cmake. */\nconst unsigned char ${SYM}[] = {\n${hex}\n};\nconst unsigned long ${SYM}_len = ${bytes}UL;\n")
