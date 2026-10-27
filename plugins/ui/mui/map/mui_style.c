#include "mui_map.h"

#include <selecta/engine.h>

uint32_t mui_style_mask(int screen_is_rtg) {
    return screen_is_rtg ? SEL_VIEW_STYLES_ALL : SEL_VIEW_STYLES_ALL & ~SEL_VIEW_STYLE_BIT(SEL_VIEW_STYLE_COVERFLOW);
}
