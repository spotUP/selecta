# UI model shared by the desktop UI, the text UI and the custom Amiga UI: plain C99, no float, no allocation.
# uimodel.c is the frame model (DUI-P1-4); display.c, keys.c, keybind.c, cmdline.c, palette.c, style.c and theme.c were
# lifted out of plugins/ui/text (DUI-P1-5).
set(SEL_UIMODEL_SOURCES uimodel.c actions.c layout.c display.c keys.c keybind.c cmdline.c palette.c style.c theme.c budget.c tokens.c features.c doors.c fkeys.c browse.c viewcmds.c sources.c panels.c select.c route.c pages.c libfilter.c sidestack.c nppanel.c rom.c cols.c about.c briefs.c)
set(SEL_UIMODEL_SOURCES ${SEL_UIMODEL_SOURCES} PARENT_SCOPE)
add_library(selecta_uimodel STATIC ${SEL_UIMODEL_SOURCES})
target_include_directories(selecta_uimodel PUBLIC "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(selecta_uimodel PUBLIC selecta_core selecta_formats)

# Amiga LoadSeg modules (I1378b) carry their own copy of the stateless UI model: same sources, no selecta_core link (the
# module reaches the engine through the host import table, cmake/amiga_seg_imports.txt). Link order is the caller's.
if(SEL_AMIGA)
  add_library(selecta_uimodel_seg STATIC ${SEL_UIMODEL_SOURCES})
  target_include_directories(selecta_uimodel_seg PUBLIC "${PROJECT_SOURCE_DIR}/include")
endif()
