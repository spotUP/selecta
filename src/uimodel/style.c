/* Cell style: see style.h. */
#include <selecta/uimodel/style.h>

txt_style txt_style_make(uint16_t fg, uint16_t bg, uint8_t attr) {
    txt_style s;
    s.fg = fg;
    s.bg = bg;
    s.attr = attr;
    return s;
}
