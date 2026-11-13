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
#include <graphics/copper.h>
#include <graphics/gfxmacros.h>
#include <graphics/view.h>
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
#include "../../../../third_party/kalms-c2p/normal/c2p1x1_8_c5_030.h"

struct IntuitionBase* IntuitionBase;
struct GfxBase* GfxBase;

#define AGA_GAP 8 /* rows: two bands this close are converted as one */
#define AGA_NO_MODE ((ULONG)INVALID_ID)

#define AGA_COPPER_MAX 600u /* pen colour changes per list: a few hundred lines of gradient */

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
    /* I441: the installed copper list and the changes it was built from */
    struct UCopList* ucl;
    sel_gui_copper_change cop[AGA_COPPER_MAX];
    uint32_t cop_n, cop_installs, cop_logged;
    /* I1260: the effect screen */
    struct Screen* fx_screen;
    struct BitMap fx_bm[2];
    UBYTE* fx_buf[2];
    struct ScreenBuffer* fx_sb[2];
    struct MsgPort* fx_port;
    uint8_t* fx_chunky; /* fast RAM, AGA_FX_W x fx_h */
    int32_t fx_h;
    int fx_safe, fx_back, fx_failed, fx_pal_set;
    uint32_t fx_palette[256];
    uint32_t fx_c2p_kind, fx_presents, fx_opens, fx_scale_ms, fx_c2p_ms; /* stage sums on the 20 ms DOS clock: only the mean over many frames says anything */
};

static void fx_close(desk_aga* b); /* I1260: desk_aga_close and desk_aga_present close the effect screen */

/* Rows y .. y + h of a chunky image w wide into the same rows of 8 contiguous planes of bplsize bytes each. */
static void c2p_rows(uint32_t kind, const uint8_t* chunky, UBYTE* planes, int32_t w, uint32_t bplsize, int32_t y, int32_t h) {
    if (kind == DESK_AGA_C2P_KALMS_040) {
        /* Kalms: rows y .. y + h of the chunky buffer into the same rows of the planes (scroffsy = y). */
        c2p1x1_8_c5_040_init(w, h, y, (int)bplsize);
        c2p1x1_8_c5_040((void*)(chunky + (size_t)y * (size_t)w), planes);
    } else if (kind == DESK_AGA_C2P_KALMS_030) {
        /* I574: the 030 routine patches its own code with the sizes; flush the caches before running it. */
        c2p1x1_8_c5_030_smcinit(w, h, y, (int)bplsize);
        CacheClearU();
        c2p1x1_8_c5_030((void*)(chunky + (size_t)y * (size_t)w), planes);
    } else {
        desk_aga_c2p_ref(chunky, w, y, h, planes, bplsize);
    }
}

static void c2p_bands(desk_aga* b, const uint8_t* chunky, const desk_aga_band* band, uint32_t n) {
    uint32_t i, bplsize = (uint32_t)(b->w / 8) * (uint32_t)b->h;
    for (i = 0; i < n; i++) {
        c2p_rows(b->c2p_kind, chunky, b->buf[b->back], b->w, bplsize, band[i].y, band[i].h);
        b->c2p_calls++;
        b->upload_pixels += (uint32_t)band[i].h * (uint32_t)b->w;
    }
}

uint32_t desk_aga_ticks(void) {
    struct DateStamp ds;
    DateStamp(&ds);
    return (uint32_t)ds.ds_Days * 86400000u + (uint32_t)ds.ds_Minute * 60000u + (uint32_t)ds.ds_Tick * 20u;
}

/* The mode of the screen: Selecta's own PAL/NTSC HIRES (640 wide) or LORES (320 wide), never Workbench's. The
 * Workbench mode is irrelevant: an RTG Workbench (rig 2: Picasso96 800x600) must not leak its size or its mode here,
 * a planar screen of that size cannot be displayed by the chipset and shows black. desk_aga_pick_size holds the rule. */
static int g_overscan; /* I566: pref ui.desktop.overscan */

void desk_aga_set_overscan(int on) {
    g_overscan = on != 0;
}

