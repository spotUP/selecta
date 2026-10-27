/* The `aga` backend of the desktop UI (I439, M4-P6-1); see backend_aga.h and
 * thoughts/shared/plans/2026-10-26-aga-c2p-backend.md. */
#include "backend_aga.h"
#include "input.h"

#include "aga_planar.h"

/* Renamed library bases, as in backend_rtg.c: libnix opens a library at program start when its base symbol is
 * referenced. Own names also keep both backends linkable into one binary. */
#define IntuitionBase sel_aga_IntuitionBase
#define GfxBase sel_aga_GfxBase

#include <devices/timer.h>
#include <dos/dos.h>
#include <exec/execbase.h>
#include <exec/memory.h>
#include <graphics/displayinfo.h>
#include <graphics/gfx.h>
#include <graphics/modeid.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <stdio.h>
#include <string.h>
#include <utility/tagitem.h>

#include "../../../../third_party/kalms-c2p/normal/c2p1x1_8_c5_040.h"

struct IntuitionBase* IntuitionBase;
struct GfxBase* GfxBase;

#define AGA_GAP 8 /* rows: two bands this close are converted as one */
#define AGA_NO_MODE ((ULONG)INVALID_ID)

struct desk_aga {
    int32_t w, h;
    struct Screen* screen;
    struct Window* window;
    struct BitMap bm[2];
    UBYTE* buf[2];             /* contiguous 8 planes of w * h / 8 bytes each, chip RAM */
    struct ScreenBuffer* sb[2];
    struct MsgPort* safe_port; /* dbi_SafeMessage: the old buffer is no longer shown */
    int safe;                  /* 1 when the back buffer may be written */
    int back;                  /* index of the buffer not on display */
    int fresh;                 /* presents left that convert the whole canvas (2 buffers) */
    desk_aga_band prev[DESK_AGA_BANDS_MAX];
    uint32_t prev_n;
    uint32_t palette[256];
    uint32_t colours;
    uint32_t rgb32[256 * 3 + 2];
    struct MsgPort* tport;
    struct timerequest* treq;
    uint32_t upload_pixels, presents, c2p_calls, c2p_kind;
    desk_amiga_input in;
};

static void c2p_bands(desk_aga* b, const uint8_t* chunky, const desk_aga_band* band, uint32_t n) {
    uint32_t i, bplsize = (uint32_t)(b->w / 8) * (uint32_t)b->h;
    for (i = 0; i < n; i++) {
        if (b->c2p_kind == DESK_AGA_C2P_KALMS_040) {
            /* Kalms: rows y .. y + h of the chunky buffer into the same rows of the planes (scroffsy = y). */
            c2p1x1_8_c5_040_init(b->w, band[i].h, band[i].y, (int)bplsize);
            c2p1x1_8_c5_040((void*)(chunky + (size_t)band[i].y * (size_t)b->w), b->buf[b->back]);
        } else {
            desk_aga_c2p_ref(chunky, b->w, band[i].y, band[i].h, b->buf[b->back], bplsize);
        }
        b->c2p_calls++;
        b->upload_pixels += (uint32_t)band[i].h * (uint32_t)b->w;
    }
}

uint32_t desk_aga_ticks(void) {
    struct DateStamp ds;
    DateStamp(&ds);
    return (uint32_t)ds.ds_Days * 86400000u + (uint32_t)ds.ds_Minute * 60000u + (uint32_t)ds.ds_Tick * 20u;
}

/* The mode of the screen: Workbench's when it is a native mode with 8 bitplanes, else PAL or NTSC HIRES (both allow
 * 8 bitplanes on AGA). */
static ULONG pick_mode(int32_t* w, int32_t* h) {
    struct Screen* wb = LockPubScreen((CONST_STRPTR) "Workbench");
    ULONG mode = AGA_NO_MODE;
    if (wb != NULL) {
        struct DimensionInfo dim;
        ULONG id = GetVPModeID(&wb->ViewPort);
        if (id != AGA_NO_MODE && (id & MONITOR_ID_MASK) != 0x40000000u &&
            GetDisplayInfoData(NULL, (UBYTE*)&dim, sizeof dim, DTAG_DIMS, id) > 0 && dim.MaxDepth >= 8) {
            mode = id;
            if (*w <= 0 || *h <= 0) {
                *w = wb->Width;
                *h = wb->Height;
            }
        }
        UnlockPubScreen(NULL, wb);
    }
    if (mode == AGA_NO_MODE) {
        int ntsc = (GfxBase->DisplayFlags & NTSC) != 0;
        mode = (ntsc ? NTSC_MONITOR_ID : PAL_MONITOR_ID) | HIRES_KEY;
        if (*w <= 0 || *h <= 0) {
            *w = 640;
            *h = ntsc ? 200 : 256;
        }
    }
    *w &= ~31;
    return ModeNotAvailable(mode) == 0 ? mode : AGA_NO_MODE;
}

