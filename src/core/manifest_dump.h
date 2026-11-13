/* Generated plugin manifest (I1193b-M2): the descriptor a module returns, written as C from the tag type table of
 * tags.def. Nobody edits the output; it is compiled into one manifest module (M3). */
#ifndef SELECTA_CORE_MANIFEST_DUMP_H
#define SELECTA_CORE_MANIFEST_DUMP_H

#include <selecta/tags.h>

#include <stdio.h>

SEL_EXTERN_C_BEGIN

/* Write C for `count` descriptors. modules[i] is the module file name stored as SEL_PLUGIN_MODULE (NULL: keep the
 * descriptor's own). INT and STR tags are copied, LIST tags recursively; PTR tags are dropped from the lists and
 * their ids collected under SEL_PLUGIN_HOOKS_PRESENT. The output defines `sel_manifest_entries[]`, a NULL ended
 * array of sel_plugin_entry_fn. Returns 0, or -1 on a tag missing from tags.def. */
int sel_manifest_dump_c(FILE* out, const sel_tag* const* descs, const char* const* modules, uint32_t count);

SEL_EXTERN_C_END

#endif
