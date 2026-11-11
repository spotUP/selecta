/* minimp3 probe and static hook facts (probe.c), shared by adapter.c and the Amiga manifest unit (I427e4). */
#ifndef SEL_MP3_PROBE_H
#define SEL_MP3_PROBE_H

#include <selecta/plugin.h>

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ID3v2 header: "ID3", version, flags, 4 syncsafe size bytes; returns the tag length or 0. */
size_t mp3_id3v2_len(const uint8_t* d, size_t n);
/* SEL_PROBE_YES for two consecutive valid MPEG audio headers within the first 4 KB after the ID3v2 tag. */
int mp3_probe(const uint8_t* data, size_t size, const char* name);
void mp3_set_tag(sel_tag* t, uint32_t tag, uintptr_t value);
/* Fills NAME, VERSION, KIND, EXTENSIONS, PROBE, MIN_CPU, NEEDS_FLOAT and STREAMING and returns how many tags it wrote. */
size_t mp3_static_hooks(sel_tag* t);

#ifdef __cplusplus
}
#endif

#endif
