/* Selecta_AAText (I667d, design section 3.7). MUI 3.8: MUI_CreateCustomClass on Area, same dispatcher pattern as
 * mui_aabtn_class.c. Draw: the super class fills the parent background, then the contents through aa_text in the tone colour
 * (mui_aa_tone_token, held as an exact pen by aa_pen), aligned (mui_aa_align_x) and cut with "..." (mui_aa_text_fit) when the box
 * is narrower than the text. The title tone opens the font at base + 4 px (mui_aa_tone_font_px). Width and height come from the
 * same font as the draw, so the line never clips. No MUIC_Text, no pen escapes. */
#include "mui_aatext_class.h"

#include <stdint.h>
#include <string.h>

#include <clib/alib_protos.h>
#include <clib/muimaster_protos.h>
#include <intuition/intuition.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <proto/utility.h>

#include "mui_aa_draw.h"

extern struct Library* MUIMasterBase;

#define AAT_CAP 160

typedef struct aatext_data {
    char text[AAT_CAP], widest[AAT_CAP];
    int tone, align, ellipsis, fixed, shown, minw, minh, defw;
    aa_ctx aa;
    Object* self;
} aatext_data;

static struct MUI_CustomClass* g_class;
static uint32_t g_made;

/* Width callback of mui_aa_text_fit: the ink width of the first len bytes in the object's font. */
static int aatext_width(void* ctx, const char* s, int len) {
    aatext_data* d = (aatext_data*)ctx;
    char buf[AAT_CAP];
    if (len >= AAT_CAP) {
        len = AAT_CAP - 1;
    }
    memcpy(buf, s, (size_t)len);
    buf[len] = '\0';
    return aa_text_width(&d->aa, d->self, buf);
}

static ULONG aatext_setup(struct IClass* cl, Object* obj, Msg msg) {
    aatext_data* d = INST_DATA(cl, obj);
    int dots;
    if (!DoSuperMethodA(cl, obj, msg)) {
        return FALSE;
    }
    d->self = obj;
    aa_setup(obj, &d->aa, NULL, 0);
    aa_set_font_px(&d->aa, mui_aa_tone_font_px(d->tone, d->aa.font_px));
    aa_pen(&d->aa, d->aa.tok[mui_aa_tone_token(d->tone)]);
    aa_pen(&d->aa, d->aa.tok[SEL_GUI_TOKEN_PANEL]);
    dots = aa_text_width(&d->aa, obj, "...");
    d->defw = aa_text_width(&d->aa, obj, d->widest);
    d->minw = d->ellipsis ? (dots < d->defw ? dots : d->defw) : d->defw;
    d->minh = aa_text_height(&d->aa, obj);
    d->shown = 1;
    return TRUE;
}

static ULONG aatext_cleanup(struct IClass* cl, Object* obj, Msg msg) {
    aatext_data* d = INST_DATA(cl, obj);
    d->shown = 0;
    aa_cleanup(&d->aa);
    return DoSuperMethodA(cl, obj, msg);
}

static ULONG aatext_askminmax(struct IClass* cl, Object* obj, struct MUIP_AskMinMax* msg) {
    aatext_data* d = INST_DATA(cl, obj);
    DoSuperMethodA(cl, obj, (Msg)msg);
    msg->MinMaxInfo->MinWidth += d->minw;
    msg->MinMaxInfo->DefWidth += d->defw;
    {
        int mw = mui_aa_text_maxw(d->fixed, d->defw);
        msg->MinMaxInfo->MaxWidth = mw == MUI_AA_TEXT_UNBOUNDED ? MUI_MAXMAX : msg->MinMaxInfo->MaxWidth + mw;
    }
    msg->MinMaxInfo->MinHeight += d->minh;
    msg->MinMaxInfo->DefHeight += d->minh;
    msg->MinMaxInfo->MaxHeight += d->minh;
    return 0;
}

