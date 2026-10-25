/* The stb_truetype (DUI-P2-3) and stb_image (DUI-P2-4) implementations, compiled once here with the project's warning set relaxed for the
 * vendored header only. */
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wcomment"
#define STB_TRUETYPE_IMPLEMENTATION
#ifdef __clang__
#pragma GCC diagnostic ignored "-Wnewline-eof"
#endif
#include "stb_truetype.h"

/* stb_image: PNG and JPEG from memory only (design doc 2.7). */
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_NO_STDIO
#define STBI_NO_LINEAR
#define STBI_NO_HDR
#define STB_IMAGE_IMPLEMENTATION
#pragma GCC diagnostic ignored "-Wcast-qual"
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#include "stb_image.h"

#include <stdlib.h>

#include "art.h"

uint32_t* desk_image_decode(const uint8_t* data, uint32_t len, int32_t* w, int32_t* h) {
    int iw = 0, ih = 0, n = 0;
    unsigned char* rgba = stbi_load_from_memory(data, (int)len, &iw, &ih, &n, 4);
    uint32_t* out;
    size_t i, count;
    if (rgba == NULL) {
        return NULL;
    }
    count = (size_t)iw * (size_t)ih;
    out = (uint32_t*)malloc(count * sizeof *out);
    if (out != NULL) {
        for (i = 0; i < count; i++) {
            const unsigned char* p = rgba + 4 * i;
            out[i] = ((uint32_t)p[3] << 24) | ((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | (uint32_t)p[2];
        }
        *w = iw;
        *h = ih;
    }
    stbi_image_free(rgba);
    return out;
}
