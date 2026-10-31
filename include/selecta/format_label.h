/* I819: the one label of a song format shown by every UI. Built on the Modland format table (src/library/formats_table.c). */
#ifndef SELECTA_FORMAT_LABEL_H
#define SELECTA_FORMAT_LABEL_H

#include <stddef.h>

/* Longest label the table can produce plus the terminator; size every label buffer with it. */
#define SEL_FORMAT_LABEL_MAX 40u

/* Writes the display label of `raw`, a format name ("Protracker") or a file extension ("mod", ".mod"), to out:
 * "<long name> (<extension>)", for example "Protracker (mod)" and "Fasttracker 2 (xm)". A name is looked up by name, anything else
 * by extension; when several table entries share an extension the first entry wins ("mod" is Protracker). The extension shown is
 * the table entry's own (lower case), the one entry per format; an entry without one shows the name alone. An unknown format, or a
 * label that already has the form, is copied unchanged (the function is idempotent). NULL and "" give "". Returns the length. */
size_t sel_format_label(const char* raw, char* out, size_t cap);

#endif /* SELECTA_FORMAT_LABEL_H */
