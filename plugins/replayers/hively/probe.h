/* Hively probe and static hook facts (probe.c), shared by adapter.c and the Amiga manifest unit (I427e4). */
#ifndef SEL_HVL_PROBE_H
#define SEL_HVL_PROBE_H

#include <selecta/plugin.h>

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HVL_MIN_SIZE 16u

/* SEL_PROBE_YES for "THX" + version 0..2 and "HVL" + version 0..1, else SEL_PROBE_NO. */
int hvl_probe(const uint8_t* data, size_t size, const char* name);
void hvl_set_tag(sel_tag* t, uint32_t tag, uintptr_t value);
/* Fills NAME, VERSION, KIND, EXTENSIONS, PROBE, PRIORITY, NEEDS_FLOAT and returns how many tags it wrote. */
size_t hvl_static_hooks(sel_tag* t);

#ifdef __cplusplus
}
#endif

#endif
