/* Plugin module loader (design 2026-10-30-plugin-loader-sdk D1, D3). Internal to the registry. */
#ifndef SELECTA_ENGINE_LOADER_H
#define SELECTA_ENGINE_LOADER_H

/* Scans the module search path ($SELECTA_PLUGIN_PATH, the app plugin folder, the user plugin folder),
 * opens every sel_* module and hands its sel_plugin_entry to sel_registry_add_entry. Rejected modules go to
 * sel_registry_note_rejected. Handles stay open for the process lifetime. Called once, by registry init. */
void sel_loader_scan(void);

#endif
