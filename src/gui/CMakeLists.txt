# Integer-only drawing code for the desktop UI (plan 2.5, 2.11): no float or double under src/gui.
add_library(selecta_gui STATIC draw/gui_draw.c)
target_include_directories(selecta_gui PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/draw")
