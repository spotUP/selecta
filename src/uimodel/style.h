/* Cell style shared by the text UI grid and the theme tokens (lifted out of plugins/ui/text/grid.h, DUI-P1-5).
 * Pure C99. */
#ifndef SELECTA_UIMODEL_STYLE_H
#define SELECTA_UIMODEL_STYLE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TXT_COLOR_DEFAULT 256u /* terminal default foreground or background */
#define TXT_ATTR_BOLD 1u
#define TXT_ATTR_REVERSE 2u

typedef struct txt_style {
    uint16_t fg; /* xterm-256color index or TXT_COLOR_DEFAULT */
    uint16_t bg;
    uint8_t attr; /* TXT_ATTR_* */
} txt_style;

txt_style txt_style_make(uint16_t fg, uint16_t bg, uint8_t attr);

#ifdef __cplusplus
}
#endif
#endif