void desk_aga_default_size(int32_t* w, int32_t* h) {
    *w = 0;
    *h = 0;
    GfxBase = (struct GfxBase*)OpenLibrary((CONST_STRPTR) "graphics.library", 39);
    IntuitionBase = (struct IntuitionBase*)OpenLibrary((CONST_STRPTR) "intuition.library", 39);
    if (GfxBase != NULL && IntuitionBase != NULL) {
        (void)pick_mode(w, h);
    } else {
        *w = 640;
        *h = 256;
    }
    if (IntuitionBase != NULL) CloseLibrary((struct Library*)IntuitionBase);
    if (GfxBase != NULL) CloseLibrary((struct Library*)GfxBase);
    IntuitionBase = NULL;
    GfxBase = NULL;
}

static int fail(desk_aga* b, const char* why) {
    fprintf(stderr, "aga: %s\n", why); /* stderr: --log redirects it, stdout would hide the reason (I522) */
    fflush(stderr);
    desk_aga_close(b);
    return 0;
}

desk_aga* desk_aga_open(int32_t w, int32_t h, int index8) {
    desk_aga* b;
    ULONG mode;
    size_t bytes;
    int i, p;
    if (!index8) {
        fprintf(stderr, "aga: the AGA screen shows an INDEX8 canvas only; use --format index8 or leave --format out\n");
        fflush(stderr);
        return NULL;
    }
    b = (desk_aga*)AllocVec(sizeof *b, MEMF_CLEAR);
    if (b == NULL) return NULL;
    b->safe = 1; /* no ChangeScreenBuffer is pending: close must not wait for a safe message that never comes */
    GfxBase = (struct GfxBase*)OpenLibrary((CONST_STRPTR) "graphics.library", 39);
    IntuitionBase = (struct IntuitionBase*)OpenLibrary((CONST_STRPTR) "intuition.library", 39);
    if (GfxBase == NULL || IntuitionBase == NULL) return fail(b, "needs graphics and intuition v39"), NULL;
    if (!desk_amiga_input_open(&b->in)) return fail(b, "needs keymap.library v37"), NULL;
    if ((GfxBase->ChipRevBits0 & GFXF_AA_ALICE) == 0) return fail(b, "no AGA chipset"), NULL;
    mode = pick_mode(&w, &h);
    if (mode == AGA_NO_MODE || w < 32 || h < 1) return fail(b, "no 8-bitplane native mode"), NULL;
    b->w = w;
    b->h = h;
    bytes = desk_aga_buffer_bytes(w, h);
    if (AvailMem(MEMF_CHIP | MEMF_LARGEST) < desk_aga_chip_bytes(w, h) / 2u ||
        AvailMem(MEMF_CHIP) < desk_aga_chip_bytes(w, h)) {
        return fail(b, "not enough chip RAM for two 8-bitplane buffers"), NULL;
    }
    for (i = 0; i < 2; i++) {
        b->buf[i] = (UBYTE*)AllocMem((ULONG)bytes, MEMF_CHIP | MEMF_CLEAR);
        if (b->buf[i] == NULL) return fail(b, "chip RAM allocation failed"), NULL;
        InitBitMap(&b->bm[i], 8, (LONG)w, (LONG)h);
        for (p = 0; p < 8; p++) b->bm[i].Planes[p] = (PLANEPTR)(b->buf[i] + (size_t)p * (bytes / 8u));
    }
    {
        struct TagItem st[] = {{SA_Width, (ULONG)w}, {SA_Height, (ULONG)h}, {SA_Depth, 8},
                               {SA_DisplayID, mode}, {SA_BitMap, (ULONG)&b->bm[0]}, {SA_Quiet, TRUE},
                               {SA_ShowTitle, FALSE}, {SA_Type, CUSTOMSCREEN}, {SA_Exclusive, TRUE},
                               {SA_Title, (ULONG) "Selecta"}, {TAG_DONE, 0}};
        b->screen = OpenScreenTagList(NULL, st);
    }
    if (b->screen == NULL) return fail(b, "OpenScreen failed"), NULL;
    {
        struct TagItem wt[] = {{WA_CustomScreen, (ULONG)b->screen}, {WA_Left, 0}, {WA_Top, 0},
                               {WA_Width, (ULONG)w}, {WA_Height, (ULONG)h}, {WA_Backdrop, TRUE},
                               {WA_Borderless, TRUE}, {WA_Activate, TRUE}, {WA_RMBTrap, TRUE},
                               {WA_IDCMP, IDCMP_RAWKEY | IDCMP_MOUSEBUTTONS | IDCMP_MOUSEMOVE},
                               {WA_ReportMouse, TRUE}, {TAG_DONE, 0}};
        b->window = OpenWindowTagList(NULL, wt);
    }
    if (b->window == NULL) return fail(b, "OpenWindow failed"), NULL;
    /* A screen opened while another screen (Directory Opus) is in front stays behind it (I524a): bring it up. */
    ScreenToFront(b->screen);
    ActivateWindow(b->window);
    b->sb[0] = AllocScreenBuffer(b->screen, &b->bm[0], 0); /* SB_SCREEN_BITMAP fails on a screen opened with SA_BitMap */
    b->sb[1] = AllocScreenBuffer(b->screen, &b->bm[1], 0);
    b->safe_port = CreateMsgPort();
    if (b->sb[0] == NULL || b->sb[1] == NULL || b->safe_port == NULL) return fail(b, "screen buffers failed"), NULL;
    for (i = 0; i < 2; i++) b->sb[i]->sb_DBufInfo->dbi_SafeMessage.mn_ReplyPort = b->safe_port;
    b->tport = CreateMsgPort();
    b->treq = b->tport ? (struct timerequest*)CreateIORequest(b->tport, sizeof *b->treq) : NULL;
    if (b->treq == NULL || OpenDevice((CONST_STRPTR)TIMERNAME, UNIT_VBLANK, (struct IORequest*)b->treq, 0) != 0) {
        if (b->treq != NULL) DeleteIORequest((struct IORequest*)b->treq);
        b->treq = NULL;
        return fail(b, "timer.device failed"), NULL;
    }
    b->safe = 1;
    b->back = 1;
    b->fresh = 2;
    b->c2p_kind = (SysBase->AttnFlags & (AFF_68040 | AFF_68060)) ? DESK_AGA_C2P_KALMS_040 : DESK_AGA_C2P_REF;
    return b;
}

