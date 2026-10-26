/* Plugin metadata text to engine UTF-8 (I444). Strings inside the engine are UTF-8 (latin1.h); a replayer's
 * SEL_META_* STR values come straight from the song file, and Amiga module text (author, title, sample names) is
 * ISO-8859-1. The two places plugin metadata enters the host, the engine's song state and events and the library
 * scan, pass the replayer's list through sel_meta_utf8 so a Latin-1 "Andr\xE9" is stored and drawn as "André". */
#ifndef SELECTA_LIBRARY_META_UTF8_H
#define SELECTA_LIBRARY_META_UTF8_H

#include <selecta/tags.h>

#include <stddef.h>

/* Buffer sizes a caller of sel_meta_utf8 uses: replayers emit at most 14 tags today; MESSAGE can run to kilobytes. */
#define SEL_META_UTF8_TAGS 32u
#define SEL_META_UTF8_TEXT 8192u

/* Returns `in` itself when every STR value in it is valid UTF-8 (the common case: nothing is copied). Otherwise
 * copies the list, flattened (MORE and SKIP resolved), into `out` (capacity nout entries, END included) and gives
 * each STR value that is not valid UTF-8 a converted copy in `text` (capacity ntext), its bytes read as Latin-1.
 * A value that no longer fits in `text` is converted truncated, then empty. A list longer than `out` is returned
 * unchanged. The result stays valid until the next call with the same buffers or until `in` changes. */
const sel_tag* sel_meta_utf8(const sel_tag* in, sel_tag* out, size_t nout, char* text, size_t ntext);

#endif /* SELECTA_LIBRARY_META_UTF8_H */
