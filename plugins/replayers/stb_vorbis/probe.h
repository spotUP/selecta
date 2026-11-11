/* stb_vorbis probe and static hook facts (probe.c), shared by adapter.c and the Amiga manifest unit (I427e4). */
#ifndef SEL_VORBIS_PROBE_H
#define SEL_VORBIS_PROBE_H

#include <selecta/plugin.h>

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* SEL_PROBE_YES for "OggS" with a Vorbis identification packet in the first page, else SEL_PROBE_NO. */
int vorbis_probe(const uint8_t* data, size_t size, const char* name);
void vorbis_set_tag(sel_tag* t, uint32_t tag, uintptr_t value);
/* Fills NAME, VERSION, KIND, EXTENSIONS, PROBE, MIN_CPU, NEEDS_FLOAT and STREAMING and returns how many tags it wrote. */
size_t vorbis_static_hooks(sel_tag* t);

#ifdef __cplusplus
}
#endif

#endif
