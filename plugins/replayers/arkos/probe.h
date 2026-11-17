/* Arkos Tracker 3 probe and static hook facts (probe.c), shared by adapter.c and the Amiga manifest unit (I433b). */
#ifndef SEL_ARKOS_PROBE_H
#define SEL_ARKOS_PROBE_H

#include <selecta/plugin.h>

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* SEL_PROBE_MAYBE for a zip or an XML song named *.aks, else SEL_PROBE_NO; the importer decides. */
int arkos_probe(const uint8_t* data, size_t size, const char* name);
void arkos_set_tag(sel_tag* t, uint32_t tag, uintptr_t value);
/* Fills NAME, VERSION, KIND, EXTENSIONS, PROBE, MEMORY and NEEDS_FLOAT and returns how many tags it wrote. */
size_t arkos_static_hooks(sel_tag* t);

#ifdef __cplusplus
}
#endif

#endif
