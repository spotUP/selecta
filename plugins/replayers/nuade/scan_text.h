/* nuade scan text (I1314c): title and author read straight from a song header for the Amiga manifest's SEL_RP_SCAN_META,
 * so a signature-claimed song's scan row is complete without loading the player module. */
#ifndef SEL_NUADE_SCAN_TEXT_H
#define SEL_NUADE_SCAN_TEXT_H

#include <stddef.h>
#include <stdint.h>

/* id is the player id sel_nuade_sig_match returned for data. Writes the header's title and author as UTF-8 (Latin-1
 * source bytes, trailing blanks cut) to title/author, each NUL-terminated and truncated to its buffer; an empty
 * string when the format has no such field or the header is short. Returns 1 when the player has a parser. */
int sel_nuade_scan_text(const char* id, const uint8_t* data, size_t size, char* title, size_t title_n, char* author,
                        size_t author_n);

#endif
