# Text UI: cell grid with diff flush, key parser, theme tokens (pure C99) and
# the POSIX terminal device (the only file with OS headers, rule 3).
add_library(selecta_textui STATIC grid.c keys.c keybind.c cmdline.c quote.c palette.c theme.c)
# One terminal device and one spawner per OS family (rule 3, rule 7).
if(SEL_AMIGA)
  target_sources(selecta_textui PRIVATE amiga/term.c amiga/spawn.c)
else()
  target_sources(selecta_textui PRIVATE posix/term.c posix/spawn.c)
endif()
target_include_directories(selecta_textui PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

# The UI plugin entry (row I60); the shell reaches it through the registry.
sel_add_plugin(text_ui 5 text_ui.c)
target_link_libraries(sel_plugin_text_ui PUBLIC selecta_textui)
target_link_libraries(selecta_textui PUBLIC selecta_core)
