/* UTF-8 path to AmigaDOS Latin-1 (I218d). Every path that enters dos.library from the OS layer goes through
 * sel_dos_path; names that come back are converted with sel_latin1_to_utf8 (src/library/latin1.h). */
#ifndef SELECTA_CORE_AMIGA_DOS_NAME_H
#define SELECTA_CORE_AMIGA_DOS_NAME_H

#include "../../library/latin1.h"

#include <stddef.h>

/* Room for a converted path; a DOS name is at most 107 bytes, a path of nested names stays well below this. */
#define SEL_DOS_PATH_MAX 512

/* Fills dst with the Latin-1 form of the UTF-8 `utf8` path. 0 when it has no Latin-1 spelling or does not fit:
 * the caller fails the call, because no such file can exist. */
static inline int sel_dos_path(char* dst, size_t n, const char* utf8) {
    return sel_utf8_to_latin1(dst, n, utf8);
}

#endif /* SELECTA_CORE_AMIGA_DOS_NAME_H */
