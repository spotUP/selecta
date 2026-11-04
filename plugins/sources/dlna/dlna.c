/* The `dlna` source descriptor (row I894a3; network libs design, DLNA section): a UPnP / DLNA MediaServer (Synology
 * Media Server, Minidlna, Plex DLNA, ...) as a library source. Discovery (SSDP M-SEARCH, device description,
 * ContentDirectory controlURL) and the paged Browse SOAP live in plugins/shared/upnp; devices without a
 * ContentDirectory service (a Philips Hue bridge also answers the MediaServer search) never reach this plugin
 * because the description parser only accepts a device that has one.
 *
 * A "sync" takes one argument:
 *   - a device description URL (http://host:port/desc/device.xml): that server is scanned;
 *   - "discover": SSDP finds the servers on the LAN (each is named on stderr) and every one is scanned.
 * SELECTA_DLNA_SSDP="ip:port" replaces the multicast address (tests point it at a unicast fixture).
 * The scan walks by BrowseDirectChildren, depth first, paging each container by StartingIndex until TotalMatches.
 * Synology and Minidlna list every file under several virtual views (Artist, Album, Genre, Folder), so a walk of the
 * whole tree costs a request per virtual container and meets each file about seven times (I894a4: 23045 requests, the
 * row cap). The scan therefore looks for the server's folder view (a container titled "Folder", "Folders" or
 * "Browse Folders" among the children of the root or of one level below it, e.g. root > Music > Folder) and walks
 * only that; with no such container it walks everything from "0". A res URL already stored in this sync is skipped.
 * Every audio item with a res URL becomes one song, key
 * `dlna:<artist>/<title>/<res url>`; play_url returns the res URL and the engine streams it over HTTP (seekable by
 * Range when the server allows it). Browse is read-only; nothing is written to the server, the library or the cache. */
#include <selecta/catalogue.h>
#include <selecta/host.h>
#include <selecta/net.h>
#include <selecta/plugin.h>
#include <selecta/source.h>
#include "source/descriptor.h"
#include "source/sync_stat.h"
#include "upnp/upnp.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define USER_AGENT "Selecta-DLNA/1 (+https://github.com/spotUP/Selecta)"
#define DESC_CAP (64u * 1024u)
#define MAX_DEPTH 8u
#define MAX_SONGS 200000u
#define MAX_SERVERS 8u
#define URL_BYTES 1100u
#define SSDP_WAIT_MS 2500u

static const sel_tag g_tags[] = {
    {SEL_TAG_END, 0},
};

#define LINE_BYTES 2700u
#define MAX_SUBS 512u
#define MAX_CHANGED 256u
#define STATE_MAGIC "selecta-dlna-state 1"
#define DLNA_HOME "http://www.upnp.org/"

typedef struct hset {
    uint64_t* t; /* open addressing set of 64 bit hashes (0 = empty slot) */
    size_t cap, n;
} hset;

/* One container of the walked tree as stored in the state file. */
typedef struct cont {
    char id[SEL_UPNP_ID_BYTES], parent[SEL_UPNP_ID_BYTES];
    unsigned char dead;
} cont;

/* I894a5: the per-server delta state. The library only holds the rows, so the state keeps what a delta needs: the server UDN, its SystemUpdateID, the CD:3 reset
 * token, the walk's start container, every walked container (`c id parent`) and every stored row with the container
 * it was listed in (`r container path`). I894a6: the engine owns it (catalogue->state_get/state_set, table source_state in
 * the library db, dropped with the source's rows); `file` is only the working copy of that blob in the cache dir while a
 * sync runs, so a removed source, a reset db or another db starts with no state and walks in full. */
typedef struct state {
    char file[640];
    char key[32];  /* catalogue state key of this server */
    int unchanged; /* the sync changed nothing: the stored blob stays */
    char udn[SEL_UPNP_NAME_BYTES];
    char token[64];
    char start[SEL_UPNP_ID_BYTES];
    unsigned long sysid;
    cont* c;
    size_t cn, ccap;
    uint32_t rows;
    int valid;
} state;

typedef struct run {
    const sel_host* host;
    const sel_catalogue* cat;
    const char* server;
    const char* control;
    uint32_t rows, skipped, requests, dups, removed, errors;
    hset urls;  /* res URL hashes already stored in this sync (I894a4 dedup) */
    hset paths; /* catalogue path hashes listed in this sync; a stored row outside it has vanished (I894a5) */
    FILE* out;  /* part file the walk writes its `c` and `r` lines to */
    const char* cur;                    /* container whose items are being added */
    char (*subs)[SEL_UPNP_ID_BYTES];    /* containers listed by a shallow walk */
    unsigned nsubs;
    state* known;                       /* shallow walk: containers the old state already has */
} run;

