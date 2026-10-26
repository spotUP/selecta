/* The rtg backend of the desktop UI; see backend_rtg.h. Picasso96 is used through its LVOs with the ndk inline
 * macros, because the toolchain ships no Picasso96 headers and the Replay has Picasso96API.library but no
 * cybergraphics.library. The offsets and tag values are those of Picasso96API's fd file and Picasso96.h. */
#include "backend_rtg.h"

/* The library bases are renamed: libnix opens a library at program start when its base symbol (GfxBase, ...) is
 * referenced, which made every binary linking this file (selecta-cli too) need graphics.library and keymap.library
 * even without a UI. This backend opens and closes its libraries itself in desk_rtg_open / desk_rtg_close. */
#define IntuitionBase sel_rtg_IntuitionBase
#define GfxBase sel_rtg_GfxBase
#define KeymapBase sel_rtg_KeymapBase

#include <selecta/uimodel/keys.h>

#include <devices/timer.h>
#include <exec/io.h>
#include <exec/memory.h>
#include <devices/inputevent.h>
#include <dos/dos.h>
#include <inline/macros.h>
#include <intuition/intuition.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/keymap.h>
#include <stdio.h>
#include <string.h>
#include <utility/tagitem.h>

struct IntuitionBase* IntuitionBase;
struct GfxBase* GfxBase;
struct Library* KeymapBase;
static struct Library* P96Base;

#define DESK_RTG_QUEUE 64

/* Picasso96.h: struct RenderInfo, RGBFB_A8R8G8B8, P96IDA_*, P96BIDTAG_*. */
struct rtg_render_info {
    APTR memory;
    WORD bytes_per_row;
    WORD pad;
    ULONG rgb_format;
};
#define RTG_FMT_A8R8G8B8 6u
#define RTG_FMT_CLUT 1u /* RGBFB_CLUT */
#define RTG_FORMATS_CLUT 0x2u /* RGBFF_CLUT */
#define RTG_FORMATS_COLOR 0x3FFCu /* RGBFF_R8G8B8 up to RGBFF_B5G5R5PC: every direct-colour format */
#define RTG_IDA_WIDTH 0
#define RTG_IDA_HEIGHT 1
#define RTG_IDA_BITSPERPIXEL 4
#define RTG_IDA_ISP96 6
#define RTG_BID_DUMMY (TAG_USER + 96)

static ULONG p96_best_mode(struct TagItem* tags) {
    return LP1(0x3c, ULONG, p96BestModeIDTagList, struct TagItem*, tags, a0, , P96Base);
}
static ULONG p96_mode_attr(ULONG mode, ULONG attr) {
    return LP2(0x54, ULONG, p96GetModeIDAttr, ULONG, mode, d0, ULONG, attr, d1, , P96Base);
}
static void p96_write(struct rtg_render_info* ri, LONG sx, LONG sy, struct RastPort* rp, LONG dx, LONG dy, LONG w,
                      LONG h) {
    LP8NR(0x66, p96WritePixelArray, struct rtg_render_info*, ri, a0, UWORD, sx, d0, UWORD, sy, d1, struct RastPort*, rp,
          a1, UWORD, dx, d2, UWORD, dy, d3, UWORD, w, d4, UWORD, h, d5, , P96Base);
}

struct desk_rtg {
    struct Screen* screen;
    struct Window* window;
    struct MsgPort* tport;
    struct timerequest* treq;
    int32_t w, h, depth;
    int index8;
    uint32_t pal[256]; /* the palette last loaded on the screen (0xRRGGBB) */
    int pal_set;
    uint32_t presents, upload;
    sel_gui_input_map_state map;
    sel_gui_input queue[DESK_RTG_QUEUE];
    uint32_t q_head, q_count;
    uint32_t last_sec, last_micros;
    int32_t last_x, last_y;
    uint32_t clicks;
    struct InputEvent ie;
};

uint32_t desk_rtg_ticks(void) {
    struct DateStamp ds;
    DateStamp(&ds);
    return (uint32_t)ds.ds_Days * 86400000u + (uint32_t)ds.ds_Minute * 60000u + (uint32_t)ds.ds_Tick * 20u;
}

