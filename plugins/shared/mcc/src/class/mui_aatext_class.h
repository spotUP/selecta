/* Selecta_AAText (I667d, design 2026-10-28-mui-custom-classes.md section 3.7): MUI 3.8 Area subclass that draws EVERY Selecta text
 * line (title, artist, album, clock, status, crumb, hints, settings labels) through mui_aa_draw: antialiased, in a theme token
 * colour by tone (TEXT, TEXT_MUTED, title = TEXT at font + 4 px), left, centre or right aligned, cut with "..." at the width.
 * Replaces MUIC_Text and its PreParse pen escapes. Amiga only; tone, alignment and the ellipsis cut are in map/mui_aa.c. */
#ifndef MUI_AATEXT_CLASS_H
#define MUI_AATEXT_CLASS_H

#include <exec/types.h>
#include <libraries/mui.h>
#include <stdint.h>

#define AAT_Contents (TAG_USER + 0x53410001UL) /* STRPTR copied into the object; redraws only when it changed */

typedef struct mui_aatext_spec {
    const char* contents; /* first text */
    const char* widest;   /* longest text the line will show (default width); NULL = contents */
    int tone;             /* MUI_AA_TONE_NORMAL, _MUTED or _TITLE */
    int align;            /* MUI_AA_ALIGN_LEFT, _CENTRE or _RIGHT */
    int ellipsis;         /* cut with "..." when wider than the box; the minimum width is then the dots */
    ULONG frame;          /* MUIA_Frame value, 0 = MUIV_Frame_None */
    ULONG background;     /* MUIA_Background value (spec string pointer), 0 = none */
    int fixed;            /* nonzero: never wider than the default width of widest (mui_aa_text_maxw) */
    int hidden;           /* start with MUIA_ShowMe FALSE */
} mui_aatext_spec;

Object* mui_aatext_new(const mui_aatext_spec* spec);
/* New text (AAT_Contents); redraws only on change. */
void mui_aatext_set(Object* obj, const char* contents);
/* Texts constructed since start (sentinel: equals the number of text lines the window build made). */
uint32_t mui_aatext_made(void);
void mui_aatext_class_free(void);

#endif
