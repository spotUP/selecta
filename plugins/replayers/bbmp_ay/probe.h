/* bbmp ay probe and static hook facts (probe.c), shared by adapter.c and the Amiga manifest unit (I1223c). */
#ifndef SEL_BBAY_PROBE_H
#define SEL_BBAY_PROBE_H

#include <selecta/plugin.h>

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { KIND_FLS = 0, KIND_FXM = 1, KIND_STRC = 2, KIND_BEEPOLA = 3, KIND_NONE = -1 };

int bbay_kind_of(const uint8_t* data, size_t size, const char* name);
void bbay_set_tag(sel_tag* t, uint32_t tag, uintptr_t value);
/* Fills NAME, VERSION, KIND, EXTENSIONS, PROBE, PRIORITY, MEMORY and returns how many tags it wrote. */
size_t bbay_static_hooks(sel_tag* t);

#ifdef __cplusplus
}
#endif

#endif
