/* I685f (design 2026-11-06-mui-mcc-package.md section 8.3): Selecta's spec structs map to the tag lists of the UP- classes.
 * Each builder (map/mui_aatagmap.c, the function the Amiga wrapper uses) gets a spec with distinct values; every spec field must
 * appear exactly once as its mapped tag with the right value, no id tag may exist, and the list ends with TAG_DONE. Source checks:
 * no class reads a spec id, and every UP_TAG( definition is in include/mcc/up.h only. Host only. */
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <selecta/gui/gui_icons.h>

#include "mui_aatagmap.h"
#include "mui_iconmap.h"
#include "mui_icons.h"

static int failures;
#define CHECK(cond, ...)                                \
    do {                                                \
        if (!(cond)) {                                  \
            failures++;                                 \
            printf("FAIL %s:%d: ", __FILE__, __LINE__); \
            printf(__VA_ARGS__);                        \
            printf("\n");                               \
        }                                               \
    } while (0)

static int count_tag(const struct TagItem* t, ULONG tag) {
    int n = 0;
    for (; t->ti_Tag != TAG_DONE; t++) {
        n += t->ti_Tag == tag;
    }
    return n;
}

static int has_tag(const struct TagItem* t, ULONG tag, UP_Data want, const char* what) {
    int ok = count_tag(t, tag) == 1;
    CHECK(ok, "%s: tag %08lx appears %d times, not once", what, (unsigned long)tag, count_tag(t, tag));
    for (; ok && t->ti_Tag != TAG_DONE; t++) {
        if (t->ti_Tag == tag) {
            CHECK(t->ti_Data == want, "%s: tag %08lx holds %lx, wants %lx", what, (unsigned long)tag, (unsigned long)t->ti_Data, (unsigned long)want);
        }
    }
    return ok;
}

/* No tag of the list is the ReturnID or any private id: a list holds only the tags the case names, then TAG_DONE. */
static void shape(const struct TagItem* t, int n, int want_tags, const char* what) {
    CHECK(n == want_tags + 1, "%s: %d items, wants %d tags and TAG_DONE", what, n, want_tags);
    CHECK(n > 0 && t[n - 1].ti_Tag == TAG_DONE, "%s: no TAG_DONE at the end", what);
}

static void test_button(void) {
    mui_aabtn_spec sp;
    struct TagItem t[MUI_AATAGMAP_MAX];
    int n;
    memset(&sp, 0, sizeof sp);
    sp.caption = "Cap";
    sp.widest = "Widest caption";
    sp.help = "Help";
    sp.icon = SEL_GUI_ICON_MINUS;
    sp.alt_icon = SEL_GUI_ICON_PLUS;
    sp.icon_only = 1;
    sp.role = UPV_Button_Role_Toggle;
    sp.id = 0x7701;
    sp.selected = 1;
    sp.weight = 37;
    sp.fixed_w = 41;
    sp.fixed_h = 43;
    sp.align = UPV_Button_Align_Right;
    n = mui_aabtn_tags(&sp, t, MUI_AATAGMAP_MAX);
    shape(t, n, 12, "button");
    has_tag(t, UPA_Button_Caption, UP_D(sp.caption), "button");
    has_tag(t, UPA_Button_Widest, UP_D(sp.widest), "button");
    has_tag(t, MUIA_ShortHelp, UP_D(sp.help), "button");
    CHECK(has_tag(t, UPA_Button_Icon, UP_D(mui_icon_name(SEL_GUI_ICON_MINUS)), "button") && mui_icon_name(SEL_GUI_ICON_MINUS) != NULL &&
              !strcmp(mui_icon_name(SEL_GUI_ICON_MINUS), "minus"),
          "button: UPA_Button_Icon carries the Lucide name \"minus\"");
    CHECK(has_tag(t, UPA_Button_AltIcon, UP_D(mui_icon_name(SEL_GUI_ICON_PLUS)), "button") && mui_icon_name(SEL_GUI_ICON_PLUS) != NULL &&
              !strcmp(mui_icon_name(SEL_GUI_ICON_PLUS), "plus"),
          "button: UPA_Button_AltIcon carries the Lucide name \"plus\"");
    has_tag(t, UPA_Button_IconOnly, 1, "button");
    has_tag(t, UPA_Button_Role, UPV_Button_Role_Toggle, "button");
    has_tag(t, MUIA_Selected, 1, "button");
    has_tag(t, MUIA_Weight, 37, "button");
    has_tag(t, UPA_Button_FixedWidth, 41, "button");
    has_tag(t, UPA_Button_FixedHeight, 43, "button");
    has_tag(t, UPA_Button_Align, UPV_Button_Align_Right, "button");
    sp.help = NULL;
    n = mui_aabtn_tags(&sp, t, MUI_AATAGMAP_MAX);
    has_tag(t, MUIA_ShortHelp, UP_D(sp.caption), "button without help falls back to the caption");
    CHECK(mui_aabtn_tags(&sp, t, 5) == 0, "button: a short cap must fail, not overrun");
}

