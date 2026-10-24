# UI model shared by the desktop UI and the custom Amiga UI (DUI-P1-4): plain C99, no float, no allocation.
add_library(selecta_uimodel STATIC uimodel.c)
target_include_directories(selecta_uimodel PUBLIC "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(selecta_uimodel PUBLIC selecta_core)
