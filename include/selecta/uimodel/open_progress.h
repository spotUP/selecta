/* I1415: the one text and fraction of the open progress (sel_engine_open_progress), shared by the desktop, text and MUI UIs.
 * Text: "Connecting", "Downloading 42%", "Downloading 1.2 MB" (total unknown), "Downloading file 2 of 3 - 42%",
 * "Unpacking", "Starting"; "" when idle. out always gets a NUL when n > 0. */
#ifndef SELECTA_UIMODEL_OPEN_PROGRESS_H
#define SELECTA_UIMODEL_OPEN_PROGRESS_H

#include <selecta/engine.h>
#include <stddef.h>

void sel_open_progress_text(const sel_open_progress* p, char* out, size_t n);
/* Bar fill in q16 (65536 = full) while the download has a known total; -1 = indeterminate (moving segment). */
int32_t sel_open_progress_fraction(const sel_open_progress* p);

#endif
