/* I774n R8: song info sections Credits, Other Sources, Found In. See include/selecta/uimodel/siinfo.h. */
#include <selecta/uimodel/siinfo.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    sel_uim_si_line* out;
    uint32_t n, max;
} si_sink;

static sel_uim_si_line* si_add(si_sink* s, sel_uim_si_kind kind, const char* key) {
    sel_uim_si_line* l;
    if (s->n >= s->max) return NULL;
    l = &s->out[s->n++];
    l->kind = kind;
    snprintf(l->key, sizeof l->key, "%s", key);
    l->val[0] = '\0';
    return l;
}

/* Appends `part` to a ", " joined list when it is not empty. */
static void si_join(char* dst, size_t cap, const char* part) {
    size_t n = strlen(dst);
    if (part[0] == '\0' || n >= cap) return;
    snprintf(dst + n, cap - n, "%s%s", n ? ", " : "", part);
}

/* Upstream's "~"-joined author list shown as "A, B". */
static void si_authors(char* dst, size_t cap, const char* a) {
    size_t n = 0;
    for (; *a != '\0' && n + 3 < cap; a++) {
        if (*a == '~') {
            dst[n++] = ',';
            dst[n++] = ' ';
        } else {
            dst[n++] = *a;
        }
    }
    dst[n] = '\0';
}

static void si_num(char* dst, size_t cap, uint32_t v) {
    if (v) snprintf(dst, cap, "%u", (unsigned)v);
}

uint32_t sel_uim_si_lines(const sel_uim_si_input* in, sel_uim_si_line* out, uint32_t max) {
    static const char* const kind_label[] = {"Sites", "Collections", "Groups", "Bulletin Boards", "CD-ROMs"};
    si_sink s = {out, 0u, max};
    uint32_t i, k;
    sel_uim_si_line* l;

    if (in->have_info) {
        const sel_uim_si_credit* c = &in->info;
        char authors[sizeof c->authors], year[16] = "", chan[16] = "";
        const char* lab[] = {"Authors", "Group / Publisher", "Release", "Year", "Format", "Channels"};
        const char* val[6];
        int head = 0;
        si_authors(authors, sizeof authors, c->authors);
        si_num(year, sizeof year, c->year);
        si_num(chan, sizeof chan, c->channels);
        val[0] = authors;
        val[1] = c->publisher;
        val[2] = c->album;
        val[3] = year;
        val[4] = c->format;
        val[5] = chan;
        for (i = 0; i < 6u; i++) {
            if (val[i][0] == '\0') continue;
            if (!head) head = si_add(&s, SEL_UIM_SI_HEADING, "Credits") != NULL;
            if ((l = si_add(&s, SEL_UIM_SI_CREDIT, lab[i])) != NULL) snprintf(l->val, sizeof l->val, "%s", val[i]);
        }
    }
    for (i = 0; i < in->ncredits && i < SEL_UIM_SI_CREDIT_MAX; i++) {
        const sel_uim_si_credit* c = &in->credits[i];
        char authors[sizeof c->authors], year[16] = "";
        if (i == 0u) si_add(&s, SEL_UIM_SI_HEADING, "Other Sources");
        if ((l = si_add(&s, SEL_UIM_SI_OTHER, c->source)) == NULL) break;
        si_authors(authors, sizeof authors, c->authors);
        si_num(year, sizeof year, c->year);
        si_join(l->val, sizeof l->val, authors);
        si_join(l->val, sizeof l->val, c->publisher);
        si_join(l->val, sizeof l->val, c->album);
        si_join(l->val, sizeof l->val, year);
    }
    if (in->nprov > 0u) si_add(&s, SEL_UIM_SI_HEADING, "Found In");
    for (k = 0; k < 5u; k++) {
        uint32_t count = 0;
        for (i = 0; i < in->nprov && i < SEL_UIM_SI_PROV_MAX; i++) count += in->prov[i].kind == (int)k;
        if (count == 0u) continue;
        if ((l = si_add(&s, SEL_UIM_SI_FOUND_KIND, kind_label[k])) != NULL) snprintf(l->val, sizeof l->val, "%u", (unsigned)count);
        for (i = 0; i < in->nprov && i < SEL_UIM_SI_PROV_MAX; i++) {
            if (in->prov[i].kind == (int)k && (l = si_add(&s, SEL_UIM_SI_FOUND_NAME, "")) != NULL) snprintf(l->val, sizeof l->val, "%s", in->prov[i].display);
        }
    }
    return s.n;
}

uint32_t sel_uim_si_panel_put(const sel_uim_si_input* in, char* out, uint32_t cap, uint32_t* used) {
    sel_uim_si_line ln[SEL_UIM_SI_LINES_MAX];
    uint32_t n = sel_uim_si_lines(in, ln, SEL_UIM_SI_LINES_MAX), i, added = 0;

    for (i = 0; i < n; i++) {
        char label[40];
        size_t ll, lv;
        const char* tag = ln[i].kind == SEL_UIM_SI_HEADING ? "\x01" : ln[i].kind == SEL_UIM_SI_FOUND_KIND ? "\x02" : ln[i].kind == SEL_UIM_SI_FOUND_NAME ? "\x03" : "";
        snprintf(label, sizeof label, "%s%s", tag, ln[i].key);
        ll = strlen(label) + 1u;
        lv = strlen(ln[i].val) + 1u;
        if (*used + ll + lv + 1u > cap) break;
        memcpy(out + *used, label, ll);
        memcpy(out + *used + ll, ln[i].val, lv);
        *used += (uint32_t)(ll + lv);
        out[*used] = '\0';
        added++;
    }
    return added;
}