/* Picks the first size of the wanted list the card offers as a real Picasso96 mode; returns the mode id or 0. */
static ULONG pick_mode(int32_t* w, int32_t* h, ULONG* depth, int index8) {
    static const int32_t sizes[][2] = {{1920, 1080}, {1600, 900}, {1280, 800}, {1280, 720},
                                       {1024, 768},  {800, 600},  {640, 480}};
    size_t i;
    for (i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
        struct TagItem tags[5];
        ULONG mode;
        if (*w > 0 && *h > 0 && (sizes[i][0] != *w || sizes[i][1] != *h)) {
            if (i + 1 < sizeof sizes / sizeof sizes[0]) {
                continue;
            }
            tags[0].ti_Data = (ULONG)*w;
            tags[1].ti_Data = (ULONG)*h;
        } else {
            tags[0].ti_Data = (ULONG)sizes[i][0];
            tags[1].ti_Data = (ULONG)sizes[i][1];
        }
        tags[0].ti_Tag = RTG_BID_DUMMY + 3;
        tags[1].ti_Tag = RTG_BID_DUMMY + 4;
        tags[2].ti_Tag = RTG_BID_DUMMY + 1;
        tags[2].ti_Data = index8 ? RTG_FORMATS_CLUT : RTG_FORMATS_COLOR;
        tags[3].ti_Tag = RTG_BID_DUMMY + 5;
        tags[3].ti_Data = index8 ? 8 : 16;
        tags[4].ti_Tag = TAG_DONE;
        tags[4].ti_Data = 0;
        mode = p96_best_mode(tags);
        if (mode != (ULONG)INVALID_ID && p96_mode_attr(mode, RTG_IDA_ISP96) != 0u &&
            p96_mode_attr(mode, RTG_IDA_WIDTH) == tags[0].ti_Data &&
            p96_mode_attr(mode, RTG_IDA_HEIGHT) == tags[1].ti_Data) {
            *w = (int32_t)tags[0].ti_Data;
            *h = (int32_t)tags[1].ti_Data;
            *depth = p96_mode_attr(mode, RTG_IDA_BITSPERPIXEL);
            return mode;
        }
    }
    return 0;
}

void desk_rtg_default_size(int32_t* w, int32_t* h) {
    struct IntuitionBase* ib = (struct IntuitionBase*)OpenLibrary((CONST_STRPTR) "intuition.library", 37);
    *w = 1280;
    *h = 800;
    if (ib != NULL) {
        struct IntuitionBase* saved = IntuitionBase;
        struct Screen* wb;
        IntuitionBase = ib;
        wb = LockPubScreen(NULL);
        if (wb != NULL) {
            *w = wb->Width;
            *h = wb->Height;
            UnlockPubScreen(NULL, wb);
        }
        IntuitionBase = saved;
        CloseLibrary((struct Library*)ib);
    }
}

