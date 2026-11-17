/* I1440q3: the real cover of a song for the MUI canvas. Same source as the desktop UI: the library blob "cover" of the song's album
 * (sel_ui_art; the DeepSID composer photo is stored there as the SID cover, design 2.3), decoded by the desktop stb_image path
 * (desk_image_decode, compiled here without stb_truetype) and cut to a square. */
#define SEL_STB_IMAGE_ONLY 1
#include "../desktop/stb_impl.c"

#include "mui_cover_image.h"

#include "map/mui_canvas.h"

#define MUI_COVER_MAX_BLOB (512u * 1024u) /* shortcut: bigger covers keep the generated cover, a 68k decode of a large JPEG would stall the UI */

uint32_t* mui_cover_image(const sel_ui_art* art, const char* path, int32_t edge) {
    int64_t album;
    uint32_t len;
    uint8_t* blob;
    uint32_t* full;
    uint32_t* sq;
    int32_t w = 0, h = 0;
    if (art == NULL || art->find == NULL || art->get == NULL || path == NULL || edge <= 0) {
        return NULL;
    }
    album = art->find(art->ctx, path);
    len = album != 0 ? art->get(art->ctx, album, "cover", NULL, 0) : 0u;
    if (len == 0u || len > MUI_COVER_MAX_BLOB) {
        return NULL;
    }
    blob = (uint8_t*)malloc(len);
    if (blob == NULL) {
        return NULL;
    }
    if (art->get(art->ctx, album, "cover", blob, len) != len) {
        free(blob);
        return NULL;
    }
    full = desk_image_decode(blob, len, &w, &h);
    free(blob);
    if (full == NULL) {
        return NULL;
    }
    sq = (uint32_t*)malloc((size_t)edge * (size_t)edge * sizeof *sq);
    if (sq != NULL && !mui_image_square(full, w, h, sq, edge)) {
        free(sq);
        sq = NULL;
    }
    free(full);
    return sq;
}
