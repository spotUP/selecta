# AMP (Amiga Music Preservation) search source plugin (row I1407). Thin: the UHC client is plugins/shared/source/uhc*.c; the
# row filter uses the player's extension table (selecta_platform).
sel_add_plugin(amp 7 amp.c amp_row.c)
target_link_libraries(sel_plugin_amp PUBLIC selecta_shared_source selecta_platform)
