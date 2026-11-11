/* Klystrack probe and static hook facts (probe.c), shared by adapter.c and the Amiga manifest unit (I1305m). */
#ifndef SEL_KT_PROBE_H
#define SEL_KT_PROBE_H

#include <selecta/plugin.h>

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KT_SIG "cyd!song"
#define KT_SIG_LEN 8u

/* SEL_PROBE_YES when the data starts with the "cyd!song" signature (and holds at least one byte more), else SEL_PROBE_NO. */
int kt_probe(const uint8_t* data, size_t size, const char* name);
void kt_set_tag(sel_tag* t, uint32_t tag, uintptr_t value);
/* Fills NAME, VERSION, KIND, EXTENSIONS, PROBE, PRIORITY, NEEDS_FLOAT and returns how many tags it wrote. */
size_t kt_static_hooks(sel_tag* t);

#ifdef __cplusplus
}
#endif

#endif