static uint64_t str_hash(const char* s) {
    uint64_t h = 1469598103934665603ull; /* FNV-1a */

    for (; *s != '\0'; s++) {
        h = (h ^ (unsigned char)*s) * 1099511628211ull;
    }
    return h != 0u ? h : 1u;
}

static int hset_has(const hset* s, uint64_t h) {
    size_t i;

    if (s->cap == 0u) {
        return 0;
    }
    for (i = (size_t)(h % s->cap); s->t[i] != 0u; i = (i + 1u) % s->cap) {
        if (s->t[i] == h) {
            return 1;
        }
    }
    return 0;
}

/* Adds the hash. 1 when it was new, 0 when the set had it. */
static int hset_add(hset* s, uint64_t h) {
    size_t i;

    if ((s->n + 1u) * 2u > s->cap) {
        size_t ncap = s->cap != 0u ? s->cap * 2u : 4096u, k;
        uint64_t* nt = (uint64_t*)calloc(ncap, sizeof *nt);

        if (nt == NULL) {
            return 1; /* out of memory: keep the song rather than lose it */
        }
        for (k = 0; k < s->cap; k++) {
            if (s->t[k] != 0u) {
                size_t j = (size_t)(s->t[k] % ncap);

                while (nt[j] != 0u) {
                    j = (j + 1u) % ncap;
                }
                nt[j] = s->t[k];
            }
        }
        free(s->t);
        s->t = nt;
        s->cap = ncap;
    }
    for (i = (size_t)(h % s->cap); s->t[i] != 0u; i = (i + 1u) % s->cap) {
        if (s->t[i] == h) {
            return 0;
        }
    }
    s->t[i] = h;
    s->n++;
    return 1;
}

static void clean_part(char* s) {
    for (; *s != '\0'; s++) {
        if (*s == '/' || *s == '|' || (unsigned char)*s < 32u) {
            *s = ' ';
        }
    }
}

static int is_audio(const sel_upnp_entry* e) {
    return e->res_url[0] != '\0' &&
           (strstr(e->protocol_info, ":audio/") != NULL || strstr(e->protocol_info, ":application/ogg") != NULL);
}

static void add_item(run* r, const sel_upnp_entry* e) {
    char artist[SEL_UPNP_TEXT_BYTES], title[SEL_UPNP_TEXT_BYTES], url[SEL_UPNP_URL_BYTES + 32u];
    char path[URL_BYTES + 2u * SEL_UPNP_TEXT_BYTES];
    sel_catalogue_song song;
    size_t i, j = 0;

    if (!is_audio(e) || e->title[0] == '\0') {
        r->skipped++;
        return;
    }
    if (!hset_add(&r->urls, str_hash(e->res_url))) {
        r->dups++;
        return;
    }
    snprintf(artist, sizeof artist, "%s", e->artist[0] != '\0' ? e->artist : r->server);
    snprintf(title, sizeof title, "%s", e->title);
    clean_part(artist);
    clean_part(title);
    for (i = 0; e->res_url[i] != '\0' && j + 4u < sizeof url; i++) { /* '|' separates catalogue fields: escape it */
        if (e->res_url[i] == '|') {
            j += (size_t)snprintf(url + j, sizeof url - j, "%%7C");
        } else {
            url[j++] = e->res_url[i];
        }
    }
    url[j] = '\0';
    snprintf(path, sizeof path, "%s/%s/%s", artist, title, url);
    hset_add(&r->paths, str_hash(path));
    memset(&song, 0, sizeof song);
    song.path = path;
    song.format = "DLNA";
    song.artist = artist;
    song.album = e->album;
    song.name = title;
    song.md5 = "";
    song.duration_ms = (int32_t)e->duration_ms;
    if (!r->cat->song(r->host, &song)) {
        r->skipped++;
        return;
    }
    r->rows++;
    if (r->out != NULL && r->cur != NULL) {
        for (i = 0; path[i] != '\0' && (unsigned char)path[i] >= 32u; i++) {
        }
        if (path[i] == '\0') { /* a path with a control character cannot sit in the line based state */
            fprintf(r->out, "r\t%s\t%s\n", r->cur, path);
        }
    }
}

static int cont_known(const state* st, const char* id) {
    size_t i;

    for (i = 0; st != NULL && i < st->cn; i++) {
        if (strcmp(st->c[i].id, id) == 0) {
            return 1;
        }
    }
    return 0;
}

