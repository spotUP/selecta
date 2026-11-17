/* The source drill pages (I790, I1235, I1442b): the path, label and key rules every UI shares. The engine builds the pages
 * (engine.c drill_build); this file only names the levels. Pure C99. */
#ifndef SELECTA_UIMODEL_DRILL_H
#define SELECTA_UIMODEL_DRILL_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SEL_UIM_DRILL_PART 96u

/* Splits a drill path (kind US a US b) into at most 3 parts of SEL_UIM_DRILL_PART bytes; returns the part count. */
uint32_t sel_uim_drill_split(const char* path, char (*part)[SEL_UIM_DRILL_PART]);
/* The path one pick of a row opens is sel_uim_drill_child (browse.h). */
/* The label a row shows: root names capitalised, platform ids as their family label, the empty artist as "Unknown artist". */
void sel_uim_drill_label(uint32_t page, const char* path, const char* name, char* out, size_t cap);
/* The COLLECTION_OPEN key of a level: source id, path, then RS skip and GS find text when set. */
void sel_uim_drill_key(const char* source, const char* path, uint32_t skip, const char* find, char* out, size_t cap);
/* The path one level up from `path`; "" is the first level (also the answer for a first-level path). */
void sel_uim_drill_up(const char* path, char* out, size_t cap);

/* 1 when the engine filters this level's whole list before windowing (formats, artists, format US name). */
int sel_uim_drill_engine_find(const char* path);
/* 1 when the find field shows on this level: engine-find levels while groups are loaded or a find is active; other group levels
 * only when the whole level is one window (skip 0, n < SEL_DRILL_WINDOW), so a local filter never hides rows. */
int sel_uim_drill_filterable(uint32_t page, const char* path, uint32_t n, uint32_t skip, const char* find);
/* Crumb labels (96 bytes each) and the path each reopens: source name, kind, up to 2 more levels; returns the count. */
uint32_t sel_uim_drill_crumbs(const char* source_name, const char* path, char (*label)[96], char (*cpath)[160]);
/* Splits a route / COLLECTION_OPEN key into source, path, skip and find (inverse of sel_uim_drill_key). */
void sel_uim_drill_key_parse(const char* key, char* source, size_t scap, char* path, size_t pcap, uint32_t* skip, char* find, size_t fcap);
/* The count column text: "N" at the root, "N songs" below it, "" when unknown. */
void sel_uim_drill_count(uint32_t page, const char* count, char* out, size_t cap);

#ifdef __cplusplus
}
#endif
#endif
