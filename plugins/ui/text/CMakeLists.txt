# Text UI: cell grid with diff flush, key parser, theme tokens (pure C99) and
# the POSIX terminal device (the only file with OS headers, rule 3).
add_library(selecta_textui STATIC grid.c keys.c theme.c posix/term.c)
target_include_directories(selecta_textui PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
