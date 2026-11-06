/* UP- MUI class package: the only place the MUI tag base is written (design 2026-11-06-mui-mcc-package.md section 8.2, decision D1).
 * Every attribute and method number of the package is UP_TAG(class index, offset). Interim base until a registered MUI serial
 * exists; the swap is this one line plus a major version bump. */
#ifndef MCC_UP_H
#define MCC_UP_H

#if defined(__amigaos__) || defined(AMIGA)
#include <exec/types.h>
#include <utility/tagitem.h>
#else
/* Host (tests, no Amiga headers): the two names the macros need, with the Amiga values. */
#include <stdint.h>
typedef uint32_t ULONG;
#define TAG_USER (1UL << 31)
#endif

#define UP_TAGBASE (TAG_USER | 0x53410000UL)
#define UP_TAG(cls, n) (UP_TAGBASE | ((ULONG)(cls) << 8) | (ULONG)(n))

/* Class index byte (design 8.2) */
#define UP_CLS_TEXT 0x00     /* UP-Text */
#define UP_CLS_PROGRESS 0x01 /* UP-Progress / slider value */
#define UP_CLS_CYCLE 0x02
#define UP_CLS_TABS 0x03
#define UP_CLS_SCROLLER 0x04
#define UP_CLS_LIST 0x05
#define UP_CLS_BUTTON 0x06
#define UP_CLS_FIELD 0x07
#define UP_CLS_PANEL 0x08
#define UP_CLS_BALANCE 0x09
#define UP_CLS_SHARED 0x0F /* tooltip and shared */

#endif
