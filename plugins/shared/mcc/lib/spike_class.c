/* Dummy Area subclass of the I685b spike: MUIM_AskMinMax gives a fixed size, MUIM_Draw fills the rectangle with one pen. */
#include "spike_class.h"

#include <clib/alib_protos.h>
#include <graphics/rastport.h>
#include <intuition/classusr.h>
#include <intuition/intuition.h>
#include <libraries/mui.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/utility.h>

#define SPIKE_W 80
#define SPIKE_H 40

static ULONG spike_askminmax(struct IClass* cl, Object* obj, struct MUIP_AskMinMax* msg) {
    DoSuperMethodA(cl, obj, (Msg)msg);
    msg->MinMaxInfo->MinWidth += SPIKE_W;
    msg->MinMaxInfo->DefWidth += SPIKE_W;
    msg->MinMaxInfo->MaxWidth += SPIKE_W;
    msg->MinMaxInfo->MinHeight += SPIKE_H;
    msg->MinMaxInfo->DefHeight += SPIKE_H;
    msg->MinMaxInfo->MaxHeight += SPIKE_H;
    return 0;
}

static ULONG spike_draw(struct IClass* cl, Object* obj, struct MUIP_Draw* msg) {
    DoSuperMethodA(cl, obj, (Msg)msg);
    if ((msg->flags & MADF_DRAWOBJECT) != 0) {
        struct RastPort* rp = _rp(obj);
        SetAPen(rp, _dri(obj)->dri_Pens[SHINEPEN]);
        RectFill(rp, _mleft(obj), _mtop(obj), _mright(obj), _mbottom(obj));
    }
    return 0;
}

static ULONG __attribute__((used)) spike_dispatch_c(struct IClass* cl, Object* obj, Msg msg);

__asm__(".text\n.even\n.globl _spike_dispatch\n_spike_dispatch:\n"
        "\tmove.l a1,-(sp)\n\tmove.l a2,-(sp)\n\tmove.l a0,-(sp)\n"
        "\tjbsr _spike_dispatch_c\n\tlea 12(sp),sp\n\trts\n");

static ULONG __attribute__((used)) spike_dispatch_c(struct IClass* cl, Object* obj, Msg msg) {
    switch (msg->MethodID) {
    case MUIM_AskMinMax:
        return spike_askminmax(cl, obj, (struct MUIP_AskMinMax*)msg);
    case MUIM_Draw:
        return spike_draw(cl, obj, (struct MUIP_Draw*)msg);
    }
    return DoSuperMethodA(cl, obj, msg);
}

ULONG spike_instance_size(void) {
    return 0;
}
