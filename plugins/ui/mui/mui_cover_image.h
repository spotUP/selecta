#ifndef MUI_COVER_IMAGE_H
#define MUI_COVER_IMAGE_H

#include <selecta/ui.h>

#include <stdint.h>

/* Malloc'd edge x edge 0xAARRGGBB words of the library cover of the song at `path` (square centre crop), NULL when it has none or it is too big. */
uint32_t* mui_cover_image(const sel_ui_art* art, const char* path, int32_t edge);

#endif
