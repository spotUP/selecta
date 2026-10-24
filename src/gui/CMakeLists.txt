# Integer-only drawing code for the desktop UI (plan 2.5, 2.11): no float or double under src/gui.
add_library(selecta_gui STATIC draw/gui_draw.c text/gui_text.c image/gui_image.c image/gui_genart.c widgets/gui_widget.c)
target_include_directories(selecta_gui PUBLIC "${PROJECT_SOURCE_DIR}/include")