static void max_overscan(int ntsc, int32_t rect[4]) {
    struct Rectangle r;
    ULONG hires = (ntsc ? NTSC_MONITOR_ID : PAL_MONITOR_ID) | HIRES_KEY;
    rect[0] = rect[1] = rect[2] = rect[3] = 0;
    if (!g_overscan) return; /* off: no QueryOverscan at all (I566: the hang suspect); pick_size keeps the standard size */
    if (QueryOverscan(hires, &r, DESK_AGA_OSCAN_TYPE)) {
        rect[0] = r.MinX;
        rect[1] = r.MinY;
        rect[2] = r.MaxX;
        rect[3] = r.MaxY;
    }
    /* I566: the area decides the Fill Screen size; the log shows what the Overscan preferences gave */
    fprintf(stderr, "aga: overscan area %ld,%ld..%ld,%ld\n", (long)rect[0], (long)rect[1], (long)rect[2], (long)rect[3]);
}

static ULONG pick_mode(int32_t* w, int32_t* h) {
    int ntsc = (GfxBase->DisplayFlags & NTSC) != 0;
    int32_t mr[4];
    int hires;
    int32_t mw, mh;
    max_overscan(ntsc, mr);
    mw = mr[2] - mr[0] + 1;
    mh = mr[3] - mr[1] + 1;
    desk_aga_overscan_area(g_overscan, &mw, &mh);
    hires = desk_aga_pick_size(w, h, ntsc, mw, mh);
    ULONG mode;
    if (hires < 0) return AGA_NO_MODE;
    mode = (ntsc ? NTSC_MONITOR_ID : PAL_MONITOR_ID) | (hires ? HIRES_KEY : LORES_KEY);
    return ModeNotAvailable(mode) == 0 ? mode : AGA_NO_MODE;
}

void desk_aga_default_size(int32_t* w, int32_t* h) {
    *w = 0;
    *h = 0;
    GfxBase = (struct GfxBase*)OpenLibrary((CONST_STRPTR) "graphics.library", 39);
    /* I566: QueryOverscan is an intuition.library call; through the NULL IntuitionBase it jumped into low memory and froze the
     * whole machine as soon as ui.desktop.overscan was read. */
    IntuitionBase = (struct IntuitionBase*)OpenLibrary((CONST_STRPTR) "intuition.library", 39);
    if (GfxBase != NULL && IntuitionBase != NULL) {
        int ntsc = (GfxBase->DisplayFlags & NTSC) != 0;
        int32_t mr[4];
        int32_t mw, mh;
        max_overscan(ntsc, mr);
        mw = mr[2] - mr[0] + 1;
        mh = mr[3] - mr[1] + 1;
        desk_aga_overscan_area(g_overscan, &mw, &mh);
        (void)desk_aga_pick_size(w, h, ntsc, mw, mh);
    }
    if (IntuitionBase != NULL) CloseLibrary((struct Library*)IntuitionBase);
    IntuitionBase = NULL;
    if (GfxBase != NULL) CloseLibrary((struct Library*)GfxBase);
    GfxBase = NULL;
}

