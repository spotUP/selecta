/* About / Credits view model: see include/selecta/uimodel/about.h (I706d). */
#include <selecta/engine.h>
#include <selecta/uimodel/about.h>

#include <stdio.h>
#include <string.h>

#if !defined(__amigaos__) && !defined(AMIGA)
#include <dirent.h>
#endif

const char* sel_uim_about_heading(void) { return "About Selecta"; }

const char* sel_uim_about_credits_heading(void) { return "Credits"; }

static void put(char* dst, size_t cap, const char* src, size_t len) {
    while (len > 0u && (src[len - 1u] == '\r' || src[len - 1u] == ' ')) {
        len--;
    }
    if (len >= cap) {
        len = cap - 1u;
    }
    memcpy(dst, src, len);
    dst[len] = '\0';
}

int sel_uim_credit_parse(const char* text, sel_uim_credit* out) {
    char* const dst[3] = {out->title, out->copyright, out->licence};
    const size_t cap[3] = {sizeof out->title, sizeof out->copyright, sizeof out->licence};
    unsigned got = 0;
    out->title[0] = out->copyright[0] = out->licence[0] = '\0';
    while (text != NULL && *text != '\0' && got < 3u) {
        const char* nl = strchr(text, '\n');
        size_t len = nl != NULL ? (size_t)(nl - text) : strlen(text);
        if (len > 0u && !(len == 1u && text[0] == '\r')) {
            put(dst[got], cap[got], text, len);
            got++;
        } else if (got > 0u) {
            break; /* the first paragraph ends at the first blank line */
        }
        text += len + (nl != NULL ? 1u : 0u);
    }
    return out->title[0] != '\0';
}

void sel_uim_about_scan(const char* dir, sel_uim_about* out) {
    out->count = 0;
#if !defined(__amigaos__) && !defined(AMIGA)
    DIR* dp = dir != NULL ? opendir(dir) : NULL;
    struct dirent* de;
    while (dp != NULL && (de = readdir(dp)) != NULL && out->count < SEL_UIM_ABOUT_MAX) {
        char path[1100];
        char text[1024];
        size_t n;
        FILE* f;
        if (de->d_name[0] == '.' || snprintf(path, sizeof path, "%s/%s/CREDITS.txt", dir, de->d_name) >= (int)sizeof path) {
            continue;
        }
        f = fopen(path, "rb");
        if (f == NULL) {
            continue;
        }
        n = fread(text, 1, sizeof text - 1u, f);
        fclose(f);
        text[n] = '\0';
        if (sel_uim_credit_parse(text, &out->credit[out->count])) {
            out->count++;
        }
    }
    if (dp != NULL) {
        uint32_t i, j;
        closedir(dp);
        for (i = 1; i < out->count; i++) { /* by title: the same list on every file system */
            sel_uim_credit c = out->credit[i];
            for (j = i; j > 0u && strcmp(out->credit[j - 1u].title, c.title) > 0; j--) {
                out->credit[j] = out->credit[j - 1u];
            }
            out->credit[j] = c;
        }
    }
#else
    (void)dir;
#endif
}

void sel_uim_about_load(sel_uim_about* out) {
    char dir[1024];
    sel_uim_about_scan(sel_app_plugin_dir(dir, sizeof dir) ? dir : NULL, out);
}
