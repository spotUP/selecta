# Local file browse (plan M1.1 phase 4). The directory iterator is the only
# OS-specific part and lives under src/core/<os>/ (rule 3); macOS and Linux
# share the POSIX file.
if(SEL_AMIGA)
  set(SEL_DIR_SOURCE ${PROJECT_SOURCE_DIR}/src/core/amiga/dir.c)
else()
  set(SEL_DIR_SOURCE ${PROJECT_SOURCE_DIR}/src/core/macos/dir.c)
endif()
# Playlist codecs (plan M1.2 phase 3): portable C, no OS code.
add_library(selecta_library STATIC local.c ${SEL_DIR_SOURCE}
  playlist/playlist.c playlist/formats/hippo.c playlist/formats/m3u.c playlist/formats/pls.c playlist/formats/xspf.c export/wav.c export/svx.c)
target_include_directories(selecta_library PUBLIC ${PROJECT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR})