void desk_aga_close(desk_aga* b) {
    int i;
    if (b == NULL) return;
    if (b->treq != NULL) {
        CloseDevice((struct IORequest*)b->treq);
        DeleteIORequest((struct IORequest*)b->treq);
    }
    if (b->tport != NULL) DeleteMsgPort(b->tport);
    if (b->screen != NULL && !b->safe && b->safe_port != NULL) {
        while (GetMsg(b->safe_port) == NULL) WaitPort(b->safe_port);
    }
    if (b->window != NULL) CloseWindow(b->window);
    for (i = 1; i >= 0; i--) {
        if (b->sb[i] != NULL) FreeScreenBuffer(b->screen, b->sb[i]);
    }
    if (b->safe_port != NULL) DeleteMsgPort(b->safe_port);
    if (b->screen != NULL) CloseScreen(b->screen);
    for (i = 0; i < 2; i++) {
        if (b->buf[i] != NULL) FreeMem(b->buf[i], (ULONG)desk_aga_buffer_bytes(b->w, b->h));
    }
    desk_amiga_input_close(&b->in);
    if (IntuitionBase != NULL) CloseLibrary((struct Library*)IntuitionBase);
    if (GfxBase != NULL) CloseLibrary((struct Library*)GfxBase);
    IntuitionBase = NULL;
    GfxBase = NULL;
    FreeVec(b);
}