static int fail(desk_aga* b, const char* why) {
    fprintf(stderr, "aga: %s\n", why); /* stderr: --log redirects it, stdout would hide the reason (I522) */
    fflush(stderr);
    desk_amiga_why_set(why);
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
        desk_amiga_why_set("The AGA screen shows an INDEX8 canvas only. Use --format index8 or leave --format out.");
        return NULL;
    }
    b = (desk_aga*)AllocVec(sizeof *b, MEMF_CLEAR);
    if (b == NULL) {
        desk_amiga_why_set("Not enough memory to open the AGA screen. Close other programs, then start again.");
        return NULL;
    }
    b->safe = 1; /* no ChangeScreenBuffer is pending: close must not wait for a safe message that never comes */
    GfxBase = (struct GfxBase*)OpenLibrary((CONST_STRPTR) "graphics.library", 39);
    IntuitionBase = (struct IntuitionBase*)OpenLibrary((CONST_STRPTR) "intuition.library", 39);
    if (GfxBase == NULL || IntuitionBase == NULL) return fail(b, "The AGA screen needs graphics.library and intuition.library version 39 (OS 3.0 or newer). Use OS 3.0 or newer, or start Selecta with --ui text."), NULL;
    if (!desk_amiga_input_open(&b->in)) return fail(b, "The AGA screen needs keymap.library version 37. Install keymap.library in LIBS:, or start Selecta with --ui text."), NULL;
    if ((GfxBase->ChipRevBits0 & GFXF_AA_ALICE) == 0) return fail(b, "This computer has no AGA chipset (A1200, A4000). Use the MUI or RTG screen, or start Selecta with --ui text."), NULL;
    mode = pick_mode(&w, &h);
    if (mode == AGA_NO_MODE) return fail(b, "No AGA screen mode of 640x256 or 320x256 (640x200 or 320x200 on NTSC) is available. Choose a PAL or NTSC mode in the Screenmode preferences."), NULL;
    b->w = w;
    b->h = h;
    bytes = desk_aga_buffer_bytes(w, h);
    if (AvailMem(MEMF_CHIP | MEMF_LARGEST) < desk_aga_chip_bytes(w, h) / 2u ||
        AvailMem(MEMF_CHIP) < desk_aga_chip_bytes(w, h)) {
        return fail(b, "Not enough chip memory for the AGA screen (two 8-bitplane buffers). Close other programs to free chip memory, then start again."), NULL;
    }
    for (i = 0; i < 2; i++) {
        b->buf[i] = (UBYTE*)AllocMem((ULONG)bytes, MEMF_CHIP | MEMF_CLEAR);
        if (b->buf[i] == NULL) return fail(b, "Not enough chip memory for the AGA screen. Close other programs to free chip memory, then start again."), NULL;
        InitBitMap(&b->bm[i], 8, (LONG)w, (LONG)h);
        for (p = 0; p < 8; p++) b->bm[i].Planes[p] = (PLANEPTR)(b->buf[i] + (size_t)p * (bytes / 8u));
    }
    {
        /* I566: a screen larger than the standard 640 x lines is the overscan fit: its display clip sits centred in the
         * user's OSCAN_STANDARD area, otherwise the clip stays the standard one and the rest of the display is border. */
        int32_t mr[4], dc[4];
        struct Rectangle clip;
        int fit = w > 640 || h > ((GfxBase->DisplayFlags & NTSC) ? 200 : 256);
        max_overscan((GfxBase->DisplayFlags & NTSC) != 0, mr);
        desk_aga_fit_clip(mr, w, h, dc);
        clip.MinX = (WORD)dc[0];
        clip.MinY = (WORD)dc[1];
        clip.MaxX = (WORD)dc[2];
        clip.MaxY = (WORD)dc[3];
        /* I566: the border shows COLOR00 (the accent blue). SA_VideoControl hands graphics.library VTAG_BORDERBLANK_SET at
         * open: it sets BRDRBLNK and ECSENA (the chipset ignores BRDRBLNK without ECSENA) with no MakeScreen/RethinkDisplay
         * on a running system; the copper BPLCON3 writes keep BRDRBLNK set. */
        uint32_t vt[4];
        struct TagItem st[] = {{SA_Width, (ULONG)w}, {SA_Height, (ULONG)h}, {SA_Depth, 8},
                               {SA_DisplayID, mode}, {SA_BitMap, (ULONG)&b->bm[0]}, {SA_Quiet, TRUE},
                               {SA_ShowTitle, FALSE}, {SA_Type, CUSTOMSCREEN}, {SA_Exclusive, TRUE},
                               {fit ? SA_DClip : TAG_IGNORE, (ULONG)&clip},
                               {SA_VideoControl, (ULONG)vt},
                               {SA_Title, (ULONG) "Selecta"}, {TAG_DONE, 0}};
        (void)desk_aga_border_blank_tags(vt);
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
    b->c2p_kind = desk_aga_c2p_pick(SysBase->AttnFlags, (uint32_t)(w / 8) * (uint32_t)h);
    return b;
}

