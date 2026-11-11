/* nuade: which players the adapter drives and the FORMATS / EXTENSIONS lists they make (lists.c). Shared by the full
 * plugin (adapter.c) and the host tool nuade_manifest_lists, which writes manifest_lists.h for the Amiga manifest
 * (I427e4); the nuade-manifest-lists test fails when that committed header drifts from this walk. */
#ifndef SEL_NUADE_LISTS_H
#define SEL_NUADE_LISTS_H

#include <nuade/nuade.h>

#include <stddef.h>

#define SEL_NUADE_LIST_LEN 4096u

/* A player this adapter can drive: tick players only. Self-rendering players would need a RENDER-kind adapter. */
int sel_nuade_usable(const NuadePlayer* p);
/* Every usable player's id into formats and the union of their extensions into extensions, registry order, each
 * SEL_NUADE_LIST_LEN bytes. A list that would overflow returns 0 instead of truncating. */
int sel_nuade_build_lists(char* formats, char* extensions);

#endif
