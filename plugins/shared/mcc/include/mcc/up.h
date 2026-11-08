/* UP- MUI class package: the only place the MUI tag base is written (design 2026-11-06-mui-mcc-package.md section 8.2, decision D1).
 * Every attribute and method number of the package is UP_TAG(class index, offset). Interim base until a registered MUI serial
 * exists; the swap is this one line plus a major version bump. */
#ifndef MCC_UP_H
#define MCC_UP_H

#if defined(__amigaos__) || defined(AMIGA)
#include <exec/types.h>
#include <intuition/classusr.h>
#include <utility/hooks.h>
#include <utility/tagitem.h>
typedef ULONG UP_Data; /* ti_Data */
#else
/* Host (tests, no Amiga headers): the names the macros, the tag lists and the hook tags need, with the Amiga values. */
#include <stddef.h>
#include <stdint.h>
typedef uint32_t ULONG;
typedef int32_t LONG;
typedef char* STRPTR;
typedef uintptr_t UP_Data; /* ti_Data: wide enough for a pointer on the host */
typedef ULONG Object;
#define TAG_DONE 0UL
#define TAG_IGNORE 1UL
#define TAG_MORE 2UL
#define TAG_USER (1UL << 31)
struct TagItem {
    ULONG ti_Tag;
    UP_Data ti_Data;
};
struct Hook { /* the three fields the package reads; the Amiga struct has a MinNode in front */
    void* h_Entry;
    void* h_SubEntry;
    void* h_Data;
};
/* MUI tags the class reads (Amiga values; the real header is not on the host). */
#define MUIA_Background 0x8042545bUL
#define MUIA_ControlChar 0x8042120bUL
#define MUIA_CycleChain 0x80421ce7UL
#define MUIA_FixHeight 0x8042a92bUL
#define MUIA_FixWidth 0x8042a3f1UL
#define MUIA_Frame 0x8042ac64UL
#define MUIA_InputMode 0x8042fb04UL
#define MUIA_Numeric_Max 0x8042d78aUL
#define MUIA_Numeric_Min 0x8042e404UL
#define MUIA_Numeric_Value 0x8042ae3aUL
#define MUIA_Selected 0x8042654bUL
#define MUIA_ShortHelp 0x80423bbcUL
#define MUIA_ShowMe 0x80429ba8UL
#define MUIA_String_MaxLen 0x80424984UL
#define MUIA_Weight 0x80421d1fUL
#define MUIV_Frame_None 0UL
#endif
#define UP_D(x) ((UP_Data)(uintptr_t)(x)) /* a value or pointer as ti_Data */

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

/* ---- Attributes (UPA_), methods (UPM_) and enumeration values (UPV_) of every class (design 8.3). The numbers are written here
 * and nowhere else. I/S/G: settable at construction / later / readable. ---- */

/* UP-Text */
#define UPA_Text_Contents UP_TAG(UP_CLS_TEXT, 0x01) /* ISG STRPTR copied into the object; redraws only when it changed */
#define UPA_Text_Widest UP_TAG(UP_CLS_TEXT, 0x02)   /* I STRPTR longest text the line shows (default width); absent = contents */
#define UPA_Text_Tone UP_TAG(UP_CLS_TEXT, 0x03)     /* I UPV_Text_Tone_* */
#define UPA_Text_Align UP_TAG(UP_CLS_TEXT, 0x04)    /* I UPV_Text_Align_* */
#define UPA_Text_Ellipsis UP_TAG(UP_CLS_TEXT, 0x05) /* I BOOL cut with "..." when wider than the box */
#define UPA_Text_Fixed UP_TAG(UP_CLS_TEXT, 0x06)    /* I BOOL never wider than the default width of the widest text */
enum { UPV_Text_Tone_Normal, UPV_Text_Tone_Muted, UPV_Text_Tone_Title, UPV_Text_Tone_Heading };
enum { UPV_Text_Align_Left, UPV_Text_Align_Centre, UPV_Text_Align_Right };

/* UP-Progress and the slider value */
#define UPA_Progress_Value UP_TAG(UP_CLS_PROGRESS, 0x01) /* ISG LONG: filled part */
#define UPA_Progress_Max UP_TAG(UP_CLS_PROGRESS, 0x02)   /* ISG LONG: value that fills the bar */

/* UP-Cycle */
#define UPA_Cycle_Active UP_TAG(UP_CLS_CYCLE, 0x01)  /* ISG LONG: index of the shown entry (notifies) */
#define UPA_Cycle_Entries UP_TAG(UP_CLS_CYCLE, 0x02) /* I char**: NULL terminated, must outlive the object */

/* UP-Tabs */
#define UPA_Tabs_Active UP_TAG(UP_CLS_TABS, 0x01)   /* ISG LONG: index of the active tab (notifies) */
#define UPA_Tabs_Captions UP_TAG(UP_CLS_TABS, 0x02) /* I char**: NULL terminated, at most MUI_AATABS_MAX, must outlive the object */

