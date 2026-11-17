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
/* The path one pick of row `name` opens from the level `page` (SEL_DRILL_PAGE_*) at `path`; 0 (out empty) when the row is a leaf for now. */
int sel_uim_drill_child(uint32_t page, const char* path, const char* name, char* out, size_t cap);
/* The label a row shows: root names capitalised, platform ids as their family label, the empty artist as "Unknown artist". */
void sel_uim_drill_label(uint32_t page, const char* path, const char* name, char* out, size_t cap);
/* The COLLECTION_OPEN key of a level: source id, path, then RS skip and GS find text when set. */
void sel_uim_drill_key(const char* source, const char* path, uint32_t skip, const char* find, char* out, size_t cap);
/* The path one level up from `path`; "" is the first level (also the answer for a first-level path). */
void sel_uim_drill_up(const char* path, char* out, size_t cap);

#ifdef __cplusplus
}
#endif
#endif
