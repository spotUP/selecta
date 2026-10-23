/* Platform abstraction for the few calls the core needs from the OS outside
 * the plugin SDK (RULES.md rule 3: OS headers only under src/core/<os>/).
 * One implementation per platform: src/core/amiga/pal.c, src/core/macos/pal.c. */
#ifndef SELECTA_CORE_PAL_H
#define SELECTA_CORE_PAL_H

/* Sets the calling task's priority and returns the value read back. The
 * desktop has no task priorities and returns `pri` unchanged. */
int sel_pal_set_task_priority(int pri);

/* Brackets the publication of a queue head or tail (and nothing else): the
 * Amiga disables interrupts, the desktop does nothing. Never nest, never
 * block between begin and end. */
void sel_pal_publish_begin(void);
void sel_pal_publish_end(void);

#endif
