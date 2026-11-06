/* I625a3: the font file probe, pure C99 so the host tests reach it. A probe of a name whose assign or volume may be missing
 * (`Selecta:Fonts/...`) must never raise a "Please insert volume" requester (invariant I214, src/core/amiga/dos_quiet.h): the
 * whole loop runs between one quiet begin and one quiet end. The package may not include src/core, so the caller passes its own
 * begin/end (mui_aa_draw.c). */
#ifndef SELECTA_MUI_FONTPROBE_H
#define SELECTA_MUI_FONTPROBE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mui_fontprobe_ops {
    void* ctx;
    void* (*quiet_begin)(void* ctx); /* returns the token quiet_end gets back */
    void (*quiet_end)(void* ctx, void* saved);
    void* (*open)(void* ctx, const char* name); /* font handle, NULL when this name does not open */
} mui_fontprobe_ops;

/* Tries `names` (NULL terminated) in order inside one quiet begin/end pair; the first name that opens stops the loop. Returns
 * that font handle, or NULL when every name failed. quiet_end runs exactly once either way. */
void* mui_fontprobe_open(const mui_fontprobe_ops* ops, const char* const* names);

#ifdef __cplusplus
}
#endif

#endif
