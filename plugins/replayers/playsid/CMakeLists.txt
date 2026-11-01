# Native playsid.library replayer (I434b), Amiga only. HW kind: the library plays through its own audio path.
# playsid.library is NOT bundled; it is opened from LIBS: and the plugin steps aside when it is missing.
sel_add_plugin(playsid 1 adapter.c playsid_map.c playsid_amiga.c)
