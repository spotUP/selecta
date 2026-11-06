/*
 * m68kconf_host.h - Musashi options for musashi-host (MUSASHI_CNF)
 *
 * The vendored third-party/musashi/m68kconf.h guards every option with
 * #ifndef, so this sets the host's and includes it. No PMMU; TRAP #n reaches
 * the host first (UADE's score sends its messages with TRAP #5, which UAE
 * catches in newcpu.c Exception(37)).
 */
#ifndef M68KCONF_HOST_H
#define M68KCONF_HOST_H
#define M68K_EMULATE_PMMU        0
#define M68K_TRAP_HAS_CALLBACK   2   /* M68K_OPT_SPECIFY_HANDLER */
#define M68K_TRAP_CALLBACK(t)    ah_trap_callback(t)
int ah_trap_callback(int n);   /* amiga_host.c */
#include "m68kconf.h"
#endif
