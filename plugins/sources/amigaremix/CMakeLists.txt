# AmigaRemix search source plugin (row I1407). Thin: the UHC client and the row handling are plugins/shared/source/uhc*.c.
sel_add_plugin(amigaremix 7 amigaremix.c)
target_link_libraries(sel_plugin_amigaremix PUBLIC selecta_shared_source)