/* UP-Scroller */
#define UPA_Scroller_First UP_TAG(UP_CLS_SCROLLER, 0x01)   /* ISG LONG: first visible entry (notifies) */
#define UPA_Scroller_Visible UP_TAG(UP_CLS_SCROLLER, 0x02) /* ISG LONG: entries that fit the owner */
#define UPA_Scroller_Entries UP_TAG(UP_CLS_SCROLLER, 0x03) /* ISG LONG: entries in the owner */

/* UP-List */
#define UPA_List_Count UP_TAG(UP_CLS_LIST, 0x01)  /* ISG LONG: rows (notifies) */
#define UPA_List_Active UP_TAG(UP_CLS_LIST, 0x02) /* ISG LONG: selected row, -1 none (notifies) */
#define UPA_List_First UP_TAG(UP_CLS_LIST, 0x03)  /* ISG LONG: first visible row (notifies) */
#define UPA_List_Picked UP_TAG(UP_CLS_LIST, 0x04) /* G LONG: row chosen by click or Return (notifies every time) */
#define UPA_List_Columns UP_TAG(UP_CLS_LIST, 0x05) /* IS struct UP_ListColumns*, copied: the shown columns, may change while live */
#define UPA_List_Playing UP_TAG(UP_CLS_LIST, 0x06) /* IS LONG: row whose first column is drawn in ACCENT, -1 none */
#define UPA_List_RowHook UP_TAG(UP_CLS_LIST, 0x07) /* I struct Hook*: single column text; msg struct UP_ListCell (col 0), returns const char* */
#define UPA_List_CellHook UP_TAG(UP_CLS_LIST, 0x08) /* I struct Hook*: multi column text; row -1 asks the column title; same msg */
#define UPA_List_MaxRows UP_TAG(UP_CLS_LIST, 0x09)  /* I LONG: default height in rows (absent or 0 = 8) */
#define UPA_List_Title UP_TAG(UP_CLS_LIST, 0x0A)    /* I BOOL: a title row above the rows (the cell hook is asked for row -1) */
#define UPA_List_DoublePick UP_TAG(UP_CLS_LIST, 0x0B) /* I BOOL: UPA_List_Picked only on a double click or Return */
#define UPA_List_NoSelect UP_TAG(UP_CLS_LIST, 0x0C) /* I BOOL: no hover, no selection, wheel only */
/* UPM_List_GetRow (I713): the read-back method for tools that drive the window from outside. The caller fills row and text/cap;
 * the method returns TRUE when the row exists and sets: text = the cell texts joined by two spaces (truncated to cap), x/y/w/h =
 * the row's box in window coordinates (all 0 when scrolled out of view or not shown), selected = 1 on the active row. */
#define UPM_List_GetRow UP_TAG(UP_CLS_LIST, 0x10)
#define UP_LIST_COLS 5
typedef struct UP_ListColumns { /* weight > 0 shares the free width by weight, weight 0 is as wide as sample[k] (NULL = empty) */
    int n;
    int weight[UP_LIST_COLS];
    const char* sample[UP_LIST_COLS];
} UP_ListColumns;
typedef struct UP_ListCell { /* the message of both list hooks; the hook's h_Data is the owner's context */
    LONG row, col;
} UP_ListCell;
typedef struct UP_ListGetRow {
    ULONG MethodID; /* UPM_List_GetRow */
    LONG row;
    char* text;
    LONG cap;
    LONG x, y, w, h;
    LONG selected;
} UP_ListGetRow;

/* UP-Button */
#define UPA_Button_Caption UP_TAG(UP_CLS_BUTTON, 0x01)     /* I STRPTR shown text */
#define UPA_Button_Widest UP_TAG(UP_CLS_BUTTON, 0x02)      /* I STRPTR longest caption (sets the minimum width); absent = fit the caption */
#define UPA_Button_Icon UP_TAG(UP_CLS_BUTTON, 0x03)        /* I STRPTR Lucide icon name of the baked set ("play", "skip-forward"); NULL, absent or unknown = none */
#define UPA_Button_AltIcon UP_TAG(UP_CLS_BUTTON, 0x04)     /* I STRPTR icon name swapped in by mui_aabtn_set_alt; NULL, absent or unknown = none */
#define UPA_Button_IconOnly UP_TAG(UP_CLS_BUTTON, 0x05)    /* I BOOL no caption drawn */
#define UPA_Button_Role UP_TAG(UP_CLS_BUTTON, 0x06)        /* I UPV_Button_Role_* */
#define UPA_Button_FixedWidth UP_TAG(UP_CLS_BUTTON, 0x07)  /* I LONG exact width in px, 0 = from the contents */
#define UPA_Button_FixedHeight UP_TAG(UP_CLS_BUTTON, 0x08) /* I LONG exact height in px, 0 = from the contents */
#define UPA_Button_Align UP_TAG(UP_CLS_BUTTON, 0x09)       /* I UPV_Button_Align_* */
#define UPA_Button_IconData UP_TAG(UP_CLS_BUTTON, 0x0A)    /* I struct UP_IconData*: caller icon for a glyph not in the set; used when UPA_Button_Icon names none. The struct and its mask are NOT copied: they must stay valid until the object is disposed */
typedef struct UP_IconData { /* an alpha mask, w x h bytes row by row (0 clear .. 255 full); drawn like a baked icon, scaled to the icon box */
    const unsigned char* alpha;
    LONG w, h;
} UP_IconData;
enum { UPV_Button_Role_Normal, UPV_Button_Role_Accent, UPV_Button_Role_Ghost, UPV_Button_Role_Toggle, UPV_Button_Role_Tab };
enum { UPV_Button_Align_Left, UPV_Button_Align_Centre, UPV_Button_Align_Right };