desk_rtg* desk_rtg_open(int32_t w, int32_t h, int index8) {
    desk_rtg* b = (desk_rtg*)AllocVec(sizeof *b, MEMF_CLEAR);
    ULONG mode, depth = 0;
    if (b == NULL) {
        return NULL;
    }
    IntuitionBase = (struct IntuitionBase*)OpenLibrary((CONST_STRPTR) "intuition.library", 37);
    GfxBase = (struct GfxBase*)OpenLibrary((CONST_STRPTR) "graphics.library", 37);
    KeymapBase = OpenLibrary((CONST_STRPTR) "keymap.library", 37);
    P96Base = OpenLibrary((CONST_STRPTR) "Picasso96API.library", 0);
    if (IntuitionBase == NULL || GfxBase == NULL || KeymapBase == NULL) {
        fprintf(stdout, "rtg: intuition, graphics or keymap library missing\n"); fflush(stdout);
        desk_rtg_close(b);
        return NULL;
    }
    if (P96Base == NULL) {
        fprintf(stdout, "rtg: Picasso96API.library missing\n"); fflush(stdout);
        desk_rtg_close(b);
        return NULL;
    }
    mode = pick_mode(&w, &h, &depth, index8);
    if (mode == 0u) {
        fprintf(stdout, "rtg: no Picasso96 mode of the wanted size\n"); fflush(stdout);
        desk_rtg_close(b);
        return NULL;
    }
    b->screen = OpenScreenTags(NULL, SA_Width, (ULONG)w, SA_Height, (ULONG)h, SA_Depth, depth, SA_DisplayID, mode,
                               SA_Quiet, TRUE, SA_ShowTitle, FALSE, SA_Type, CUSTOMSCREEN, TAG_DONE);
    if (b->screen == NULL) {
        fprintf(stdout, "rtg: the %dx%d screen did not open\n", (int)w, (int)h); fflush(stdout);
        desk_rtg_close(b);
        return NULL;
    }
    b->window = OpenWindowTags(NULL, WA_CustomScreen, (ULONG)b->screen, WA_Left, 0, WA_Top, 0, WA_Width, (ULONG)w,
                               WA_Height, (ULONG)h, WA_Borderless, TRUE, WA_Backdrop, TRUE, WA_Activate, TRUE,
                               WA_RMBTrap, TRUE, WA_ReportMouse, TRUE, WA_NoCareRefresh, TRUE, WA_SimpleRefresh, TRUE,
                               WA_IDCMP, IDCMP_RAWKEY | IDCMP_MOUSEBUTTONS | IDCMP_MOUSEMOVE, TAG_DONE);
    b->tport = CreateMsgPort();
    b->treq = b->tport != NULL ? (struct timerequest*)CreateIORequest(b->tport, sizeof(struct timerequest)) : NULL;
    if (b->window == NULL || b->treq == NULL ||
        OpenDevice((CONST_STRPTR) "timer.device", UNIT_VBLANK, (struct IORequest*)b->treq, 0) != 0) {
        fprintf(stdout, "rtg: window or timer did not open\n"); fflush(stdout);
        if (b->treq != NULL) {
            DeleteIORequest((struct IORequest*)b->treq);
            b->treq = NULL;
        }
        desk_rtg_close(b);
        return NULL;
    }
    b->w = w;
    b->h = h;
    b->depth = (int32_t)depth;
    b->index8 = index8;
    fprintf(stdout, "rtg: screen %dx%d depth %d\n", (int)w, (int)h, (int)depth); fflush(stdout);
    return b;
}

void desk_rtg_close(desk_rtg* b) {
    if (b == NULL) {
        return;
    }
    if (b->treq != NULL) {
        if (b->treq->tr_node.io_Device != NULL) {
            CloseDevice((struct IORequest*)b->treq);
        }
        DeleteIORequest((struct IORequest*)b->treq);
    }
    if (b->tport != NULL) {
        DeleteMsgPort(b->tport);
    }
    if (b->window != NULL) {
        CloseWindow(b->window);
    }
    if (b->screen != NULL) {
        CloseScreen(b->screen);
    }
    if (P96Base != NULL) {
        CloseLibrary(P96Base);
        P96Base = NULL;
    }
    if (KeymapBase != NULL) {
        CloseLibrary(KeymapBase);
        KeymapBase = NULL;
    }
    if (GfxBase != NULL) {
        CloseLibrary((struct Library*)GfxBase);
        GfxBase = NULL;
    }
    if (IntuitionBase != NULL) {
        CloseLibrary((struct Library*)IntuitionBase);
        IntuitionBase = NULL;
    }
    FreeVec(b);
}

int32_t desk_rtg_width(const desk_rtg* b) { return b->w; }
int32_t desk_rtg_height(const desk_rtg* b) { return b->h; }
int32_t desk_rtg_depth(const desk_rtg* b) { return b->depth; }
uint32_t desk_rtg_upload_pixels(const desk_rtg* b) { return b->upload; }
uint32_t desk_rtg_presents(const desk_rtg* b) { return b->presents; }

