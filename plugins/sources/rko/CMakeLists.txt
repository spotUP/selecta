# Remix.Kwed.org search source plugin (row I1407). Thin: the UHC client and the row handling are plugins/shared/source/uhc*.c.
sel_add_plugin(rko 7 rko.c)
target_link_libraries(sel_plugin_rko PUBLIC selecta_shared_source)