/* I685g: every sel_gui_icon (counted from the enum: SEL_GUI_ICON_COUNT) maps to a Lucide name in the Selecta wrapper, and that
 * name resolves in the package to the body the enum index resolved to before, at every baked size. Unknown names resolve to nothing. */
static void test_icons(void) {
    int i;
    uint32_t z;
    CHECK(SEL_GUI_ICON_COUNT > 0 && (uint32_t)SEL_GUI_ICON_COUNT == mui_icon_body_count, "enum has %d icons, the bake %u bodies", (int)SEL_GUI_ICON_COUNT, (unsigned)mui_icon_body_count);
    for (i = 0; i < SEL_GUI_ICON_COUNT; i++) {
        const char* name = mui_icon_name(i);
        CHECK(name != NULL && name[0] != 0, "icon %d has no name", i);
        if (name == NULL) {
            continue;
        }
        CHECK(mui_icon_index(name) == i, "icon %d name '%s' resolves to index %d", i, name, mui_icon_index(name));
        CHECK(mui_icon_names[i] != NULL && !strcmp(mui_icon_names[i], name), "icon %d: wrapper name '%s' differs from the baked name", i, name);
        for (z = 0; z < mui_icon_size_count; z++) {
            const uint8_t* want = mui_icon_body_wh(i, (int)mui_icon_sizes[z], (int)mui_icon_heights[z]);
            const uint8_t* got = mui_icon_by_name(name, (int)mui_icon_sizes[z], (int)mui_icon_heights[z]);
            CHECK(want != NULL && got == want, "icon '%s' size %ux%u: name body %p, enum body %p", name, (unsigned)mui_icon_sizes[z], (unsigned)mui_icon_heights[z], (const void*)got, (const void*)want);
        }
    }
    CHECK(mui_icon_name(-1) == NULL && mui_icon_name(SEL_GUI_ICON_COUNT) == NULL, "out of range enum values have no name");
    CHECK(mui_icon_index("no-such-icon") == -1 && mui_icon_index("") == -1 && mui_icon_index(NULL) == -1, "unknown names resolve to index -1");
    CHECK(mui_icon_by_name("no-such-icon", 16, 16) == NULL && mui_icon_by_name(NULL, 16, 16) == NULL, "unknown names resolve to no body");
    CHECK(mui_icon_by_name("play", 7, 7) == NULL, "a size without a baked set has no body");
}

static void test_text(void) {
    mui_aatext_spec sp;
    struct TagItem t[MUI_AATAGMAP_MAX];
    int n;
    memset(&sp, 0, sizeof sp);
    sp.contents = "Now";
    sp.widest = "Widest";
    sp.tone = UPV_Text_Tone_Title;
    sp.align = UPV_Text_Align_Right;
    sp.ellipsis = 1;
    sp.frame = 5;
    sp.background = 0x1234;
    sp.fixed = 1;
    sp.hidden = 1;
    n = mui_aatext_tags(&sp, t, MUI_AATAGMAP_MAX);
    shape(t, n, 9, "text");
    has_tag(t, UPA_Text_Contents, UP_D(sp.contents), "text");
    has_tag(t, UPA_Text_Widest, UP_D(sp.widest), "text");
    has_tag(t, UPA_Text_Tone, UPV_Text_Tone_Title, "text");
    has_tag(t, UPA_Text_Align, UPV_Text_Align_Right, "text");
    has_tag(t, UPA_Text_Ellipsis, 1, "text");
    has_tag(t, UPA_Text_Fixed, 1, "text");
    has_tag(t, MUIA_Frame, 5, "text");
    has_tag(t, MUIA_Background, 0x1234, "text");
    has_tag(t, MUIA_ShowMe, 0, "text hidden is ShowMe FALSE");
    sp.background = 0;
    sp.hidden = 0;
    n = mui_aatext_tags(&sp, t, MUI_AATAGMAP_MAX);
    CHECK(count_tag(t, MUIA_Background) == 0, "text: no background tag when the spec has none");
    has_tag(t, MUIA_ShowMe, 1, "text visible is ShowMe TRUE");
}

