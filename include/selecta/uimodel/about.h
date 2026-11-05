/* About / Credits view model (I706d). Every bundled module that must credit its upstream ships a CREDITS.txt in its own
 * folder under the app plugin folder (<PlugIns>/<module>/CREDITS.txt); the first three non-empty lines are the credit
 * title ("Powered by SunVox ..."), the copyright line and the "Licence: ..." line. The desktop Settings page draws the
 * list; nothing is stored here. Pure C99; the folder scan is host only (the Amiga build finds no credits). */
#ifndef SELECTA_UIMODEL_ABOUT_H
#define SELECTA_UIMODEL_ABOUT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SEL_UIM_ABOUT_MAX 8u
typedef struct sel_uim_credit {
    char title[96];
    char copyright[160];
    char licence[160];
} sel_uim_credit;
typedef struct sel_uim_about {
    uint32_t count;
    sel_uim_credit credit[SEL_UIM_ABOUT_MAX];
} sel_uim_about;

const char* sel_uim_about_heading(void); /* "About Selecta" */
const char* sel_uim_about_credits_heading(void); /* "Credits" */
/* Reads the three credit lines from a CREDITS.txt text; 0 when it holds no title line. */
int sel_uim_credit_parse(const char* text, sel_uim_credit* out);
/* Collects <dir>/<module>/CREDITS.txt of every module folder, sorted by title; a NULL dir or a missing folder gives count 0. */
void sel_uim_about_scan(const char* dir, sel_uim_about* out);
/* The app plugin folder scanned (sel_app_plugin_dir). */
void sel_uim_about_load(sel_uim_about* out);

#ifdef __cplusplus
}
#endif
#endif