static ULONG aatext_draw(struct IClass* cl, Object* obj, struct MUIP_Draw* msg) {
    aatext_data* d = INST_DATA(cl, obj);
    char fit[AAT_CAP];
    const char* shown = d->text;
    int w, h, tw, th;
    DoSuperMethodA(cl, obj, (Msg)msg);
    if (!(msg->flags & (MADF_DRAWOBJECT | MADF_DRAWUPDATE)) || d->text[0] == '\0') {
        return 0;
    }
    w = _mwidth(obj);
    h = _mheight(obj);
    tw = aa_text_width(&d->aa, obj, d->text);
    if (d->ellipsis && tw > w) {
        mui_aa_text_fit(d->text, w, aatext_width, d, fit, (int)sizeof fit);
        shown = fit;
        tw = aa_text_width(&d->aa, obj, fit);
    }
    th = aa_text_height(&d->aa, obj);
    aa_text(&d->aa, obj, _rp(obj), _mleft(obj) + mui_aa_align_x(d->align, w, tw), _mtop(obj) + (h - th) / 2, shown, d->aa.tok[mui_aa_tone_token(d->tone)],
            d->aa.tok[SEL_GUI_TOKEN_PANEL]);
    return 0;
}

/* AAT_Contents: copy and redraw only when the text differs from what is shown. */
static void aatext_store(Object* obj, aatext_data* d, const char* text) {
    if (strlen(text) < sizeof d->text && strcmp(d->text, text) == 0) {
        return;
    }
    strncpy(d->text, text, sizeof d->text - 1);
    d->text[sizeof d->text - 1] = '\0';
    if (d->shown) {
        MUI_Redraw(obj, MADF_DRAWOBJECT);
    }
}

static ULONG aatext_set(struct IClass* cl, Object* obj, struct opSet* msg) {
    aatext_data* d = INST_DATA(cl, obj);
    struct TagItem* tag = FindTagItem(AAT_Contents, msg->ops_AttrList);
    if (tag != NULL && tag->ti_Data != 0) {
        aatext_store(obj, d, (const char*)tag->ti_Data);
    }
    return DoSuperMethodA(cl, obj, (Msg)msg);
}

static ULONG __attribute__((used)) aatext_dispatch_c(struct IClass* cl, Object* obj, Msg msg);

extern ULONG aatext_dispatch(void);
__asm__(".text\n.even\n.globl _aatext_dispatch\n_aatext_dispatch:\n"
        "\tmove.l a1,-(sp)\n\tmove.l a2,-(sp)\n\tmove.l a0,-(sp)\n"
        "\tjbsr _aatext_dispatch_c\n\tlea 12(sp),sp\n\trts\n");

static ULONG __attribute__((used)) aatext_dispatch_c(struct IClass* cl, Object* obj, Msg msg) {
    switch (msg->MethodID) {
    case MUIM_Setup:
        return aatext_setup(cl, obj, msg);
    case MUIM_Cleanup:
        return aatext_cleanup(cl, obj, msg);
    case MUIM_AskMinMax:
        return aatext_askminmax(cl, obj, (struct MUIP_AskMinMax*)msg);
    case MUIM_Draw:
        return aatext_draw(cl, obj, (struct MUIP_Draw*)msg);
    case OM_SET:
        return aatext_set(cl, obj, (struct opSet*)msg);
    }
    return DoSuperMethodA(cl, obj, msg);
}

Object* mui_aatext_new(const mui_aatext_spec* sp) {
    Object* o;
    aatext_data* d;
    if (g_class == NULL) {
        g_class = MUI_CreateCustomClass(NULL, (char*)MUIC_Area, NULL, sizeof(aatext_data), (APTR)(uintptr_t)aatext_dispatch);
    }
    if (g_class == NULL) {
        return NULL;
    }
    o = NewObject(g_class->mcc_Class, NULL, MUIA_Frame, sp->frame, sp->background != 0 ? (ULONG)MUIA_Background : (ULONG)TAG_IGNORE, sp->background,
                  MUIA_ShowMe, (ULONG)(sp->hidden ? FALSE : TRUE), TAG_DONE);
    if (o != NULL) {
        d = INST_DATA(g_class->mcc_Class, o);
        strncpy(d->text, sp->contents, sizeof d->text - 1);
        strncpy(d->widest, sp->widest != NULL ? sp->widest : sp->contents, sizeof d->widest - 1);
        d->tone = sp->tone;
        d->align = sp->align;
        d->ellipsis = sp->ellipsis;
        d->fixed = sp->fixed;
        g_made++;
    }
    return o;
}

uint32_t mui_aatext_made(void) {
    return g_made;
}

void mui_aatext_set(Object* obj, const char* contents) {
    SetAttrs(obj, AAT_Contents, (ULONG)contents, TAG_DONE);
}

void mui_aatext_class_free(void) {
    if (g_class != NULL) {
        MUI_DeleteCustomClass(g_class);
        g_class = NULL;
    }
}
