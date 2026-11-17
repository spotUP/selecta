# selecta_arkos: Arkos Tracker 3 song model and .aks importer (row I433a, MIT upstream, no JUCE). Plain C, no
# float, builds for every target incl. Amiga.
add_library(selecta_arkos STATIC at3_song.c)
target_include_directories(selecta_arkos PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(selecta_arkos PUBLIC selecta_archive)

# I433b: the replayer plugin (adapter.c + at3_player.c, a C port of the AT3 player, MIT, VERSION.txt) on Ayumi (MIT).
# Ayumi runs in double: SEL_RP_NEEDS_FLOAT, soft-float objects on the Amiga (the organya pattern). On the Amiga the
# player is the LoadSeg module Players/sel_arkos; the registry links only the probe.c manifest (the dr_flac pattern).
set(AYUMI_DIR "${PROJECT_SOURCE_DIR}/third_party/ayumi")
if(SEL_AMIGA)
  sel_add_plugin(arkos 1 adapter.c at3_player.c "${AYUMI_DIR}/ayumi.c" probe.c AMIGA_MANIFEST probe.c
    THIRD_PARTY_INCLUDES "${AYUMI_DIR}")
  target_link_libraries(sel_seg_arkos PRIVATE selecta_arkos m)
else()
  sel_add_plugin(arkos 1 adapter.c at3_player.c "${AYUMI_DIR}/ayumi.c" probe.c THIRD_PARTY_INCLUDES "${AYUMI_DIR}")
  target_link_libraries(sel_plugin_arkos PUBLIC selecta_arkos m)
  target_link_libraries(sel_plugin_arkos PUBLIC selecta_plugin_shared)
endif()
