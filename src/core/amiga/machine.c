/* Amiga machine probe: CPU and FPU from exec's AttnFlags, RAM from AvailMem totals, chipset from
 * graphics.library ChipRevBits0, graphics card from cybergraphics.library, AHI from ahi.device.
 * Network and disk probes arrive with the features that need them. The tier is text, planar
 * (ECS or AGA chipset, no card) or rtg (graphics card). D-M2-6: the planar tier is a probe value
 * only; no planar UI exists yet. */
#include "machine_internal.h"

#include <exec/execbase.h>
#include <exec/io.h>
#include <exec/memory.h>
#include <exec/ports.h>
#include <graphics/gfxbase.h>
#include <proto/dos.h>
#include <proto/exec.h>

#include <string.h>

#define AHI_DEVICE_NAME "ahi.device"

static const char* probe_chipset(struct Library* gfx) {
    UBYTE rev = ((struct GfxBase*)gfx)->ChipRevBits0;

    if (rev & (GFXF_AA_ALICE | GFXF_AA_LISA)) {
        return "AGA";
    }
    if (rev & (GFXF_HR_AGNUS | GFXF_HR_DENISE)) {
        return "ECS";
    }
    return "OCS";
}

/* ahi.device is a disk device: its file in DEVS: says it is installed. Opening an absent device
 * under vamos never returns, so look at the file first; the open then proves it loads. */
static int ahi_present(void) {
    struct MsgPort* port;
    struct IORequest* io = NULL;
    int found = 0;
    BPTR lock = Lock((CONST_STRPTR) "DEVS:" AHI_DEVICE_NAME, ACCESS_READ);

    if (!lock) {
        return 0;
    }
    UnLock(lock);
    port = CreateMsgPort();

    if (port) {
        io = CreateIORequest(port, sizeof(struct IOStdReq));
    }
    if (io && OpenDevice((CONST_STRPTR)AHI_DEVICE_NAME, 255, io, 0) == 0) {
        CloseDevice(io);
        found = 1;
    }
    if (io) {
        DeleteIORequest(io);
    }
    if (port) {
        DeleteMsgPort(port);
    }
    return found;
}

void sel_machine_probe_platform(sel_machine_raw* raw) {
    static char gfx_text[16];
    static char audio_text[16];
    UWORD flags = SysBase->AttnFlags;
    const char* chipset = "OCS";
    struct Library* gfx;
    struct Library* cgx;
    int rtg = 0;

    if (flags & AFF_68060) {
        raw->cpu = SEL_CPU_68060;
    } else if (flags & AFF_68040) {
        raw->cpu = SEL_CPU_68040;
    } else if (flags & AFF_68030) {
        raw->cpu = SEL_CPU_68030;
    } else if (flags & AFF_68020) {
        raw->cpu = SEL_CPU_68020;
    } else if (flags & AFF_68010) {
        raw->cpu = SEL_CPU_68010;
    } else {
        raw->cpu = SEL_CPU_68000;
    }
    raw->fpu = (flags & (AFF_68881 | AFF_68882 | AFF_FPU40)) ? 1u : 0u;
    raw->chip_ram = (uintptr_t)AvailMem(MEMF_CHIP | MEMF_TOTAL);
    raw->fast_ram = (uintptr_t)AvailMem(MEMF_FAST | MEMF_TOTAL);

    gfx = OpenLibrary((CONST_STRPTR) "graphics.library", 36);
    if (gfx) {
        chipset = probe_chipset(gfx);
        CloseLibrary(gfx);
    }
    cgx = OpenLibrary((CONST_STRPTR) "cybergraphics.library", 0);
    if (cgx) {
        rtg = 1;
        CloseLibrary(cgx);
    }
    gfx_text[0] = '\0';
    strcat(gfx_text, chipset);
    if (rtg) {
        strcat(gfx_text, "+RTG");
    }
    raw->gfx = gfx_text;

    strcpy(audio_text, "paula");
    if (ahi_present()) {
        strcat(audio_text, ",ahi");
    }
    raw->audio = audio_text;

    if (rtg) {
        raw->tier = SEL_TIER_RTG;
    } else if (strcmp(chipset, "OCS") != 0) {
        raw->tier = SEL_TIER_PLANAR;
    } else {
        raw->tier = SEL_TIER_TEXT;
    }
}
