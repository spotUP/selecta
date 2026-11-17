/* UADE data set downloader (I774o R12), see uadedata.h. */
#include "uadedata.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_FILES 12
#define NAME_MAX_LEN 40
#define SRC "uadedata"

typedef struct entry {
    char md5[33];
    char name[NAME_MAX_LEN];
} entry;

/* md5 of a whole file as 32 hex characters; 0 when it cannot be read. */
static int file_md5(const char* path, char out[33]) {
    sel_md5 m;
    unsigned char buf[4096];
    size_t n;
    FILE* f = fopen(path, "rb");

    if (f == NULL) {
        return 0;
    }
    sel_md5_init(&m);
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) {
        sel_md5_update(&m, buf, n);
    }
    fclose(f);
    sel_md5_hex(&m, out);
    return 1;
}

static int safe_name(const char* s) {
    size_t i;

    for (i = 0; s[i] != '\0'; i++) {
        char c = s[i];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '_')) {
            return 0;
        }
    }
    return i > 0 && s[0] != '.' && i < NAME_MAX_LEN;
}

/* Parses the manifest; returns the file count, 0 when malformed. bake receives the id (digits only). */
static unsigned parse_manifest(const char* path, char* bake, size_t bakecap, entry* out) {
    char line[160];
    unsigned n = 0;
    int head = 0;
    FILE* f = fopen(path, "r");

    if (f == NULL) {
        return 0;
    }
    while (fgets(line, sizeof line, f) != NULL) {
        char md5[64], name[64];
        unsigned long size;

        line[strcspn(line, "\r\n")] = '\0';
        if (head == 0) {
            if (strcmp(line, "selecta-uadedata 1") != 0) {
                break;
            }
            head = 1;
        } else if (head == 1) {
            size_t i, l = strlen(line);
            int digits = l >= 6 && strncmp(line, "bake ", 5) == 0 && l - 5 < bakecap;

            for (i = 5; digits && i < l; i++) {
                digits = line[i] >= '0' && line[i] <= '9';
            }
            if (!digits) {
                break;
            }
            memcpy(bake, line + 5, l - 4);
            head = 2;
        } else if (sscanf(line, "%63s %lu %63s", md5, &size, name) == 3 && strlen(md5) == 32 && safe_name(name) && n < MAX_FILES) {
            memcpy(out[n].md5, md5, 33);
            snprintf(out[n].name, sizeof out[n].name, "%s", name);
            n++;
        } else {
            n = 0;
            break;
        }
    }
    fclose(f);
    return head == 2 ? n : 0;
}

static int copy_file(const char* from, const char* to) {
    unsigned char buf[8192];
    size_t n;
    int ok = 1;
    FILE* in = fopen(from, "rb");
    FILE* o = in != NULL ? fopen(to, "wb") : NULL;

    if (o == NULL) {
        if (in != NULL) {
            fclose(in);
        }
        return 0;
    }
    while ((n = fread(buf, 1, sizeof buf, in)) > 0) {
        if (fwrite(buf, 1, n, o) != n) {
            ok = 0;
            break;
        }
    }
    fclose(in);
    return fclose(o) == 0 && ok;
}

sel_uadedata_result sel_uadedata_update(sel_cache* c, const char* dir) {
    char mpath[SEL_CACHE_PATH_MAX], bake[24], rel[96], dst[SEL_CACHE_PATH_MAX], tmp[SEL_CACHE_PATH_MAX + 8];
    char have[33];
    entry e[MAX_FILES];
    char cached[MAX_FILES][SEL_CACHE_PATH_MAX];
    unsigned stale[MAX_FILES];
    unsigned n, i, nstale = 0, done;
    sel_cache_result r;

    /* The cache never re-requests a file it holds, so a stale manifest is dropped before each check. */
    if (sel_cache_path(c, SRC, "manifest.txt", mpath, sizeof mpath)) {
        remove(mpath);
    }
    if (sel_cache_fetch(c, SRC, "manifest.txt", NULL, mpath, sizeof mpath) != SEL_CACHE_OK) {
        return SEL_UADEDATA_FAILED;
    }
    n = parse_manifest(mpath, bake, sizeof bake, e);
    if (n == 0) {
        return SEL_UADEDATA_FAILED;
    }
    for (i = 0; i < n; i++) {
        snprintf(dst, sizeof dst, "%s/%s", dir, e[i].name);
        if (!file_md5(dst, have) || strcmp(have, e[i].md5) != 0) {
            stale[nstale++] = i;
        }
    }
    if (nstale == 0) {
        return SEL_UADEDATA_CURRENT;
    }
    for (done = 0; done < nstale; done++) {
        snprintf(rel, sizeof rel, "%s/%s", bake, e[stale[done]].name);
        r = sel_cache_fetch(c, SRC, rel, e[stale[done]].md5, cached[done], sizeof cached[done]);
        if (r != SEL_CACHE_OK) {
            return r == SEL_CACHE_MD5 ? SEL_UADEDATA_MD5 : SEL_UADEDATA_FAILED;
        }
    }
    /* All downloaded and verified: stage next to the targets first so a full disk never breaks the old set. */
    for (done = 0; done < nstale; done++) {
        snprintf(tmp, sizeof tmp, "%s/%s.new", dir, e[stale[done]].name);
        if (!copy_file(cached[done], tmp)) {
            remove(tmp);
            while (done-- > 0) {
                snprintf(tmp, sizeof tmp, "%s/%s.new", dir, e[stale[done]].name);
                remove(tmp);
            }
            return SEL_UADEDATA_FAILED;
        }
    }
    for (done = 0; done < nstale; done++) {
        snprintf(tmp, sizeof tmp, "%s/%s.new", dir, e[stale[done]].name);
        snprintf(dst, sizeof dst, "%s/%s", dir, e[stale[done]].name);
        if (rename(tmp, dst) != 0) {
            remove(dst); /* the Amiga rename does not replace an existing file */
            if (rename(tmp, dst) != 0) {
                return SEL_UADEDATA_FAILED;
            }
        }
        remove(cached[done]);
    }
    return SEL_UADEDATA_UPDATED;
}