int32_t desk_aga_width(const desk_aga* b) { return b->w; }
int32_t desk_aga_height(const desk_aga* b) { return b->h; }
int32_t desk_aga_depth(const desk_aga* b) { return b->screen ? (int32_t)b->screen->RastPort.BitMap->Depth : 0; }
uint32_t desk_aga_upload_pixels(const desk_aga* b) { return b->upload_pixels; }
uint32_t desk_aga_presents(const desk_aga* b) { return b->presents; }
uint32_t desk_aga_c2p_calls(const desk_aga* b) { return b->c2p_calls; }
uint32_t desk_aga_c2p_kind(const desk_aga* b) { return b->c2p_kind; }
size_t desk_aga_chip_held(const desk_aga* b) { return b->buf[1] ? desk_aga_chip_bytes(b->w, b->h) : 0u; }

void desk_aga_present(desk_aga* b, const sel_gui_canvas* c) {
    desk_aga_band now[DESK_AGA_BANDS_MAX], todo[DESK_AGA_BANDS_MAX];
    uint32_t n, nt;
    if (c->format != SEL_GUI_INDEX8 || c->w != b->w || c->h != b->h) return;
    if (c->colours != b->colours || memcmp(c->palette, b->palette, sizeof b->palette) != 0) {
        memcpy(b->palette, c->palette, sizeof b->palette);
        b->colours = c->colours;
        desk_aga_rgb32_table(b->palette, 0, 256, b->rgb32);
        LoadRGB32(&b->screen->ViewPort, (ULONG*)b->rgb32);
    }
    if (b->fresh > 0) {
        now[0].y = 0;
        now[0].h = b->h;
        n = 1;
    } else {
        n = desk_aga_bands(c->dirty, c->dirty_n, b->h, AGA_GAP, now, DESK_AGA_BANDS_MAX);
    }
    /* The back buffer was last written two presents ago: it lacks the previous present's rows too. */
    memcpy(todo, now, n * sizeof now[0]);
    nt = desk_aga_bands_union(todo, n, b->prev, b->prev_n, AGA_GAP, DESK_AGA_BANDS_MAX);
    memcpy(b->prev, now, n * sizeof now[0]);
    b->prev_n = n;
    if (nt == 0u) return;
    if (!b->safe) {
        while (GetMsg(b->safe_port) == NULL) WaitPort(b->safe_port);
        b->safe = 1;
    }
    c2p_bands(b, (const uint8_t*)c->pixels, todo, nt);
    while (!ChangeScreenBuffer(b->screen, b->sb[b->back])) WaitTOF();
    b->safe = 0;
    b->back ^= 1;
    if (b->fresh > 0) b->fresh--;
    b->presents++;
}

int desk_aga_poll(desk_aga* b, uint32_t timeout_ms) {
    struct MsgPort* wp = b->window->UserPort;
    int result = DESK_AGA_NONE;
    struct IntuiMessage* m;
    if (timeout_ms > 0u && wp->mp_MsgList.lh_Head->ln_Succ == NULL) {
        ULONG got;
        b->treq->tr_node.io_Command = TR_ADDREQUEST;
        b->treq->tr_time.tv_secs = timeout_ms / 1000u;
        b->treq->tr_time.tv_micro = (timeout_ms % 1000u) * 1000u;
        SendIO((struct IORequest*)b->treq);
        got = Wait((1UL << wp->mp_SigBit) | (1UL << b->tport->mp_SigBit) | SIGBREAKF_CTRL_C);
        if (!CheckIO((struct IORequest*)b->treq)) AbortIO((struct IORequest*)b->treq);
        WaitIO((struct IORequest*)b->treq);
        if (got & SIGBREAKF_CTRL_C) return DESK_AGA_QUIT;
    } else if (SetSignal(0, SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C) {
        return DESK_AGA_QUIT;
    }
    while ((m = (struct IntuiMessage*)GetMsg(wp)) != NULL) {
        if (desk_amiga_input_handle(&b->in, m)) result = DESK_AGA_ACTIVITY;
        ReplyMsg((struct Message*)m);
    }
    return result;
}

int desk_aga_next_input(desk_aga* b, sel_gui_input* ev) { return desk_amiga_input_next(&b->in, ev); }
int desk_aga_open_folder_dialog(desk_aga* b) { return desk_amiga_input_pick_folder(&b->in, b->screen); }