/* Depth first walk of one container; pages by StartingIndex until TotalMatches. 0 on a transport error. Writes a
 * `c` line per container to r->out. `shallow` (a delta re-browse of one container, I894a5) lists only this level:
 * containers the old state already has are noted in r->subs, new ones are walked in full. */
static int walk(run* r, const char* id, const char* parent, unsigned depth, int shallow) {
    sel_upnp_entry* page = (sel_upnp_entry*)malloc(SEL_UPNP_BROWSE_PAGE * sizeof *page);
    unsigned start = 0, total = 0;
    int ok = 1;

    if (page == NULL) {
        return 0;
    }
    if (!shallow && r->out != NULL) {
        fprintf(r->out, "c\t%s\t%s\n", id, parent);
    }
    do {
        int n = sel_upnp_browse(r->control, id, start, SEL_UPNP_BROWSE_PAGE, page, SEL_UPNP_BROWSE_PAGE, &total);
        int i;

        r->requests++;
        if (n < 0) {
            ok = 0;
            r->errors++;
            break;
        }
        if (n == 0) {
            break;
        }
        for (i = 0; i < n && r->rows < MAX_SONGS; i++) {
            if (!page[i].is_container) {
                r->cur = id;
                add_item(r, &page[i]);
            } else if (shallow) {
                if (r->subs != NULL && r->nsubs < MAX_SUBS) {
                    snprintf(r->subs[r->nsubs++], SEL_UPNP_ID_BYTES, "%s", page[i].id);
                } else {
                    r->errors++;
                }
                if (!cont_known(r->known, page[i].id)) {
                    walk(r, page[i].id, id, 1u, 0);
                }
            } else if (depth + 1u < MAX_DEPTH) {
                walk(r, page[i].id, id, depth + 1u, 0); /* a failing sub-container does not drop the rest of the share */
            }
        }
        start += (unsigned)n;
    } while (start < total && r->rows < MAX_SONGS);
    free(page);
    return ok;
}

static int ci_has(const char* s, const char* word) {
    size_t n = strlen(word), i;

    for (i = 0; s[i] != '\0'; i++) {
        size_t k = 0;

        while (k < n && s[i + k] != '\0' && (s[i + k] | 32) == word[k]) {
            k++;
        }
        if (k == n) {
            return 1;
        }
    }
    return 0;
}

/* Looks for the server's folder view in the children of container `id`: a container whose title holds "folder".
 * Returns 1 and copies its id; 0 when none. `below` collects the other containers for a one level deeper search. */
static int find_in(run* r, const char* id, char* folder, size_t fn, char (*below)[SEL_UPNP_ID_BYTES], unsigned* nb) {
    sel_upnp_entry* page = (sel_upnp_entry*)malloc(SEL_UPNP_BROWSE_PAGE * sizeof *page);
    unsigned start = 0, total = 0;
    int found = 0;

    if (page == NULL) {
        return 0;
    }
    do {
        int n = sel_upnp_browse(r->control, id, start, SEL_UPNP_BROWSE_PAGE, page, SEL_UPNP_BROWSE_PAGE, &total);
        int i;

        r->requests++;
        if (n <= 0) {
            break;
        }
        for (i = 0; i < n && !found; i++) {
            if (!page[i].is_container) {
                continue;
            }
            if (ci_has(page[i].title, "folder")) {
                snprintf(folder, fn, "%s", page[i].id);
                found = 1;
            } else if (below != NULL && *nb < 16u) {
                snprintf(below[(*nb)++], SEL_UPNP_ID_BYTES, "%s", page[i].id);
            }
        }
        start += (unsigned)n;
    } while (!found && start < total);
    free(page);
    return found;
}

/* The container to walk: the folder view under the root or one level below it (Synology: Music > Folder), else
 * the root "0" (full walk). */
static void pick_start(run* r, char* start, size_t n) {
    char (*below)[SEL_UPNP_ID_BYTES] = (char(*)[SEL_UPNP_ID_BYTES])malloc(16u * SEL_UPNP_ID_BYTES);
    unsigned nb = 0, i;

    snprintf(start, n, "0");
    if (below == NULL) {
        return;
    }
    if (!find_in(r, "0", start, n, below, &nb)) {
        for (i = 0; i < nb; i++) {
            if (find_in(r, below[i], start, n, NULL, NULL)) {
                break;
            }
        }
    }
    free(below);
}

