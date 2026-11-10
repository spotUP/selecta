/* bbmp ptk probe and static hook facts (probe.c), shared by adapter.c and the Amiga manifest unit (I427e). */
#ifndef SEL_BBPK_PROBE_H
#define SEL_BBPK_PROBE_H

#include <selecta/plugin.h>

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int ptks_probe(const uint8_t* d, size_t n);
void bbpk_set_tag(sel_tag* t, uint32_t tag, uintptr_t value);
/* Fills NAME, VERSION, KIND, EXTENSIONS, PROBE, PRIORITY, NEEDS_FLOAT and returns how many tags it wrote. */
size_t bbpk_static_hooks(sel_tag* t);

#ifdef __cplusplus
}
#endif

#endif
