# selecta_arkos: Arkos Tracker 3 song model and .aks importer (row I433a, MIT upstream, no JUCE). Plain C, no
# float, builds for every target incl. Amiga; the player and plugin follow in I433b/I433c.
add_library(selecta_arkos STATIC at3_song.c)
target_include_directories(selecta_arkos PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(selecta_arkos PUBLIC selecta_archive)