/* Reads the device description at `location` into `out`. 0 when it is unreachable or has no ContentDirectory. */
static int describe(const sel_host* host, const sel_net* net, const char* location, sel_upnp_server* out,
                    uint32_t* rq) {
    char* xml = (char*)malloc(DESC_CAP);
    uint32_t got = 0;
    int ok = 0;

    if (xml == NULL) {
        return 0;
    }
    if (net->request_ex(host, location, USER_AGENT, NULL, xml, DESC_CAP - 1u, &got, rq) == 200) {
        xml[got] = '\0';
        memset(out, 0, sizeof *out);
        snprintf(out->location, sizeof out->location, "%s", location);
        ok = sel_upnp_parse_description(xml, location, out);
    }
    free(xml);
    return ok;
}

/* Splits "a\tb\tc" in place: returns the text after the first tab and ends `s` there; NULL without a tab. */
static char* tab(char* s) {
    char* t = strchr(s, '\t');

    if (t == NULL) {
        return NULL;
    }
    *t = '\0';
    return t + 1;
}

static cont* cont_find(state* st, const char* id) {
    size_t i;

    for (i = 0; i < st->cn; i++) {
        if (strcmp(st->c[i].id, id) == 0) {
            return &st->c[i];
        }
    }
    return NULL;
}

static void state_free(state* st) {
    free(st->c);
    st->c = NULL;
    st->cn = st->ccap = 0u;
    st->valid = 0;
    st->rows = 0u;
}

/* Reads st->file: header fields and the container list; rows are only counted (they are streamed when needed). */
static void state_load(state* st) {
    FILE* f;
    char line[LINE_BYTES];

    state_free(st);
    st->udn[0] = st->token[0] = st->start[0] = '\0';
    st->sysid = 0;
    f = st->file[0] != '\0' ? fopen(st->file, "r") : NULL;
    if (f == NULL) {
        return;
    }
    if (fgets(line, sizeof line, f) == NULL || strncmp(line, STATE_MAGIC, sizeof STATE_MAGIC - 1u) != 0) {
        fclose(f);
        return;
    }
    while (fgets(line, sizeof line, f) != NULL) {
        char* a = line;
        char* b;
        char* c;

        line[strcspn(line, "\n")] = '\0';
        b = tab(a);
        if (b == NULL) {
            continue;
        }
        if (strcmp(a, "udn") == 0) {
            snprintf(st->udn, sizeof st->udn, "%s", b);
        } else if (strcmp(a, "sysid") == 0) {
            st->sysid = strtoul(b, NULL, 10);
        } else if (strcmp(a, "token") == 0) {
            snprintf(st->token, sizeof st->token, "%s", b);
        } else if (strcmp(a, "start") == 0) {
            snprintf(st->start, sizeof st->start, "%s", b);
        } else if (strcmp(a, "r") == 0) {
            st->rows++;
        } else if (strcmp(a, "c") == 0 && (c = tab(b)) != NULL) {
            if (st->cn == st->ccap) {
                size_t nc = st->ccap != 0u ? st->ccap * 2u : 256u;
                cont* nv = (cont*)realloc(st->c, nc * sizeof *nv);

                if (nv == NULL) {
                    fclose(f);
                    state_free(st);
                    return;
                }
                st->c = nv;
                st->ccap = nc;
            }
            memset(&st->c[st->cn], 0, sizeof st->c[0]);
            snprintf(st->c[st->cn].id, SEL_UPNP_ID_BYTES, "%s", b);
            snprintf(st->c[st->cn].parent, SEL_UPNP_ID_BYTES, "%s", c);
            st->cn++;
        }
    }
    fclose(f);
    st->valid = 1;
}

static int flush_removed(run* r, char** rem, unsigned* n) {
    int ok = 1;
    unsigned i;

    if (*n != 0u) {
        ok = r->cat->remove(r->host, (const char* const*)rem, *n) != 0;
        r->removed += *n;
    }
    for (i = 0; i < *n; i++) {
        free(rem[i]);
    }
    *n = 0u;
    return ok;
}

/* Writes the new state: header, then the old lines that survive, then the part file the walk just wrote. Rows of the
 * dropped containers whose path this sync did not list again are removed from the library. `full` drops every old
 * container and row (a full walk); else container `x` (its rows are replaced by the part file) and the old
 * sub-containers of x that the shallow walk no longer listed, with their descendants, are dropped. x NULL only
 * refreshes the header. The new file replaces st->file. 0 on failure. */