void desk_rtg_present(desk_rtg* b, const sel_gui_canvas* c) {
    struct rtg_render_info ri;
    uint32_t i;
    ri.memory = (APTR)c->pixels;
    ri.pad = 0;
    if (b->index8 && c->format == SEL_GUI_INDEX8) {
        ri.bytes_per_row = (WORD)c->w;
        ri.rgb_format = RTG_FMT_CLUT;
        if (!b->pal_set || memcmp(b->pal, c->palette, sizeof b->pal) != 0) { /* LoadRGB32: count<<16|first, r,g,b as left-aligned 32-bit, 0 */
            static ULONG tab[2 + 256 * 3];
            uint32_t k;
            tab[0] = (256UL << 16) | 0UL;
            for (k = 0; k < 256u; k++) {
                tab[1 + k * 3] = ((c->palette[k] >> 16) & 0xFFu) * 0x01010101UL;
                tab[2 + k * 3] = ((c->palette[k] >> 8) & 0xFFu) * 0x01010101UL;
                tab[3 + k * 3] = (c->palette[k] & 0xFFu) * 0x01010101UL;
            }
            tab[1 + 256 * 3] = 0;
            LoadRGB32(&b->screen->ViewPort, tab);
            memcpy(b->pal, c->palette, sizeof b->pal);
            b->pal_set = 1;
        }
    } else {
        ri.bytes_per_row = (WORD)(c->w * 4);
        ri.rgb_format = RTG_FMT_A8R8G8B8;
    }
    for (i = 0; i < c->dirty_n; i++) {
        sel_gui_rect r = c->dirty[i];
        if (r.x < 0) {
            r.w += r.x;
            r.x = 0;
        }
        if (r.y < 0) {
            r.h += r.y;
            r.y = 0;
        }
        if (r.x + r.w > b->w) {
            r.w = b->w - r.x;
        }
        if (r.y + r.h > b->h) {
            r.h = b->h - r.y;
        }
        if (r.w <= 0 || r.h <= 0) {
            continue;
        }
        p96_write(&ri, r.x, r.y, b->window->RPort, r.x, r.y, r.w, r.h);
        b->upload += (uint32_t)(r.w * r.h);
    }
    b->presents++;
}

static int queue_raw(desk_rtg* b, const sel_gui_raw* raw) {
    sel_gui_input in;
    if (!sel_gui_input_map(&b->map, raw, &in) || b->q_count >= DESK_RTG_QUEUE) {
        return 0;
    }
    b->queue[(b->q_head + b->q_count) % DESK_RTG_QUEUE] = in;
    b->q_count++;
    return 1;
}

int desk_rtg_next_input(desk_rtg* b, sel_gui_input* ev) {
    if (b->q_count == 0u) {
        return 0;
    }
    *ev = b->queue[b->q_head];
    b->q_head = (b->q_head + 1u) % DESK_RTG_QUEUE;
    b->q_count--;
    return 1;
}

static uint32_t mods_of(UWORD q) {
    uint32_t m = 0;
    if (q & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT)) {
        m |= TXT_MOD_SHIFT;
    }
    if (q & IEQUALIFIER_CONTROL) {
        m |= TXT_MOD_CTRL;
    }
    if (q & (IEQUALIFIER_LALT | IEQUALIFIER_RALT)) {
        m |= TXT_MOD_ALT;
    }
    return m;
}

/* The key code of a raw key that has no character: arrows, function keys and the edit block. 0 when it has one. */
static uint32_t special_code(UWORD raw) {
    switch (raw) {
    case 0x4C: return TXT_KEY_UP;
    case 0x4D: return TXT_KEY_DOWN;
    case 0x4E: return TXT_KEY_RIGHT;
    case 0x4F: return TXT_KEY_LEFT;
    case 0x46: return TXT_KEY_DELETE;
    case 0x47: return TXT_KEY_INSERT;
    case 0x48: return TXT_KEY_PAGE_UP;
    case 0x49: return TXT_KEY_PAGE_DOWN;
    case 0x70: return TXT_KEY_HOME;
    case 0x71: return TXT_KEY_END;
    case 0x4B: return TXT_KEY_F1 + 10;
    case 0x6F: return TXT_KEY_F1 + 11;
    case 0x44: case 0x43: return 13;
    case 0x45: return 27;
    case 0x42: return 9;
    case 0x41: return 127;
    default: break;
    }
    if (raw >= 0x50 && raw <= 0x59) {
        return TXT_KEY_F1 + (uint32_t)(raw - 0x50);
    }
    return 0;
}

static int queue_key(desk_rtg* b, UWORD code, UWORD qual, APTR addr) {
    sel_gui_raw raw;
    char buf[8];
    uint32_t mods = mods_of(qual);
    uint32_t sp = special_code(code);
    memset(&raw, 0, sizeof raw);
    raw.mods = mods;
    if (sp != 0u) {
        raw.kind = SEL_GUI_RAW_KEY;
        raw.code = sp;
        return queue_raw(b, &raw);
    }
    b->ie.ie_Class = IECLASS_RAWKEY;
    b->ie.ie_Code = code;
    b->ie.ie_EventAddress = addr;
    /* With Control or Alt held the key is a chord on its unmodified letter. */
    b->ie.ie_Qualifier = (UWORD)(qual & ~(IEQUALIFIER_CONTROL | IEQUALIFIER_LALT | IEQUALIFIER_RALT));
    if (MapRawKey(&b->ie, buf, sizeof buf, NULL) != 1 || (unsigned char)buf[0] < 32u || buf[0] == 127) {
        return 0;
    }
    raw.code = (uint32_t)(unsigned char)buf[0];
    if (mods & (TXT_MOD_CTRL | TXT_MOD_ALT)) {
        raw.kind = SEL_GUI_RAW_KEY;
    } else {
        raw.kind = SEL_GUI_RAW_TEXT;
        raw.mods = 0;
    }
    return queue_raw(b, &raw);
}

