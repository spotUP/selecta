/* SunVox probe and static hook facts (probe.c), shared by adapter.c and the Amiga manifest unit (I427e4). */
#ifndef SEL_SV_PROBE_H
#define SEL_SV_PROBE_H

#include <selecta/plugin.h>

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Highest song version the 2009 engine writes and reads (it saves VERS 0x0103); newer SunVox versions store
 * 0x01xx0000-style values and use blocks and modules this engine does not know. */
#define SV_MAX_VERSION 0x0103u

/* SEL_PROBE_YES for "SVOX" + "VERS" at 8 with a version up to SV_MAX_VERSION, else SEL_PROBE_NO (a newer song is refused
 * here, never handed to the engine). The one header check: rp_open uses it too. */
int sv_probe(const uint8_t* data, size_t size, const char* name);
void sv_set_tag(sel_tag* t, uint32_t tag, uintptr_t value);
/* Fills NAME, VERSION, KIND, EXTENSIONS, PROBE, PRIORITY, NEEDS_FLOAT and returns how many tags it wrote. */
size_t sv_static_hooks(sel_tag* t);

#ifdef __cplusplus
}
#endif

#endif