static int state_stage(run* r, state* st, int full, const char* x, const char* part_file, unsigned long sysid,
                       const char* token, const char* start, const char* udn) {
    char tmp[sizeof st->file + 8u], line[LINE_BYTES];
    char* rem[64];
    unsigned nrem = 0;
    size_t i, j;
    int changed = 1, ok = 1;
    FILE *o, *f, *pf;

    if (!full && x != NULL) {
        for (i = 0; i < st->cn; i++) {
            if (strcmp(st->c[i].parent, x) == 0) {
                unsigned k;

                for (k = 0; k < r->nsubs && strcmp(r->subs[k], st->c[i].id) != 0; k++) {
                }
                st->c[i].dead = k == r->nsubs;
            }
        }
        while (changed) { /* descendants of a dead container are dead */
            changed = 0;
            for (i = 0; i < st->cn; i++) {
                for (j = 0; st->c[i].dead && j < st->cn; j++) {
                    if (!st->c[j].dead && strcmp(st->c[j].parent, st->c[i].id) == 0) {
                        st->c[j].dead = 1;
                        changed = 1;
                    }
                }
            }
        }
    }
    snprintf(tmp, sizeof tmp, "%s.new", st->file);
    o = fopen(tmp, "w");
    if (o == NULL) {
        return 0;
    }
    fprintf(o, "%s\nudn\t%s\nsysid\t%lu\ntoken\t%s\nstart\t%s\n", STATE_MAGIC, udn, sysid, token, start);
    f = fopen(st->file, "r");
    while (f != NULL && fgets(line, sizeof line, f) != NULL) {
        char* a = line;
        char* b;
        char* c;
        cont* k;

        line[strcspn(line, "\n")] = '\0';
        b = tab(a);
        c = b != NULL ? tab(b) : NULL;
        if (c == NULL) {
            continue;
        }
        if (strcmp(a, "c") == 0) {
            k = cont_find(st, b);
            if (!full && (k == NULL || !k->dead)) {
                fprintf(o, "c\t%s\t%s\n", b, c);
            }
        } else if (strcmp(a, "r") == 0) {
            k = cont_find(st, b);
            if (full || (x != NULL && strcmp(b, x) == 0) || (k != NULL && k->dead)) {
                if (!hset_has(&r->paths, str_hash(c)) && nrem < 64u && (rem[nrem] = strdup(c)) != NULL) {
                    nrem++;
                    if (nrem == 64u) {
                        ok &= flush_removed(r, rem, &nrem);
                    }
                }
            } else {
                fprintf(o, "r\t%s\t%s\n", b, c);
            }
        }
    }
    if (f != NULL) {
        fclose(f);
    }
    ok &= flush_removed(r, rem, &nrem);
    pf = part_file != NULL ? fopen(part_file, "r") : NULL;
    while (pf != NULL && fgets(line, sizeof line, pf) != NULL) {
        fputs(line, o);
    }
    if (pf != NULL) {
        fclose(pf);
    }
    ok &= fclose(o) == 0 && ferror(o) == 0;
    return ok && rename(tmp, st->file) == 0;
}

static void state_path(const sel_host* host, const sel_catalogue* cat, const sel_upnp_server* sv, state* st) {
    char dir[512];

    unsigned long long h = (unsigned long long)str_hash(sv->udn[0] != '\0' ? sv->udn : sv->control_url);

    st->file[0] = '\0';
    snprintf(st->key, sizeof st->key, "state-%016llx", h);
    if (cat->cache_dir != NULL && cat->cache_dir(host, "dlna", dir, sizeof dir)) {
        snprintf(st->file, sizeof st->file, "%s/%016llx.state", dir, h);
    }
}

/* Writes the db's blob of this server to the working file; no blob (or no way to read it) removes the working file. */
static void state_fetch(const sel_host* host, const sel_catalogue* cat, const state* st) {
    uint32_t len = 0;
    void* buf;
    FILE* f;
    int ok = 0;

    if (st->file[0] == '\0') {
        return;
    }
    remove(st->file);
    if (cat->state_get == NULL || !cat->state_get(host, st->key, NULL, 0u, &len) || len == 0u) {
        return;
    }
    buf = malloc(len);
    if (buf != NULL && cat->state_get(host, st->key, buf, len, &len) && (f = fopen(st->file, "w")) != NULL) {
        ok = fwrite(buf, 1u, len, f) == len;
        ok &= fclose(f) == 0;
    }
    free(buf);
    if (!ok) {
        remove(st->file);
    }
}

