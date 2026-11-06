/* Selecta glue for GoatTracker (row I710al), licence GPL-2.0-or-later (SPDX-License-Identifier: GPL-2.0-or-later). Written for Selecta; no bbmp code.
 * Stands in for GoatTracker 2.77's editor header goattrk2.h: the play routine (gplay.c), the song loader (gsong.c) and makespeedtable (gtable.c) are
 * compiled unchanged apart from the marked SELECTA EDIT lines (third_party/goattracker/VERSION.txt); this header supplies what they take from the editor. */
#ifndef GT_HEADLESS_H
#define GT_HEADLESS_H

#define GT_HEADLESS 1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "gcommon.h"
#include "gsid.h"
#include "gsong.h"
#include "gplay.h"
#include "gtable.h"
#include "ginstr.h"

#define MAX_FILENAME 256
#define VISIBLEPATTROWS 31

#ifdef __cplusplus
extern "C" {
#endif

/* Editor state the play routine and loader read; defaults are GoatTracker's own (goattrk2.c): multiplier 1, hard restart 0x0f00, fine vibrato on,
 * pulse and real-time optimisation on. Defined in gt_engine.cpp. */
extern int followplay, defaultpatternlength;
extern unsigned multiplier, adparam, finevibrato, optimizepulse, optimizerealtime;
extern int epnum[MAX_CHN], espos[MAX_CHN], esend[MAX_CHN];
extern int eppos, epview, epchn, epcolumn, epmarkchn, esnum, eschn, escolumn, eseditpos, esview, esmarkchn, enpos;
extern char songfilename[MAX_FILENAME], loadedsongfilename[MAX_FILENAME], instrfilename[MAX_FILENAME];
extern int instrused[MAX_INSTR];
extern int pattused[MAX_PATT];

FILE* gt_open_song(void);
int fread8(FILE* h);
void fwrite8(FILE* h, unsigned char v);
void songchange(void);
void resettime(void);
void incrementtime(void);
void sound_suspend(void);
void sound_flush(void);

/* The table editor's view of the speed table is not compiled; nothing to scroll. */
#define settableview(num, pos) ((void)0)

#ifdef __cplusplus
}
#endif

#endif
