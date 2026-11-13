/* Tag text and attribute lookup in a bounded XML range (podcast.c, plugins/shared/upnp). */
#ifndef SELECTA_SHARED_SOURCE_XML_SCAN_H
#define SELECTA_SHARED_SOURCE_XML_SCAN_H

#include <stddef.h>

/* Copies the text between `<tag ...>` and `</tag>` (first match in [s, end)) to out, decoding CDATA and the five XML
 * entities, trailing space trimmed. 1 when non-empty text was found. */
int sel_xml_tag_text(const char* s, const char* end, const char* tag, char* out, size_t n);

/* Decodes the entity whose '&' is at p (inside [p, end)): decimal and hex numeric references, amp lt gt quot apos nbsp
 * hellip ndash mdash rsquo lsquo ldquo rdquo and a few more, and the double-escaped "&amp;#039;". Writes UTF-8 to out
 * (cap bytes) and returns its length, *used = input bytes consumed. 0 when p is not a known entity or does not fit. */
size_t sel_xml_entity(const char* p, const char* end, char* out, size_t cap, size_t* used);

/* The value of attribute `attr` of the first `<tag ...>` (a space must follow the tag name) in [s, end). */
int sel_xml_tag_attr(const char* s, const char* end, const char* tag, const char* attr, char* out, size_t n);

#endif
