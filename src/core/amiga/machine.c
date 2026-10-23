/* Amiga machine probe: CPU and FPU from exec's AttnFlags, RAM from AvailMem totals, chipset from
 * graphics.library ChipRevBits0, graphics card from cybergraphics.library, AHI from ahi.device.
 * Network and disk probes arrive with the features that need them. The tier is text, planar
 * (ECS or AGA chipset, no card) or rtg (graphics card). D-M2-6: the planar tier is a probe value
 * only; no planar UI exists yet. */
#include "machine_internal.h"

#include <selecta/machine.h>

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

/* ahi.device takes a struct AHIRequest: an IOStdReq followed by ahir_Version (the AHI version the
 * caller was built for) and AHI's own fields, 80 bytes in all. The toolchain ships no AHI headers,
 * so the size and the field offset are written out here (devices/ahi.h, AHI 4). Opened with a
 * plain IOStdReq, AHI reads past the request, raises recoverable alert #00080000 (AG_BadParm) and
 * fails the open; measured on the Replay 68060 2026-10-23 (row I217). */
#define AHI_REQUEST_BYTES 80u
#define AHI_VERSION_OFFSET sizeof(struct IOStdReq)
#define AHI_VERSION 4u

/* Size of the last request ahi_request_create asked exec for. The sentinel reports it instead of
 * io_Message.mn_Length because vamos's CreateIORequest leaves mn_Length unset. */
static ULONG ahi_request_bytes;

static struct IORequest* ahi_request_create(struct MsgPort* port) {
    struct IORequest* io;

    ahi_request_bytes = AHI_REQUEST_BYTES;
    io = CreateIORequest(port, ahi_request_bytes);

    if (io) {
        *(UWORD*)(void*)((UBYTE*)io + AHI_VERSION_OFFSET) = AHI_VERSION;
    }
    return io;
}

void sel_machine_ahi_request(uint32_t* bytes, uint32_t* version) {
    struct MsgPort* port = CreateMsgPort();
    struct IORequest* io = port ? ahi_request_create(port) : NULL;

    *bytes = io ? ahi_request_bytes : 0u;
    *version = io ? *(UWORD*)(void*)((UBYTE*)io + AHI_VERSION_OFFSET) : 0u;
    if (io) {
        DeleteIORequest(io);
    }
    if (port) {
        DeleteMsgPort(port);
    }
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
        io = ahi_request_create(port);
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
