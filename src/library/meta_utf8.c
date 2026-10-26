/* Plugin metadata text to engine UTF-8, declared in meta_utf8.h. */
#include "meta_utf8.h"

#include "latin1.h"

#include <string.h>

static int is_bad_str(const sel_tag* t) {
    sel_tag_type type;

    return t->value != 0u && sel_tag_type_of(t->tag, &type) && type == SEL_TT_STR &&
           !sel_utf8_valid((const char*)t->value);
}

const sel_tag* sel_meta_utf8(const sel_tag* in, sel_tag* out, size_t nout, char* text, size_t ntext) {
    const sel_tag* it = in;
    const sel_tag* t;
    size_t n = 0, used = 0;
    int bad = 0;

    while ((t = sel_tag_next(&it)) != NULL) {
        n++;
        bad |= is_bad_str(t);
    }
    if (!bad || n + 1u > nout) {
        return in;
    }
    n = 0;
    it = in;
    while ((t = sel_tag_next(&it)) != NULL) {
        out[n] = *t;
        if (is_bad_str(t)) {
            const char* s = (const char*)t->value;
            char* d = text + used;
            size_t room = used < ntext ? ntext - used : 0u;

            if (room == 0u) {
                out[n].value = (uintptr_t)"";
            } else {
                used += sel_text_to_utf8(d, room, s) + 1u;
                out[n].value = (uintptr_t)d;
            }
        }
        n++;
    }
    out[n].tag = SEL_TAG_END;
    out[n].value = 0;
    return out;
}
