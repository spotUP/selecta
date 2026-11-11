/* pxtone probe and static hook facts (probe.c), shared by adapter.c and the Amiga manifest unit (I427e4). */
#ifndef SEL_PX_PROBE_H
#define SEL_PX_PROBE_H

#include <selecta/plugin.h>

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* SEL_PROBE_YES for a "PTCOLLAGE-" (.ptcop) or "PTTUNE--" (.pttune) header in at least 16 bytes, else SEL_PROBE_NO. The one
 * header check: rp_open uses it too. */
int px_probe(const uint8_t* data, size_t size, const char* name);
void px_set_tag(sel_tag* t, uint32_t tag, uintptr_t value);
/* Fills NAME, VERSION, KIND, EXTENSIONS, PROBE, PRIORITY, NEEDS_FLOAT and returns how many tags it wrote. */
size_t px_static_hooks(sel_tag* t);

#ifdef __cplusplus
}
#endif

#endif
