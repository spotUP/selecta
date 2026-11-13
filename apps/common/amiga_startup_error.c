/* Amiga part of the fatal start-up report (I563, I1352): always an intuition.library EasyRequest, console or not, so a
 * failure is never silent (Johan 2026-11-13). Text states the cause and the fix.
 * EasyRequestArgs exists since OS 2.0; the library is opened at version 36 or later only when needed, so OS 3.0
 * is covered. */
#include "startup_error.h"

/* The library base is renamed for the reason given in backend_rtg.c: libnix would open it at program start, and a
 * shared global set to NULL here would break any later user of it. */
#define IntuitionBase sel_startup_IntuitionBase

#include <dos/dos.h>
#include <intuition/intuition.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/intuition.h>

struct IntuitionBase* IntuitionBase;

void sel_amiga_startup_requester(const char* text) {
    struct EasyStruct es;
    struct IntuitionBase* ib;

    ib = (struct IntuitionBase*)OpenLibrary((CONST_STRPTR) "intuition.library", 36);
    if (ib == NULL) {
        return;
    }
    IntuitionBase = ib;
    es.es_StructSize = sizeof es;
    es.es_Flags = 0;
    es.es_Title = (UBYTE*)"Selecta";
    es.es_TextFormat = (UBYTE*)"%s";
    es.es_GadgetFormat = (UBYTE*)"Ok";
    EasyRequest(NULL, &es, NULL, (APTR)text);
    CloseLibrary((struct Library*)ib);
    IntuitionBase = NULL;
}
