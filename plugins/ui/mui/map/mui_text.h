/* I1139: the one text boundary between Selecta and MUI. The uimodel, the library and the engine hand out UTF-8; MUI on AmigaOS 3
 * draws Latin-1. Every uimodel string that reaches a MUI or UP- object passes through this file on Selecta's side:
 *   - new of UP- text, button and field specs (mui_aawrap.c) and the set calls (mui_ui.c) through the *_spec_l1 functions / mui_l1,
 *   - list row and cell hook results (map/mui_aatagmap.c), converted into the hook's own buffer,
 *   - strings MUI keeps by pointer (menu and window titles, ShortHelp, requesters) through mui_l1_keep / mui_l1.
 * The UP- classes themselves (plugins/shared/mcc) take Latin-1 STRPTRs untouched: a third party app's "Bj\xF6rk" must arrive as
 * 0xF6, so nothing in the package converts. Text typed into a string gadget comes back as Latin-1; mui_l1_to_utf8 undoes it. */
#ifndef MUI_TEXT_H
#define MUI_TEXT_H

#include <stddef.h>

#include "mui_aabtn_class.h"
#include "mui_aafield_class.h"
#include "mui_aatext_class.h"

#define MUI_L1_TEXT 160 /* the UP- text class holds 160 bytes, the button class 48, a hook cell 256 */

/* `s` as Latin-1: pure ASCII (or NULL) comes back as `s` itself, anything else is converted into buf[cap] (never fails, always
 * NUL terminated, cut at a whole character; U+2022 bullet to 0xB7). */
const char* mui_l1(char* buf, size_t cap, const char* s);

/* Same, into storage that lives until mui_text_free (MUI keeps menu and window titles by pointer). */
const char* mui_l1_keep(const char* s);
void mui_text_free(void);

/* Spec copies whose strings are Latin-1; a, b, c are the caller's buffers of MUI_L1_TEXT bytes. */
void mui_aatext_spec_l1(const mui_aatext_spec* in, mui_aatext_spec* out, char* a, char* b);
void mui_aabtn_spec_l1(const mui_aabtn_spec* in, mui_aabtn_spec* out, char* a, char* b, char* c);
void mui_aafield_spec_l1(const mui_aafield_spec* in, mui_aafield_spec* out);

/* Latin-1 text read back from a string gadget to UTF-8 for the uimodel (dst[cap], NUL terminated). */
size_t mui_l1_to_utf8(char* dst, size_t cap, const char* src);

#endif
