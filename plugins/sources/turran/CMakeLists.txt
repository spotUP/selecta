# Turran FTP search source plugin (row I1407). Thin: the UHC client is plugins/shared/source/uhc*.c; the extension filter
# uses the player's extension table (selecta_platform).
sel_add_plugin(turran 7 turran.c turran_row.c)
target_link_libraries(sel_plugin_turran PUBLIC selecta_shared_source selecta_platform)
