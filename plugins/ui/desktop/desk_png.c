/* PNG writer for the desktop UI frame dump (I1013): the one translation unit holding the stb_image_write implementation,
 * kept out of desktop_ui.c. The warning set is relaxed for the vendored header only. */
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wcomment"
#pragma GCC diagnostic ignored "-Wcast-qual"
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wdouble-promotion"
#ifdef __clang__
#pragma GCC diagnostic ignored "-Wnewline-eof"
#endif
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_WRITE_NO_STDIO
#include "stb_image_write.h"

#include <stdio.h>

#include "desk_png.h"

static void desk_png_sink(void* ctx, void* data, int size) {
    if (ctx != NULL && size > 0) {
        (void)fwrite(data, 1, (size_t)size, (FILE*)ctx); /* a short write sets ferror, which desk_png_write reports */
    }
}

int desk_png_write(FILE* f, int w, int h, const unsigned char* rgb) {
    return stbi_write_png_to_func(desk_png_sink, f, w, h, 3, rgb, w * 3) != 0 && ferror(f) == 0;
}
