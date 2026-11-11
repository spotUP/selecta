# Native playsid.library replayer (I434b), Amiga only. HW kind: the library plays through its own audio path.
# playsid.library is NOT bundled; it is opened from LIBS: and the plugin steps aside when it is missing.
# An on-demand LoadSeg module (I427e4, the bbmp_ptk pattern): Players/sel_playsid holds the adapter, the pref map and the
# LVO layer; the registry links only probe.c as the manifest (PSID/RSID header plus an OpenLibrary presence check).
sel_add_plugin(playsid 1 adapter.c playsid_map.c playsid_amiga.c probe.c AMIGA_MANIFEST probe.c)
