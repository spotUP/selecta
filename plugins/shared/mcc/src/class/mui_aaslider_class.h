/* Selecta_AASlider and Selecta_AAProgress (I667f, design 2026-10-28-mui-custom-classes.md section 3.2). MUI 3.8.
 * AASlider: Numeric subclass (keeps MUIA_Numeric_Value/Min/Max, the key handling and the notifications of the stock slider). Draws
 * a rounded TRACK, the ACCENT filled part and a round knob (TEXT, HOVER_LIGHT on hover) through aa_round_fill; a click on the track
 * jumps, a drag moves the knob. AAProgress: Area subclass with UPA_Progress_Value and UPA_Progress_Max (the gauge). Amiga only; the sizes, colours
 * and value <-> position maths are in map/mui_aa.c. */
#ifndef MUI_AASLIDER_CLASS_H
#define MUI_AASLIDER_CLASS_H

#include <exec/types.h>
#include <libraries/mui.h>
#include <stdint.h>
#include <mcc/up.h>

#define UPA_Progress_Value UP_TAG(UP_CLS_PROGRESS, 0x01) /* LONG: filled part */
#define UPA_Progress_Max UP_TAG(UP_CLS_PROGRESS, 0x02)   /* LONG: value that fills the bar */

typedef struct mui_aaslider_spec {
    long lo, hi, value;
    long fix_w; /* fixed width in pixels; 0 = takes the free width of its row (I717: the volume slider is fixed, the seek slider flexes) */
    long id; /* MUIM_Application_ReturnID on every value change; 0 = no notification */
} mui_aaslider_spec;

Object* mui_aaslider_new(const mui_aaslider_spec* spec);
Object* mui_aaprogress_new(long value, long max);
/* Sliders and progress bars constructed since start (sentinels: the number of sites the window build made). */
uint32_t mui_aaslider_made(void);
uint32_t mui_aaprogress_made(void);
void mui_aaslider_class_free(void);

#endif