/* UP-Field */
#define UPM_Field_Redraw UP_TAG(UP_CLS_FIELD, 0x01)     /* method: the window's active object changed, repaint the outline */
#define UPA_Field_Tokens UP_TAG(UP_CLS_FIELD, 0x02)     /* I const uint32_t*: theme token array */
#define UPA_Field_String UP_TAG(UP_CLS_FIELD, 0x03)     /* G Object*: the inner string gadget (for MUIA_String_Acknowledge notifies) */
#define UPA_Field_Placeholder UP_TAG(UP_CLS_FIELD, 0x04) /* I STRPTR hint while empty and not active; must outlive the field */
#define UPA_Field_MaxWidth UP_TAG(UP_CLS_FIELD, 0x05)   /* I LONG cap of the field's MaxWidth in px, 0 = unbounded */
/* the string length limit is MUIA_String_MaxLen */

/* UP-Panel */
#define UPA_Panel_Tokens UP_TAG(UP_CLS_PANEL, 0x01)      /* I const uint32_t*: theme token array */
#define UPA_Panel_Role UP_TAG(UP_CLS_PANEL, 0x02)        /* I UPV_Panel_Role_* */
#define UPA_Panel_FixedWidth UP_TAG(UP_CLS_PANEL, 0x03)  /* I LONG > 0: fixed width and height, no inner padding */
#define UPA_Panel_FixedHeight UP_TAG(UP_CLS_PANEL, 0x04) /* I LONG > 0 with FixedWidth: the fixed height when it differs */
#define UPA_Panel_Self UP_TAG(UP_CLS_PANEL, 0x05)        /* G Object*: the panel itself. A MUI Group passes an unknown OM_GET on to its \
                                                            children, so a Role answer counts only when Self names the asked object */
enum { UPV_Panel_Role_Panel, UPV_Panel_Role_Surface, UPV_Panel_Role_Raised, UPV_Panel_Role_Count };

/* Shared: tooltip (private to the package) */
#define UPM_Shared_Fire UP_TAG(UP_CLS_SHARED, 0x31) /* private method the timer node sends to the owner */
#define UPA_Shared_Side UP_TAG(UP_CLS_SHARED, 0x32) /* I/S: preferred side, MUI_AA_TIP_* */
/* UPA_Shared_Origin
 *   G  struct Library*  The base of the UP-<Name>.mcc library the object's class was loaded from. Every class answers it; the
 *                       answer is non-NULL for an object made by name from MUI:Libs/mui and NULL (not answered, storage left 0)
 *                       in the static Selecta build. Proof that a program uses the shipped library and not linked-in class code
 *                       (gate item 1, section 10). Not settable, not notified. */
#define UPA_Shared_Origin UP_TAG(UP_CLS_SHARED, 0x33)

/* ---- Constructors: the tag list is the whole interface. Defaults a caller does not give (no frame, cycle chain) are the class's. ---- */
#if defined(__amigaos__) || defined(AMIGA)
Object* UP_NewButton(const struct TagItem* tags);
Object* UP_NewText(const struct TagItem* tags);
Object* UP_NewSlider(const struct TagItem* tags);   /* MUIA_Numeric_Min/Max/Value, MUIA_FixWidth */
Object* UP_NewProgress(const struct TagItem* tags);
Object* UP_NewCycle(const struct TagItem* tags);
Object* UP_NewTabs(const struct TagItem* tags);
Object* UP_NewScroller(const struct TagItem* tags);
Object* UP_NewList(const struct TagItem* tags, Object** list_out); /* returns the list+scroller group; *list_out the list */
Object* UP_NewField(const struct TagItem* tags);
Object* UP_NewPanel(const struct TagItem* tags);
Object* UP_NewBalance(const struct TagItem* tags);
#endif

/* Theme token names (I685d2): UPTOK_<id> equals SEL_GUI_TOKEN_<id> for every token of the SEL_GUI_TOKENS list in
 * selecta/uimodel/tokens.h. The names are generated from that list, so a token added or removed there appears or goes here. */
#include <selecta/uimodel/tokens.h>
enum {
#define UPTOK_X(id, name, def, lo, hi) UPTOK_##id = SEL_GUI_TOKEN_##id,
    SEL_GUI_TOKENS(UPTOK_X)
#undef UPTOK_X
    UPTOK_END_
};

#endif

/* Support library functions (UP_ThemeLoad, UP_IconFind, ...). */
#include "up_lib.h"
