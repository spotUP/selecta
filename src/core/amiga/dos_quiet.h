#ifndef SELECTA_AMIGA_DOS_QUIET_H
#define SELECTA_AMIGA_DOS_QUIET_H

/* Requester-free probing of AmigaDOS names (I214). Any Lock/Open/CreateDir on a name whose
 * assign or volume may not exist ("ENVARC:", "ENV:", a user path) makes dos.library open a
 * "Please insert volume X in any drive" requester on the Workbench screen; in a shell run
 * from a terminal program that window is invisible and the program waits for ever. Every such
 * call in src/core/amiga goes through sel_amiga_dos_quiet_begin/end, which set the process's
 * pr_WindowPtr to -1 (the documented "no requesters" value) and restore it. */
void* sel_amiga_dos_quiet_begin(void);
void sel_amiga_dos_quiet_end(void* saved);

/* 1 when `name` (e.g. "ENVARC:") can be locked, 0 when the assign or volume is missing. Never
 * opens a requester. */
int sel_amiga_name_exists(const char* name);

#endif
