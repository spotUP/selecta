# UI model shared by the desktop UI, the text UI and the custom Amiga UI: plain C99, no float, no allocation.
# uimodel.c is the frame model (DUI-P1-4); display.c, keys.c, keybind.c, cmdline.c, palette.c, style.c and theme.c were
# lifted out of plugins/ui/text (DUI-P1-5).
add_library(selecta_uimodel STATIC uimodel.c actions.c layout.c display.c keys.c keybind.c cmdline.c palette.c style.c theme.c budget.c tokens.c features.c doors.c)
target_include_directories(selecta_uimodel PUBLIC "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(selecta_uimodel PUBLIC selecta_core)
