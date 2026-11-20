/* I1440q3: the real cover of a song for the MUI canvas. Same source as the desktop UI: the library blob "cover" of the song's album
 * (sel_ui_art; the DeepSID composer photo is stored there as the SID cover, design 2.3), decoded by the desktop stb_image path
 * (desk_image_decode, compiled here without stb_truetype) and cut to a square. */
#define SEL_STB_IMAGE_ONLY 1
#include "../desktop/stb_impl.c"

#include "mui_cover_image.h"

#include "map/mui_canvas.h"

#include <selecta/uimodel.h>

#include <stdio.h>

#define MUI_COVER_MAX_BLOB (512u * 1024u) /* shortcut: bigger covers keep the generated cover, a 68k decode of a large JPEG would stall the UI */

/* The decoded full cover (or still `frame`) of `album`, malloc'd words, NULL when it has none or it is too big. */
uint32_t* mui_cover_album_image(const sel_ui_art* art, int64_t album, uint32_t frame, int32_t* w, int32_t* h) {
    uint32_t len, kind = 0u;
    uint8_t* blob;
    uint32_t* full;
    char blob_name[8] = "cover";
    if (art == NULL || art->get == NULL || album == 0) {
        return NULL;
    }
    if (frame > 0u && art->kinds != NULL) { /* I1454: the n-th still of the album (dz<k>), cover for k = 0 */
        kind = sel_uim_art_frame_kind(art->kinds(art->ctx, album), frame);
        if (kind > 0u) {
            snprintf(blob_name, sizeof blob_name, "dz%u", (unsigned)kind);
        }
    }
    len = art->get(art->ctx, album, blob_name, NULL, 0);
    if (len == 0u || len > MUI_COVER_MAX_BLOB) {
        return NULL;
    }
    blob = (uint8_t*)malloc(len);
    if (blob == NULL) {
        return NULL;
    }
    if (art->get(art->ctx, album, blob_name, blob, len) != len) {
        free(blob);
        return NULL;
    }
    full = desk_image_decode(blob, len, w, h);
    free(blob);
    return full;
}

uint32_t* mui_cover_image(const sel_ui_art* art, const char* path, int32_t edge, uint32_t frame) {
    uint32_t* full;
    uint32_t* sq;
    int32_t w = 0, h = 0;
    if (art == NULL || art->find == NULL || path == NULL || edge <= 0) {
        return NULL;
    }
    full = mui_cover_album_image(art, art->find(art->ctx, path), frame, &w, &h);
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