void desk_aga_close(desk_aga* b) {
    int i;
    if (b == NULL) return;
    if (b->fx_presents > 0u) { /* I1260: the sentinel and stage times of the effect screen */
        printf("aga_direct opens=%lu presents=%lu c2p_kind=%lu scale_us=%lu c2p_us=%lu\n", (unsigned long)b->fx_opens,
               (unsigned long)b->fx_presents, (unsigned long)b->fx_c2p_kind,
               (unsigned long)(b->fx_scale_ms * 1000u / b->fx_presents),
               (unsigned long)(b->fx_c2p_ms * 1000u / b->fx_presents));
        fflush(stdout);
    }
    if (b->screen != NULL) fx_close(b);
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
    if (b->screen != NULL && b->ucl != NULL) { /* the list is ours: unhook it before the screen goes */
        b->screen->ViewPort.UCopIns = NULL;
        if (b->ucl->FirstCopList != NULL) FreeCopList(b->ucl->FirstCopList);
        FreeMem(b->ucl, sizeof *b->ucl);
    }
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

uint32_t desk_aga_copper_changes(const desk_aga* b) { return b->cop_n; }
uint32_t desk_aga_copper_installs(const desk_aga* b) { return b->cop_installs; }

/* I441: replaces the screen's user copper list with one that sets the gradient pens per scanline. */
static void copper_install(desk_aga* b, const sel_gui_copper_change* ch, uint32_t n) {
    struct UCopList* nl = NULL;
    struct UCopList* old;
    uint32_t i, k;
    int32_t last_v = -1;
    { /* I566: the list always exists, even with no gradient pens: it carries the border blank */
        nl = (struct UCopList*)AllocMem(sizeof *nl, MEMF_PUBLIC | MEMF_CLEAR);
        if (nl == NULL) return;
        CINIT(nl, (UWORD)(n * 5u + 6u));
        CMOVE(nl, *(volatile UWORD*)(0xDFF106u), (WORD)DESK_AGA_BPLCON3);
        for (i = 0; i < n; i++) {
            uint32_t regs[4][2];
            int32_t v = b->h > 300 ? ch[i].y / 2 : ch[i].y;
            if (v != last_v) {
                CWAIT(nl, (WORD)v, 0);
                last_v = v;
            }
            desk_aga_copper_regs(ch[i].pen, ch[i].rgb, regs);
            for (k = 0; k < 4u; k++) {
                CMOVE(nl, *(volatile UWORD*)(0xDFF000u + regs[k][0]), (WORD)regs[k][1]);
            }
        }
        CMOVE(nl, *(volatile UWORD*)(0xDFF106u), (WORD)DESK_AGA_BPLCON3);
        CEND(nl);
    }
    Forbid();
    old = b->screen->ViewPort.UCopIns;
    b->screen->ViewPort.UCopIns = nl;
    Permit();
    MakeScreen(b->screen);
    RethinkDisplay();
    if (old != NULL) {
        if (old->FirstCopList != NULL) FreeCopList(old->FirstCopList);
        FreeMem(old, sizeof *old);
    }
    b->ucl = nl;
    fprintf(stderr, "aga: copper list installed: %lu changes, UCopIns=%s, copper list=%s\n", (unsigned long)n,
            b->screen->ViewPort.UCopIns != NULL ? "set" : "NULL", b->screen->ViewPort.UCopIns != NULL && b->screen->ViewPort.UCopIns->FirstCopList != NULL ? "built" : "NULL");
    memcpy(b->cop, ch, n * sizeof ch[0]);
    b->cop_n = n;
    b->cop_installs++;
}

/* ---- I1260: the effect screen ---- */
#define AGA_FX_W 320

static void fx_close(desk_aga* b) {
    int i;
    if (b->fx_screen != NULL && !b->fx_safe && b->fx_port != NULL) {
        while (GetMsg(b->fx_port) == NULL) WaitPort(b->fx_port);
    }
    b->fx_safe = 1;
    for (i = 1; i >= 0; i--) {
        if (b->fx_sb[i] != NULL) FreeScreenBuffer(b->fx_screen, b->fx_sb[i]);
        b->fx_sb[i] = NULL;
    }
    if (b->fx_port != NULL) DeleteMsgPort(b->fx_port);
    b->fx_port = NULL;
    if (b->fx_screen != NULL) CloseScreen(b->fx_screen);
    b->fx_screen = NULL;
    for (i = 0; i < 2; i++) {
        if (b->fx_buf[i] != NULL) FreeMem(b->fx_buf[i], (ULONG)desk_aga_buffer_bytes(AGA_FX_W, b->fx_h));
        b->fx_buf[i] = NULL;
    }
    if (b->fx_chunky != NULL) FreeVec(b->fx_chunky);
    b->fx_chunky = NULL;
}

static int fx_open(desk_aga* b) {
    ULONG mode = GetVPModeID(&b->screen->ViewPort) & ~(ULONG)(HIRES_KEY | LORESLACE_KEY);
    size_t bytes;
    int i, p;
    b->fx_h = (GfxBase->DisplayFlags & NTSC) ? 200 : 256;
    bytes = desk_aga_buffer_bytes(AGA_FX_W, b->fx_h);
    if (AvailMem(MEMF_CHIP | MEMF_LARGEST) < bytes || AvailMem(MEMF_CHIP) < 2u * bytes) {
        fprintf(stderr, "aga: effect screen: not enough chip RAM, the viz stays on the desk screen\n");
        return 0;
    }
    b->fx_chunky = (uint8_t*)AllocVec((ULONG)AGA_FX_W * (ULONG)b->fx_h, MEMF_ANY);
    for (i = 0; i < 2; i++) {
        b->fx_buf[i] = (UBYTE*)AllocMem((ULONG)bytes, MEMF_CHIP | MEMF_CLEAR);
        if (b->fx_buf[i] == NULL) break;
        InitBitMap(&b->fx_bm[i], 8, AGA_FX_W, (LONG)b->fx_h);
        for (p = 0; p < 8; p++) b->fx_bm[i].Planes[p] = (PLANEPTR)(b->fx_buf[i] + (size_t)p * (bytes / 8u));
    }
    if (b->fx_chunky != NULL && b->fx_buf[0] != NULL && b->fx_buf[1] != NULL) {
        /* No border blank: the border shows colour 0, the effect's own background. */
        struct TagItem st[] = {{SA_Width, AGA_FX_W}, {SA_Height, (ULONG)b->fx_h}, {SA_Depth, 8},
                               {SA_DisplayID, mode}, {SA_BitMap, (ULONG)&b->fx_bm[0]}, {SA_Quiet, TRUE},
                               {SA_ShowTitle, FALSE}, {SA_Type, CUSTOMSCREEN}, {SA_Exclusive, TRUE},
                               {SA_Title, (ULONG) "Selecta visualizer"}, {TAG_DONE, 0}};
        b->fx_screen = OpenScreenTagList(NULL, st);
    }
    if (b->fx_screen != NULL) {
        b->fx_sb[0] = AllocScreenBuffer(b->fx_screen, &b->fx_bm[0], 0);
        b->fx_sb[1] = AllocScreenBuffer(b->fx_screen, &b->fx_bm[1], 0);
        b->fx_port = CreateMsgPort();
    }
    if (b->fx_screen == NULL || b->fx_sb[0] == NULL || b->fx_sb[1] == NULL || b->fx_port == NULL) {
        fprintf(stderr, "aga: effect screen: open failed (mode %08lx), the viz stays on the desk screen\n", (unsigned long)mode);
        fx_close(b);
        return 0;
    }
    for (i = 0; i < 2; i++) b->fx_sb[i]->sb_DBufInfo->dbi_SafeMessage.mn_ReplyPort = b->fx_port;
    b->fx_safe = 1;
    b->fx_back = 1;
    b->fx_pal_set = 0;
    b->fx_c2p_kind = desk_aga_c2p_pick(SysBase->AttnFlags, (uint32_t)(AGA_FX_W / 8) * (uint32_t)b->fx_h);
    b->fx_opens++;
    ActivateWindow(b->window); /* keys and clicks stay with the desk window */
    return 1;
}

int desk_aga_direct_show(desk_aga* b, const uint8_t* idx, uint32_t w, uint32_t h, const uint32_t* rgb) {
    uint32_t t0, t1;
    if (b == NULL || b->fx_failed || idx == NULL || rgb == NULL || w != AGA_FX_W / 2u || h < 2u) return 0;
    if (b->fx_screen == NULL && !fx_open(b)) {
        b->fx_failed = 1;
        return 0;
    }
    t0 = desk_aga_ticks();
    if (!b->fx_pal_set || memcmp(rgb, b->fx_palette, sizeof b->fx_palette) != 0) {
        memcpy(b->fx_palette, rgb, sizeof b->fx_palette);
        desk_aga_rgb32_table(b->fx_palette, 0, 256, b->rgb32);
        LoadRGB32(&b->fx_screen->ViewPort, (ULONG*)b->rgb32);
        b->fx_pal_set = 1;
    }
    desk_aga_index_scale2(idx, w, h, b->fx_chunky, (uint32_t)b->fx_h);
    t1 = desk_aga_ticks();
    b->fx_scale_ms += t1 - t0;
    if (!b->fx_safe) {
        while (GetMsg(b->fx_port) == NULL) WaitPort(b->fx_port);
        b->fx_safe = 1;
    }
    t0 = desk_aga_ticks();
    c2p_rows(b->fx_c2p_kind, b->fx_chunky, b->fx_buf[b->fx_back], AGA_FX_W, (uint32_t)(AGA_FX_W / 8) * (uint32_t)b->fx_h, 0, b->fx_h);
    b->fx_c2p_ms += desk_aga_ticks() - t0;
    while (!ChangeScreenBuffer(b->fx_screen, b->fx_sb[b->fx_back])) WaitTOF();
    b->fx_safe = 0;
    b->fx_back ^= 1;
    b->fx_presents++;
    return 1;
}

uint32_t desk_aga_direct_presents(const desk_aga* b) { return b->fx_presents; }
uint32_t desk_aga_direct_opens(const desk_aga* b) { return b->fx_opens; }

void desk_aga_present(desk_aga* b, const sel_gui_canvas* c) {
    desk_aga_band now[DESK_AGA_BANDS_MAX], todo[DESK_AGA_BANDS_MAX];
    uint32_t n, nt;
    int fx_was_up = b->fx_screen != NULL;
    if (c->format != SEL_GUI_INDEX8 || c->w != b->w || c->h != b->h) return;
    if (fx_was_up) b->fresh = 2; /* I1260: the desk buffers are stale behind the effect screen: convert both whole */
    if (c->colours != b->colours || memcmp(c->palette, b->palette, sizeof b->palette) != 0) {
        memcpy(b->palette, c->palette, sizeof b->palette);
        b->colours = c->colours;
        desk_aga_rgb32_table(b->palette, 0, 256, b->rgb32);
        LoadRGB32(&b->screen->ViewPort, (ULONG*)b->rgb32);
    }
    if (b->cop_logged++ == 0u) fprintf(stderr, "aga: present: copper_pens=%lu copper_n=%lu\n", (unsigned long)c->copper_pens, (unsigned long)c->copper_n);
    if (c->copper_pens > 0u) {
        sel_gui_copper_change ch[AGA_COPPER_MAX];
        uint32_t nc = sel_gui_copper_changes(c, ch, AGA_COPPER_MAX);
        if (b->cop_installs == 0u || nc != b->cop_n || memcmp(ch, b->cop, nc * sizeof ch[0]) != 0) copper_install(b, ch, nc);
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
    if (fx_was_up) fx_close(b); /* the desk picture is up to date: uncover it */
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
