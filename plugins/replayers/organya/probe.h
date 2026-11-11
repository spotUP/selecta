/* Organya probe and static hook facts (probe.c), shared by adapter.c and the Amiga manifest unit (I427e4). */
#ifndef SEL_ORG_PROBE_H
#define SEL_ORG_PROBE_H

#include <selecta/plugin.h>

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* SEL_PROBE_YES for "Org-01".."Org-03", SEL_PROBE_UNSURE for another "Org-" header on a .org name, else SEL_PROBE_NO. */
int org_probe(const uint8_t* data, size_t size, const char* name);
void org_set_tag(sel_tag* t, uint32_t tag, uintptr_t value);
/* Fills NAME, VERSION, KIND, EXTENSIONS, PROBE, PRIORITY, NEEDS_FLOAT and returns how many tags it wrote. */
size_t org_static_hooks(sel_tag* t);

#ifdef __cplusplus
}
#endif

#endif