/* Hands the working file to the db (no file = the state is dropped, the next sync walks in full), then removes it. */
static void state_push(const sel_host* host, const sel_catalogue* cat, const state* st) {
    FILE* f = st->file[0] != '\0' ? fopen(st->file, "r") : NULL;
    void* buf = NULL;
    long len = 0;

    if (st->file[0] == '\0' || cat->state_set == NULL) {
        return;
    }
    if (st->unchanged && f != NULL) {
        fclose(f);
        remove(st->file);
        return;
    }
    if (f != NULL) {
        if (fseek(f, 0, SEEK_END) == 0 && (len = ftell(f)) > 0 && fseek(f, 0, SEEK_SET) == 0 &&
            (buf = malloc((size_t)len)) != NULL && fread(buf, 1u, (size_t)len, f) != (size_t)len) {
            free(buf);
            buf = NULL;
        }
        fclose(f);
    }
    (void)cat->state_set(host, st->key, buf, buf != NULL ? (uint32_t)len : 0u);
    free(buf);
    remove(st->file);
}

static FILE* part_open(run* r, const state* st, char* part, size_t n) {
    snprintf(part, n, "%s.part", st->file);
    r->out = st->file[0] != '\0' ? fopen(part, "w") : NULL;
    return r->out;
}

static void part_close(run* r, const char* part) {
    if (r->out != NULL) {
        fclose(r->out);
        remove(part);
    }
    r->out = NULL;
}

/* One level of container x re-browsed alone, then the state is restaged. 1 when the state was updated. */
static int rebrowse(run* r, state* st, const char* x, unsigned long sysid) {
    char part[sizeof st->file + 8u];
    uint32_t err0 = r->errors;
    int ok;

    r->nsubs = 0u;
    r->known = st;
    part_open(r, st, part, sizeof part);
    ok = walk(r, x, "", 0u, 1);
    r->known = NULL;
    if (r->out != NULL) {
        fflush(r->out);
    }
    ok = ok && r->errors == err0 && r->out != NULL &&
         state_stage(r, st, 0, x, part, sysid, st->token, st->start, st->udn);
    part_close(r, part);
    if (ok) {
        char udn[SEL_UPNP_NAME_BYTES], token[64];

        snprintf(udn, sizeof udn, "%s", st->udn);
        snprintf(token, sizeof token, "%s", st->token);
        state_load(st);
        snprintf(st->udn, sizeof st->udn, "%s", udn);
    }
    return ok;
}

/* CD:3 change tracking: Search for the containers whose objectUpdateID passed the stored SystemUpdateID and re-browse
 * each of them alone. 0 when the server cannot answer the Search (the caller walks in full); else 1 and *ok. */
static int delta_search(run* r, const sel_upnp_server* sv, state* st, unsigned long sysid, int* ok) {
    sel_upnp_entry* page = (sel_upnp_entry*)malloc(SEL_UPNP_BROWSE_PAGE * sizeof *page);
    char (*ids)[SEL_UPNP_ID_BYTES] = (char(*)[SEL_UPNP_ID_BYTES])malloc(MAX_CHANGED * SEL_UPNP_ID_BYTES);
    unsigned start = 0, total = 0, nid = 0, i;
    int n, handled = 1, did = 0;

    *ok = 1;
    if (page == NULL || ids == NULL) {
        free(page);
        free(ids);
        return 0;
    }
    do {
        n = sel_upnp_search_changed(sv->control_url, st->sysid, start, page, SEL_UPNP_BROWSE_PAGE, &total);
        r->requests++;
        if (n < 0 || total > MAX_CHANGED) {
            handled = 0;
            break;
        }
        for (i = 0; i < (unsigned)n; i++) {
            if (page[i].is_container && nid < MAX_CHANGED) {
                snprintf(ids[nid++], SEL_UPNP_ID_BYTES, "%s", page[i].id);
            }
        }
        start += (unsigned)(n > 0 ? n : 0);
    } while (n > 0 && start < total);
    for (i = 0; handled && i < nid; i++) {
        if (cont_known(st, ids[i])) {
            fprintf(stderr, "dlna: container %s changed, re-browsing it alone\n", ids[i]);
            *ok &= rebrowse(r, st, ids[i], sysid);
            did = 1;
        }
    }
    if (handled && *ok && !did) { /* nothing a container holds changed: only the stored SystemUpdateID moves */
        *ok = state_stage(r, st, 0, NULL, NULL, sysid, st->token, st->start, st->udn);
    }
    free(page);
    free(ids);
    return handled;
}

