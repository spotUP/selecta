/* dr_flac probe and static hook facts (probe.c), shared by adapter.c and the Amiga manifest unit (I427e4). */
#ifndef SEL_FLAC_PROBE_H
#define SEL_FLAC_PROBE_H

#include <selecta/plugin.h>

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* SEL_PROBE_YES for the "fLaC" stream marker (at least 8 bytes), else SEL_PROBE_NO. */
int flac_probe(const uint8_t* data, size_t size, const char* name);
void flac_set_tag(sel_tag* t, uint32_t tag, uintptr_t value);
/* Fills NAME, VERSION, KIND, EXTENSIONS and PROBE and returns how many tags it wrote. */
size_t flac_static_hooks(sel_tag* t);

#ifdef __cplusplus
}
#endif

#endif
