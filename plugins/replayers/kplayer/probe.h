/* KPlayer probe and static hook facts (probe.c), shared by adapter.c and the Amiga manifest unit (I427e4). */
#ifndef SEL_KP_PROBE_H
#define SEL_KP_PROBE_H

#include <selecta/plugin.h>

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 1 when the 31-sample module carries "M.K.", "M!K!" or "FLT4" at offset 1080 (the magic KP_INIT converts). */
int kp_has_magic(const uint8_t* data, size_t size);
/* SEL_PROBE_YES on kp_has_magic, else SEL_PROBE_NO. */
int kp_probe(const uint8_t* data, size_t size, const char* name);
void kp_set_tag(sel_tag* t, uint32_t tag, uintptr_t value);
/* Fills NAME, VERSION, KIND, EXTENSIONS, PROBE, MIN_CPU, PRIORITY and returns how many tags it wrote. */
size_t kp_static_hooks(sel_tag* t);

#ifdef __cplusplus
}
#endif

#endif
