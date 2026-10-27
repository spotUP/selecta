/* The IDCMP to sel_gui_input translation of the Amiga desktop backends; see input.h. */
#include "input.h"
#include "folder_pick.h"

/* The library bases are renamed for the reason given in backend_rtg.c: libnix would open them at program start. */
#define IntuitionBase sel_input_IntuitionBase
#define KeymapBase sel_input_KeymapBase
#define AslBase sel_input_AslBase

#include <selecta/uimodel/keys.h>

#include <devices/inputevent.h>
#include <intuition/intuition.h>
#include <libraries/asl.h>
#include <proto/asl.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/keymap.h>
#include <stdio.h>
#include <string.h>

struct IntuitionBase* IntuitionBase;
struct Library* KeymapBase;
struct Library* AslBase; /* opened for the length of one folder requester only */
int desk_amiga_input_trace;

int desk_amiga_input_open(desk_amiga_input* in) {
    memset(in, 0, sizeof *in);
    IntuitionBase = (struct IntuitionBase*)OpenLibrary((CONST_STRPTR) "intuition.library", 39);
    KeymapBase = OpenLibrary((CONST_STRPTR) "keymap.library", 37);
    return IntuitionBase != NULL && KeymapBase != NULL;
}

void desk_amiga_input_close(desk_amiga_input* in) {
    (void)in;
    if (KeymapBase != NULL) CloseLibrary(KeymapBase);
    if (IntuitionBase != NULL) CloseLibrary((struct Library*)IntuitionBase);
    KeymapBase = NULL;
    IntuitionBase = NULL;
}

int desk_amiga_input_next(desk_amiga_input* b, sel_gui_input* ev) {
    if (b->q_count == 0u) return 0;
    *ev = b->queue[b->q_head];
    b->q_head = (b->q_head + 1u) % DESK_AMIGA_INPUT_QUEUE;
    b->q_count--;
    return 1;
}

static int queue_raw(desk_amiga_input* b, const sel_gui_raw* raw) {
    sel_gui_input in;
    if (!sel_gui_input_map(&b->map, raw, &in) || b->q_count >= DESK_AMIGA_INPUT_QUEUE) {
        return 0;
    }
    b->queue[(b->q_head + b->q_count) % DESK_AMIGA_INPUT_QUEUE] = in;
    b->q_count++;
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

static int queue_key(desk_amiga_input* b, UWORD code, UWORD qual, APTR addr) {
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

int desk_amiga_input_handle(desk_amiga_input* b, struct IntuiMessage* m) {
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
        if (desk_amiga_input_trace) {
            static uint32_t downs, ups;
            ULONG now_s, now_m;
            CurrentTime(&now_s, &now_m);
            if (down) downs++; else ups++;
            fprintf(stderr, "button %s x=%d y=%d stamped=%lu.%06lu taken=%lu.%06lu downs=%u ups=%u\n", down ? "down" : "up",
                    (int)raw.x, (int)raw.y, (unsigned long)m->Seconds, (unsigned long)m->Micros, (unsigned long)now_s,
                    (unsigned long)now_m, (unsigned)downs, (unsigned)ups);
        }
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


/* The folder dialog (I531): an asl.library file requester in drawers-only mode on the Selecta screen. It blocks until
 * the user answers (the screen keeps its last frame). The chosen drawer becomes the same SEL_GUI_IN_DROP the SDL folder
 * dialog queues; desktop_ui.c answers it with LIBRARY_SCAN. Cancel queues nothing. Returns 1 when the requester ran
 * (whatever the answer), 0 when asl.library or the requester could not be opened. */
int desk_amiga_input_pick_folder(desk_amiga_input* b, struct Screen* screen) {
    struct FileRequester* fr;
    int ran = 0;
    AslBase = OpenLibrary((CONST_STRPTR) "asl.library", 37);
    if (AslBase == NULL) {
        return 0;
    }
    fr = (struct FileRequester*)AllocAslRequestTags(ASL_FileRequest, ASLFR_Screen, (ULONG)screen, ASLFR_TitleText,
                                                    (ULONG) "Add folder", ASLFR_DrawersOnly, TRUE,
                                                    ASLFR_RejectIcons, TRUE, TAG_DONE);
    if (fr != NULL) {
        ran = 1;
        if (AslRequest(fr, NULL)) {
            sel_gui_input ev;
            if (desk_amiga_folder_event((const char*)fr->fr_Drawer, (const char*)fr->fr_File, &ev) &&
                b->q_count < DESK_AMIGA_INPUT_QUEUE) {
                b->queue[(b->q_head + b->q_count) % DESK_AMIGA_INPUT_QUEUE] = ev;
                b->q_count++;
            }
        }
        FreeAslRequest(fr);
    }
    CloseLibrary(AslBase);
    AslBase = NULL;
    return ran;
}