static int full_walk(run* r, const sel_upnp_server* sv, state* st, int have, unsigned long sysid, const char* token) {
    char from[SEL_UPNP_ID_BYTES], part[sizeof st->file + 8u];
    uint32_t err0 = r->errors;
    int ok;

    pick_start(r, from, sizeof from);
    fprintf(stderr, "dlna: walking %s\n", strcmp(from, "0") == 0 ? "the whole tree (no folder view)" : "the folder view");
    part_open(r, st, part, sizeof part);
    ok = walk(r, from, "", 0u, 0);
    if (r->out != NULL) {
        fflush(r->out);
    }
    if (ok && have && r->out != NULL && r->errors == err0) { /* a failed sub-container keeps the old state */
        state_stage(r, st, 1, NULL, part, sysid, token, from, sv->udn);
    }
    part_close(r, part);
    return ok;
}

/* Brings one server's rows up to date: unchanged SystemUpdateID = nothing to do; CD:3 change tracking = only the
 * changed containers; else the full walk. */
static int sync_server_state(run* r, const sel_upnp_server* sv, state* stp) {
    state st = *stp;
    unsigned long sysid = 0;
    char token[64] = "";
    int have, ok = 1, handled = 0, force = getenv("SELECTA_DLNA_FULL") != NULL;

    state_load(&st);
    have = sel_upnp_get_system_update_id(sv->control_url, &sysid);
    r->requests++;
    if (have && sv->cd_version >= 3) {
        (void)sel_upnp_get_reset_token(sv->control_url, token, sizeof token);
        r->requests++;
    }
    if (have && st.valid && !force && st.sysid == sysid && strcmp(st.udn, sv->udn) == 0) {
        fprintf(stderr, "dlna: SystemUpdateID %lu unchanged, no Browse needed (%u stored rows kept)\n", sysid,
                (unsigned)st.rows);
        r->rows += st.rows;
        stp->unchanged = 1;
        state_free(&st);
        return 1;
    }
    if (have && st.valid && !force && sv->cd_version >= 3 && token[0] != '\0' && strcmp(token, st.token) == 0 &&
        strcmp(st.udn, sv->udn) == 0) {
        uint32_t rows0 = r->rows;

        handled = delta_search(r, sv, &st, sysid, &ok);
        if (handled) { /* the re-browsed rows were counted by the walk and are inside st.rows too */
            r->rows = rows0 + st.rows;
        }
    }
    if (!handled) {
        ok = full_walk(r, sv, &st, have, sysid, token);
    }
    if (!ok && st.file[0] != '\0') {
        remove(st.file); /* the library may be ahead of the state: the next sync walks in full */
    }
    state_free(&st);
    return ok;
}

static int sync_server(run* r, const sel_upnp_server* sv) {
    state st;
    int ok;

    memset(&st, 0, sizeof st);
    state_path(r->host, r->cat, sv, &st);
    state_fetch(r->host, r->cat, &st);
    ok = sync_server_state(r, sv, &st);
    state_push(r->host, r->cat, &st);
    return ok;
}

