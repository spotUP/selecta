/* I625a3: see mui_fontprobe.h. */
#include "mui_fontprobe.h"

#include <stddef.h>

void* mui_fontprobe_open(const mui_fontprobe_ops* ops, const char* const* names) {
    void* saved = ops->quiet_begin(ops->ctx);
    void* font = NULL;
    size_t i;
    for (i = 0; names[i] != NULL && font == NULL; i++) {
        font = ops->open(ops->ctx, names[i]);
    }
    ops->quiet_end(ops->ctx, saved);
    return font;
}

void mui_fontprobe_reset(void) {}
