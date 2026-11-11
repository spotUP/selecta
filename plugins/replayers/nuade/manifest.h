/* nuade: the static hook facts (manifest.c), shared by the full plugin (adapter.c) and the Amiga manifest unit the
 * registry links (I427e4, thoughts/shared/plans/2026-11-10-amiga-loadseg-plugins.md). */
#ifndef SEL_NUADE_MANIFEST_H
#define SEL_NUADE_MANIFEST_H

#include <selecta/plugin.h>

#include <stddef.h>
#include <stdint.h>

/* SEL_RP_CONFIG option (I134): output gain percent 0 to 100, answered by the host from the rp.config pref or the
 * per-song override. Unanswered: 100, the song's own volume. */
#define SEL_NUADE_CFG_GAIN "nuade.gain"

void sel_nuade_set_tag(sel_tag* t, uint32_t tag, uintptr_t value);
/* Fills NAME, VERSION, KIND, EXTENSIONS, FORMATS, MAX_VOICES, MIN_CPU, NEEDS_FLOAT, MEMORY, CONFIG, COMPANIONS and
 * CONTENT_COMPANIONS and returns how many tags it wrote. No PROBE: the probe asks every player, so it lives with them
 * in the module; the engine resolves a probeless manifest when its EXTENSIONS claim the song's name. */
size_t sel_nuade_static_hooks(sel_tag* t, const char* extensions, const char* formats);

#endif