static int handle(desk_rtg* b, struct IntuiMessage* m) {
    sel_gui_raw raw;
    memset(&raw, 0, sizeof raw);
    raw.x = m->MouseX;
    raw.y = m->MouseY;
    if (m->Class == IDCMP_MOUSEMOVE) {
        raw.kind = SEL_GUI_RAW_MOTION;
        return queue_raw(b, &raw);
    }
    if (m->Class == IDCMP_MOUSEBUTTONS) {
        UWORD c = m->Code;
        int down = c == SELECTDOWN || c == MENUDOWN;
        if (!down && c != SELECTUP && c != MENUUP) {
            if (c == 0x7A || c == 0x7B) { /* unused here: the wheel arrives as a raw key below */
                return 0;
            }
            return 0;
        }
        raw.kind = down ? SEL_GUI_RAW_BUTTON_DOWN : SEL_GUI_RAW_BUTTON_UP;
        raw.button = (c == SELECTDOWN || c == SELECTUP) ? 1u : 3u;
        if (down) {
            int near_prev = raw.x - b->last_x < 8 && b->last_x - raw.x < 8 && raw.y - b->last_y < 8 &&
                            b->last_y - raw.y < 8;
            b->clicks = (b->clicks > 0u && near_prev && DoubleClick(b->last_sec, b->last_micros, m->Seconds, m->Micros))
                            ? 2u
                            : 1u;
            b->last_sec = m->Seconds;
            b->last_micros = m->Micros;
            b->last_x = raw.x;
            b->last_y = raw.y;
        }
        raw.clicks = b->clicks;
        return queue_raw(b, &raw);
    }
    if (m->Class == IDCMP_RAWKEY) {
        if (m->Code & IECODE_UP_PREFIX) {
            return 0;
        }
        if (m->Code == 0x7A || m->Code == 0x7B) {
            raw.kind = SEL_GUI_RAW_WHEEL;
            raw.dy = m->Code == 0x7A ? 65536 : -65536;
            return queue_raw(b, &raw);
        }
        return queue_key(b, m->Code, m->Qualifier, *(APTR*)m->IAddress);
    }
    return 0;
}

int desk_rtg_poll(desk_rtg* b, uint32_t timeout_ms) {
    struct MsgPort* wp = b->window->UserPort;
    int result = DESK_RTG_NONE;
    struct IntuiMessage* m;
    if (timeout_ms > 0u && wp->mp_MsgList.lh_Head->ln_Succ != NULL) {
        timeout_ms = 0;
    }
    if (timeout_ms > 0u) {
        ULONG got, tsig = 1UL << b->tport->mp_SigBit;
        b->treq->tr_node.io_Command = TR_ADDREQUEST;
        b->treq->tr_time.tv_secs = timeout_ms / 1000u;
        b->treq->tr_time.tv_micro = (timeout_ms % 1000u) * 1000u;
        SendIO((struct IORequest*)b->treq);
        got = Wait((1UL << wp->mp_SigBit) | tsig | SIGBREAKF_CTRL_C);
        if (!CheckIO((struct IORequest*)b->treq)) {
            AbortIO((struct IORequest*)b->treq);
        }
        WaitIO((struct IORequest*)b->treq);
        if (got & SIGBREAKF_CTRL_C) {
            fprintf(stdout, "rtg: Ctrl-C\n"); fflush(stdout);
            return DESK_RTG_QUIT;
        }
    } else if (SetSignal(0, 0) & SIGBREAKF_CTRL_C) {
        fprintf(stdout, "rtg: Ctrl-C\n"); fflush(stdout);
        return DESK_RTG_QUIT;
    }
    while ((m = (struct IntuiMessage*)GetMsg(wp)) != NULL) {
        if (handle(b, m)) {
            result = DESK_RTG_ACTIVITY;
        }
        ReplyMsg((struct Message*)m);
    }
    return result;
}
