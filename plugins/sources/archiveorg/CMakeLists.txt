# Internet Archive streaming source plugin (row I725d). Uses only the host net and catalogue services.
sel_add_plugin(archiveorg 7 archiveorg.c)
target_link_libraries(sel_plugin_archiveorg PUBLIC selecta_shared_source)