static void test_slider(void) {
    mui_aaslider_spec sp;
    struct TagItem t[MUI_AATAGMAP_MAX];
    int n;
    memset(&sp, 0, sizeof sp);
    sp.lo = -3;
    sp.hi = 77;
    sp.value = 21;
    sp.fix_w = 99;
    sp.id = 0x7702;
    n = mui_aaslider_tags(&sp, t, MUI_AATAGMAP_MAX);
    shape(t, n, 4, "slider");
    has_tag(t, MUIA_Numeric_Min, (UP_Data)(long)-3, "slider");
    has_tag(t, MUIA_Numeric_Max, 77, "slider");
    has_tag(t, MUIA_Numeric_Value, 21, "slider");
    has_tag(t, MUIA_FixWidth, 99, "slider");
    sp.fix_w = 0;
    n = mui_aaslider_tags(&sp, t, MUI_AATAGMAP_MAX);
    shape(t, n, 3, "slider without width");
}

static void test_cycle_tabs_field_panel(void) {
    static const char* const ent[] = {"a", "b", NULL};
    static const uint32_t tok[4] = {1, 2, 3, 4};
    struct TagItem t[MUI_AATAGMAP_MAX];
    int n;
    mui_aacycle_spec cy = {ent, 1, 0x7703};
    mui_aatabs_spec tb = {ent, 1};
    mui_aafield_spec fl = {tok, 60, 300, "Search", 0x7704};
    mui_aapanel_spec pn = {tok, UPV_Panel_Role_Raised, 64, 80};
    n = mui_aacycle_tags(&cy, t, MUI_AATAGMAP_MAX);
    shape(t, n, 2, "cycle");
    has_tag(t, UPA_Cycle_Entries, UP_D(ent), "cycle");
    has_tag(t, UPA_Cycle_Active, 1, "cycle");
    n = mui_aatabs_tags(&tb, t, MUI_AATAGMAP_MAX);
    shape(t, n, 2, "tabs");
    has_tag(t, UPA_Tabs_Captions, UP_D(ent), "tabs");
    has_tag(t, UPA_Tabs_Active, 1, "tabs");
    n = mui_aafield_tags(&fl, t, MUI_AATAGMAP_MAX);
    shape(t, n, 4, "field");
    has_tag(t, UPA_Field_Tokens, UP_D(tok), "field");
    has_tag(t, MUIA_String_MaxLen, 60, "field");
    has_tag(t, UPA_Field_MaxWidth, 300, "field");
    has_tag(t, UPA_Field_Placeholder, UP_D(fl.placeholder), "field");
    n = mui_aapanel_tags(&pn, t, MUI_AATAGMAP_MAX);
    shape(t, n, 4, "panel");
    has_tag(t, UPA_Panel_Tokens, UP_D(tok), "panel");
    has_tag(t, UPA_Panel_Role, UPV_Panel_Role_Raised, "panel");
    has_tag(t, UPA_Panel_FixedWidth, 64, "panel");
    has_tag(t, UPA_Panel_FixedHeight, 80, "panel");
}

static const char* g_row_seen;
static const char* row_fn(void* ctx, long row) {
    g_row_seen = (const char*)ctx;
    return row == 3 ? "row three" : "other";
}
static const char* cell_fn(void* ctx, long row, int col) {
    g_row_seen = (const char*)ctx;
    return row == -1 ? "title" : (col == 2 ? "col two" : "x");
}

typedef UP_Data (*hook_sub)(struct Hook*, void*, UP_ListCell*);

static void test_list(void) {
    static char ctx[] = "owner";
    static mui_aalist_cols cols = {2, {3, 0}, {NULL, "sample"}};
    mui_aalist_spec sp;
    mui_aalist_hooks hk;
    struct TagItem t[MUI_AATAGMAP_MAX];
    UP_ListCell cell;
    int n;
    memset(&sp, 0, sizeof sp);
    sp.count = 12;
    sp.active = 4;
    sp.row = row_fn;
    sp.ctx = ctx;
    sp.max_rows = 9;
    sp.title = 1;
    sp.double_pick = 1;
    sp.no_select = 1;
    n = mui_aalist_tags(&sp, &hk, t, MUI_AATAGMAP_MAX);
    shape(t, n, 7, "list row hook");
    has_tag(t, UPA_List_Count, 12, "list");
    has_tag(t, UPA_List_Active, 4, "list");
    has_tag(t, UPA_List_RowHook, UP_D(&hk.row.hook), "list");
    CHECK(count_tag(t, UPA_List_CellHook) == 0, "list: no cell hook when the spec has none");
    has_tag(t, UPA_List_MaxRows, 9, "list");
    has_tag(t, UPA_List_Title, 1, "list");
    has_tag(t, UPA_List_DoublePick, 1, "list");
    has_tag(t, UPA_List_NoSelect, 1, "list");
    CHECK(hk.row.hook.h_Data == (void*)ctx, "list: the spec's ctx is the hook's h_Data");
    cell.row = 3;
    cell.col = 0;
    g_row_seen = NULL;
    {
        const char* s = (const char*)(uintptr_t)((hook_sub)(uintptr_t)hk.row.hook.h_SubEntry)(&hk.row.hook, NULL, &cell);
        CHECK(g_row_seen == ctx, "list: the row hook calls the owner with its ctx");
        CHECK(s != NULL && !strcmp(s, "row three"), "list: the row hook returns the owner's text");
    }
    sp.row = NULL;
    sp.cell = cell_fn;
    sp.cols = &cols;
    n = mui_aalist_tags(&sp, &hk, t, MUI_AATAGMAP_MAX);
    shape(t, n, 8, "list cell hook");
    has_tag(t, UPA_List_CellHook, UP_D(&hk.cell.hook), "list");
    has_tag(t, UPA_List_Columns, UP_D(&cols), "list");
    CHECK(count_tag(t, UPA_List_RowHook) == 0, "list: the cell hook replaces the row hook");
    cell.row = -1;
    cell.col = 2;
    {
        const char* s = (const char*)(uintptr_t)((hook_sub)(uintptr_t)hk.cell.hook.h_SubEntry)(&hk.cell.hook, NULL, &cell);
        CHECK(s != NULL && !strcmp(s, "title"), "list: the cell hook passes row -1 for the title");
    }
}

