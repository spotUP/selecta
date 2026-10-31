# Modules.pl search source plugin (row I406). Uses only the host's net and catalogue services.
sel_add_plugin(modulespl 7 modulespl.c modulespl_parse.c)
target_link_libraries(sel_plugin_modulespl PUBLIC selecta_shared_source)
