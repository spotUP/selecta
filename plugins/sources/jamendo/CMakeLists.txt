# Jamendo streaming source plugin (row I725e). Uses only the host's net and catalogue services.
sel_add_plugin(jamendo 7 jamendo.c)
target_link_libraries(sel_plugin_jamendo PUBLIC selecta_shared_source)
