/* playsid probe and static hook facts (probe.c), shared by adapter.c and the Amiga manifest unit (I427e4). */
#ifndef SEL_PSID_PROBE_H
#define SEL_PSID_PROBE_H

#include <selecta/plugin.h>

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Probe calls (sentinel, tests/engine/playsid_test.c). */
extern uint32_t sel_playsid_probes;
/* 1 for a "PSID"/"RSID" header of at least 0x76 bytes. */
int psid_has_magic(const uint8_t* data, size_t size);
/* SEL_PROBE_YES when psid_has_magic and LIBS:playsid.library opens, else SEL_PROBE_NO (the next SID owner takes it). */
int psid_probe(const uint8_t* data, size_t size, const char* name);
void psid_set_tag(sel_tag* t, uint32_t tag, uintptr_t value);
/* Fills NAME, VERSION, KIND, EXTENSIONS, PROBE, MIN_CPU, PRIORITY and returns how many tags it wrote. */
size_t psid_static_hooks(sel_tag* t);

#ifdef __cplusplus
}
#endif

#endif
