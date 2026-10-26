# Amiga MUI UI (plan 2026-10-25-amiga-mui.md, MUI-P1-1). The mapping functions in map/ are pure C and build everywhere
# so host tests reach them; the plugin entry (mui_ui.c, the only file that will include MUI headers) is registered
# with the shell on the Amiga only, as sel_add_plugin does for text_ui and desktop_ui.
add_library(mui_map STATIC map/mui_style.c)
target_include_directories(mui_map PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/map" PRIVATE "${PROJECT_SOURCE_DIR}/include")

if(SEL_AMIGA)
  sel_add_plugin(mui_ui 5 mui_ui.c)
  target_include_directories(sel_plugin_mui_ui PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/map")
  # libmui.a (toolchain) holds the out-of-line varargs MUI_NewObject the ApplicationObject/End idiom needs (I508).
  target_link_libraries(sel_plugin_mui_ui PUBLIC mui_map mui)
endif()