static int dl_sync(const sel_host* host, const char* arg, int64_t now, sel_tag* stats, uint32_t cap) {
    const sel_net* net = host->net != NULL ? host->net(host) : NULL;
    const sel_catalogue* cat = host->catalogue != NULL ? host->catalogue(host) : NULL;
    sel_upnp_server servers[MAX_SERVERS];
    int n = 0, i, ok = 0;
    run r;

    (void)now;
    if (net == NULL || net->request_ex == NULL || cat == NULL || arg == NULL) {
        return 0;
    }
    memset(&r, 0, sizeof r);
    r.host = host;
    r.cat = cat;
    r.subs = (char(*)[SEL_UPNP_ID_BYTES])malloc(MAX_SUBS * SEL_UPNP_ID_BYTES);
    /* The UI's Sync button sends the source home (DLNA_HOME, from the
       SOURCES reply): like an empty arg it means "find the servers on the
       network", the same as an explicit "discover" (I1072). */
    if (arg[0] == '\0' || strcmp(arg, "discover") == 0 || strcmp(arg, DLNA_HOME) == 0) {
        const char* env = getenv("SELECTA_DLNA_SSDP");
        char ip[48] = "";
        unsigned port = 0;

        if (env != NULL && sscanf(env, "%47[^:]:%u", ip, &port) == 2) {
            n = sel_upnp_discover(ip, port, SSDP_WAIT_MS, servers, MAX_SERVERS);
        } else {
            n = sel_upnp_discover(NULL, 0u, SSDP_WAIT_MS, servers, MAX_SERVERS);
        }
        if (n <= 0) {
            fputs("dlna: no MediaServer with a ContentDirectory answered the SSDP search\n", stderr);
            n = 0;
        }
    } else if (strncmp(arg, "http", 4) == 0 && describe(host, net, arg, &servers[0], &r.requests)) {
        n = 1;
    } else {
        fputs("dlna: sync takes a device description URL (http://host:port/desc.xml) or \"discover\"\n", stderr);
        free(r.subs);
        return 0;
    }
    if (r.subs == NULL || !cat->begin(host, "dlna")) {
        free(r.subs);
        return 0;
    }
    ok = 1;
    for (i = 0; i < n; i++) {
        fprintf(stderr, "dlna: server %s control %s (ContentDirectory:%d)\n", servers[i].name, servers[i].control_url,
                servers[i].cd_version);
        r.server = servers[i].name;
        r.control = servers[i].control_url;
        ok &= sync_server(&r, &servers[i]);
    }
    fprintf(stderr, "dlna: %u songs, %u repeated res URLs skipped, %u vanished rows removed, %u requests\n",
            (unsigned)r.rows, (unsigned)r.dups, (unsigned)r.removed, (unsigned)r.requests);
    free(r.urls.t);
    free(r.paths.t);
    free(r.subs);
    if (ok) {
        ok = cat->commit(host);
    } else {
        cat->abort(host);
    }
    sel_sync_stat(stats, cap, SEL_SYNC_LISTING_ROWS, r.rows);
    sel_sync_stat(stats, cap, SEL_SYNC_SKIPPED_ROWS, r.skipped);
    sel_sync_stat(stats, cap, SEL_SYNC_NET_REQUESTS, r.requests);
    return ok;
}

/* The res URL is everything after the second '/'. */
static int dl_play_url(const sel_host* host, const char* path, char* out, size_t n) {
    const char* a = strchr(path, '/');
    const char* b = a != NULL ? strchr(a + 1, '/') : NULL;
    size_t i, o = 0;

    (void)host;
    if (b == NULL || b[1] == '\0') {
        return 0;
    }
    for (b++, i = 0; b[i] != '\0' && o + 1 < n; i++) {
        if (strncmp(b + i, "%7C", 3) == 0) {
            out[o++] = '|';
            i += 2;
        } else {
            out[o++] = b[i];
        }
    }
    out[o] = '\0';
    return b[i] == '\0';
}

static int parse(const char* path, char* scratch, size_t n, sel_tag* out) {
    static const char fmt[] = "DLNA";
    const char* name = path != NULL ? strchr(path, '/') : NULL;
    const char* url = name != NULL ? strchr(name + 1, '/') : NULL;
    size_t al, nl;

    if (url == NULL) {
        return 0;
    }
    al = (size_t)(name - path);
    nl = (size_t)(url - name - 1);
    if (sizeof fmt + al + 1u + nl + 1u > n) {
        return 0;
    }
    memcpy(scratch, fmt, sizeof fmt);
    memcpy(scratch + sizeof fmt, path, al);
    scratch[sizeof fmt + al] = '\0';
    memcpy(scratch + sizeof fmt + al + 1u, name + 1, nl);
    scratch[sizeof fmt + al + 1u + nl] = '\0';
    out[0].tag = SEL_SRC_FORMAT;
    out[0].value = (uintptr_t)scratch;
    out[1].tag = SEL_SRC_ARTIST;
    out[1].value = (uintptr_t)(scratch + sizeof fmt);
    out[2].tag = SEL_SRC_NAME;
    out[2].value = (uintptr_t)(scratch + sizeof fmt + al + 1u);
    out[3].tag = SEL_TAG_END;
    out[3].value = 0;
    return 1;
}

static const sel_source g_dlna = {
    "dlna", "DLNA", DLNA_HOME,
    "Music from a UPnP / DLNA media server on your network (Synology Media Server, Minidlna, ...).",
    g_tags, NULL, parse, NULL, NULL, NULL, NULL, NULL, dl_sync,
    SEL_SOURCE_REMOTE | SEL_SOURCE_SYNCS | SEL_SOURCE_STREAMS | SEL_SOURCE_SEEKABLE, DLNA_HOME, dl_play_url,
    NULL, /* search (I406) */
    NULL, /* search_ready (I1037) */
};

const sel_tag* SEL_PLUGIN_ENTRY(uint32_t host_api) {
    (void)host_api;
    return sel_source_plugin_descriptor("dlna", &g_dlna);
}
