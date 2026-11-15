/* I1383b: the words for a failed open, one text for the desktop toast, the text UI status line and the MUI status line.
 * The engine reports (code, detail) in SEL_EVT_ERROR: no_replayer carries "format=<ext> <key>" (I1383), open_failed may
 * carry "http_status=<n> <url>". Pure C99, no allocation. */
#ifndef SELECTA_UIMODEL_FAILTEXT_H
#define SELECTA_UIMODEL_FAILTEXT_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Writes the friendly text for a failed open into out (cap bytes, truncated) and returns 1; returns 0 and leaves out
 * alone when code is not an open-failure code (open_failed, no_replayer, unsupported_kind, replayer_incomplete,
 * replayer_load_failed). detail may be NULL. */
int sel_uim_open_fail_text(const char* code, const char* detail, char* out, size_t cap);

#ifdef __cplusplus
}
#endif

#endif