static char* slurp(const char* path) {
    FILE* f = fopen(path, "rb");
    long n;
    char* b;
    if (f == NULL) {
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    b = (char*)malloc((size_t)n + 1);
    if (b != NULL) {
        n = (long)fread(b, 1, (size_t)n, f);
        b[n] = '\0';
    }
    fclose(f);
    return b;
}

static int ident(char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

/* A spec id is read as ->id or .id, not followed by an identifier character. */
static int reads_id(const char* s) {
    const char* p;
    for (p = s; (p = strstr(p, "id")) != NULL; p += 2) {
        int dot = p > s && p[-1] == '.';
        int arrow = p > s + 1 && p[-1] == '>' && p[-2] == '-';
        if ((dot || arrow) && !ident(p[2])) {
            return 1;
        }
    }
    return 0;
}

static int ends_with(const char* n, const char* suf) {
    size_t a = strlen(n), b = strlen(suf);
    return a >= b && !strcmp(n + a - b, suf);
}

static void scan_dir(const char* dir, int* class_files, int* up_tag_files) {
    DIR* d = opendir(dir);
    struct dirent* e;
    if (d == NULL) {
        CHECK(0, "cannot open %s", dir);
        return;
    }
    while ((e = readdir(d)) != NULL) {
        char path[1024];
        char* src;
        if (!ends_with(e->d_name, ".c") && !ends_with(e->d_name, ".h")) {
            continue;
        }
        snprintf(path, sizeof path, "%s/%s", dir, e->d_name);
        src = slurp(path);
        if (src == NULL) {
            continue;
        }
        if (strstr(src, "UP_TAG(") != NULL && strcmp(e->d_name, "up.h") != 0) {
            (*up_tag_files)++;
            CHECK(0, "UP_TAG( outside include/mcc/up.h: %s", path);
        }
        if (!strncmp(e->d_name, "mui_aa", 6) && ends_with(e->d_name, "_class.c")) {
            (*class_files)++;
            CHECK(!reads_id(src), "%s reads a spec id (the wrapper owns the ReturnID)", path);
        }
        free(src);
    }
    closedir(d);
}

static void test_source(void) {
    int classes = 0, bad = 0;
    char p[1024];
    snprintf(p, sizeof p, "%s/src", MCC_DIR);
    scan_dir(p, &classes, &bad);
    snprintf(p, sizeof p, "%s/src/class", MCC_DIR);
    scan_dir(p, &classes, &bad);
    snprintf(p, sizeof p, "%s/src/map", MCC_DIR);
    scan_dir(p, &classes, &bad);
    snprintf(p, sizeof p, "%s/include/mcc", MCC_DIR);
    scan_dir(p, &classes, &bad);
    CHECK(classes == 10, "ten class sources scanned, saw %d", classes);
}

int main(int argc, char** argv) {
    const char* t = argc > 1 ? argv[1] : "all";
    int all = !strcmp(t, "all");
    if (all || !strcmp(t, "tags")) {
        test_button();
        test_icons();
        test_text();
        test_slider();
        test_cycle_tabs_field_panel();
        test_list();
    }
    if (all || !strcmp(t, "source")) {
        test_source();
    }
    if (failures == 0) {
        printf("PASS mui_tagmap_test %s\n", t);
    }
    return failures != 0;
}
