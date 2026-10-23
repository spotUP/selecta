/* last.fm scrobbler (plan 2026-10-25-m3-library.md, I177, decision D-M3-15; design doc offline scrobble queue).
 * Desktop only. Listens for SCROBBLE_DUE (the engine applies the last.fm rule: longer than 30 s, due at half the
 * length or 4 minutes), appends the song to a persistent queue file, and flushes the queue in batches of up to 50
 * with one signed track.scrobble POST per batch (API 2.0, public docs: every parameter except format and
 * callback, sorted by name, name and value concatenated, secret appended, MD5 as api_sig).
 *
 * Configuration (a pref wins over the environment; nothing is built in, scrobbling is off until key, secret and
 * session key are all set):
 *   scrobble.lastfm_key     / SELECTA_LASTFM_KEY       API key
 *   scrobble.lastfm_secret  / SELECTA_LASTFM_SECRET    shared secret
 *   scrobble.lastfm_session / SELECTA_LASTFM_SESSION   session key (selecta-cli lastfm login)
 *   scrobble.lastfm_url     / SELECTA_LASTFM_URL       endpoint, default https://ws.audioscrobbler.com/2.0/
 *   scrobble.queue          / SELECTA_SCROBBLE_QUEUE   queue file, default $HOME/.selecta/scrobble.queue
 *   scrobble.fallback_artist / SELECTA_SCROBBLE_ARTIST artist for songs whose tags carry no author (else skipped)
 *   SELECTA_SCROBBLE_NOW                               test clock: "now" in Unix seconds
 * A failed flush keeps every entry with its original timestamp and retries from INT_POLL with back-off
 * (60 s doubling to 1 h). The queue holds at most 500 entries; the oldest are dropped with a warning.
 * On close it prints `scrobble_due=<n> scrobble_posts=<n> scrobble_pending=<n>`. */
#include <selecta/engine.h>
#include <selecta/host.h>
#include <selecta/integration.h>
#include <selecta/plugin.h>
#include <selecta/tags.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define QUEUE_MAX 500
#define BATCH_MAX 50
#define FIELD 160
#define PATH_MAX_LEN 512

typedef struct entry {
    long ts;
    unsigned dur_s;
    char artist[FIELD];
    char track[FIELD];
    char album[FIELD];
} entry;

typedef struct lf_state {
    sel_int_host ih;
    const sel_host* host;
    char key[96];
    char secret[96];
    char session[96];
    char url[256];
    char queue[PATH_MAX_LEN];
    char fallback_artist[FIELD];
    int enabled;
    unsigned due;
    unsigned posts;
    unsigned pending;
    unsigned fails;
    uint32_t retry_at_ms;
} lf_state;

static void cfg(const sel_host* h, const char* pref, const char* env, const char* def, char* out, size_t cap) {
    const char* v = h != NULL && h->config_get != NULL ? h->config_get(h, pref, NULL) : NULL;
    if (v == NULL || v[0] == '\0') {
        v = getenv(env);
    }
    if (v == NULL || v[0] == '\0') {
        v = def;
    }
    snprintf(out, cap, "%s", v != NULL ? v : "");
}

static long now_s(void) {
    const char* t = getenv("SELECTA_SCROBBLE_NOW");
    return t != NULL && t[0] != '\0' ? atol(t) : (long)time(NULL);
}

/* ---- queue file: "ts<TAB>dur<TAB>artist<TAB>track<TAB>album" per line ---- */

static void clean(char* dst, const char* src) {
    size_t n = 0;
    for (; src != NULL && *src != '\0' && n + 1 < FIELD; src++) {
        dst[n++] = (*src == '\t' || *src == '\n' || *src == '\r') ? ' ' : *src;
    }
    dst[n] = '\0';
}

static int parse_line(char* line, entry* e) {
    char* f[5];
    char* p = line;
    int i;
    for (i = 0; i < 5; i++) {
        f[i] = p;
        p = strchr(p, i == 4 ? '\n' : '\t');
        if (p == NULL) {
            if (i < 4) {
                return 0;
            }
        } else {
            *p++ = '\0';
        }
    }
    e->ts = atol(f[0]);
    e->dur_s = (unsigned)atoi(f[1]);
    clean(e->artist, f[2]);
    clean(e->track, f[3]);
    clean(e->album, f[4]);
    return e->artist[0] != '\0' && e->track[0] != '\0';
}

/* Reads the queue into a heap array (oldest first); *n is the count. */
static entry* queue_load(const lf_state* s, unsigned* n) {
    FILE* f = fopen(s->queue, "r");
    entry* all = calloc(QUEUE_MAX + 1u, sizeof *all);
    char line[3 * FIELD + 64];
    *n = 0;
    if (all == NULL || f == NULL) {
        if (f != NULL) {
            fclose(f);
        }
        return all;
    }
    while (fgets(line, sizeof line, f) != NULL) {
        entry e;
        if (parse_line(line, &e)) {
            if (*n == QUEUE_MAX) {
                memmove(all, all + 1, (QUEUE_MAX - 1u) * sizeof *all); /* drop the oldest */
                (*n)--;
                s->host->log(s->host, SEL_LOG_WARN, "scrobble queue full: oldest entry dropped");
            }
            all[(*n)++] = e;
        }
    }
    fclose(f);
    return all;
}

/* Writes entries to a temporary file and renames it over the queue, so a crash keeps one whole version. */
static int queue_store(const lf_state* s, const entry* all, unsigned n) {
    char tmp[PATH_MAX_LEN + 8];
    FILE* f;
    unsigned i;
    snprintf(tmp, sizeof tmp, "%s.tmp", s->queue);
    f = fopen(tmp, "w");
    if (f == NULL) {
        return 0;
    }
    for (i = 0; i < n; i++) {
        fprintf(f, "%ld\t%u\t%s\t%s\t%s\n", all[i].ts, all[i].dur_s, all[i].artist, all[i].track, all[i].album);
    }
    if (fclose(f) != 0) {
        return 0;
    }
    return rename(tmp, s->queue) == 0;
}

/* ---- request ---- */

typedef struct param {
    char name[24];
    char value[FIELD];
} param;

static void pct_encode(const char* in, char* out, size_t cap) {
    static const char hex[] = "0123456789ABCDEF";
    size_t n = 0;
    for (; *in != '\0' && n + 4 < cap; in++) {
        unsigned char c = (unsigned char)*in;
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' ||
            c == '.' || c == '~') {
            out[n++] = (char)c;
        } else {
            out[n++] = '%';
            out[n++] = hex[c >> 4];
            out[n++] = hex[c & 15u];
        }
    }
    out[n] = '\0';
}

static void add_param(param* p, unsigned* np, const char* name, const char* value) {
    snprintf(p[*np].name, sizeof p[*np].name, "%s", name);
    snprintf(p[*np].value, sizeof p[*np].value, "%s", value);
    (*np)++;
}

static void add_indexed(param* p, unsigned* np, const char* base, unsigned i, const char* value) {
    char name[24];
    snprintf(name, sizeof name, "%s[%u]", base, i);
    add_param(p, np, name, value);
}

static int param_cmp(const void* a, const void* b) {
    return strcmp(((const param*)a)->name, ((const param*)b)->name);
}

/* The api_sig: parameters sorted by name, "name" "value" concatenated, secret appended, MD5 hex. */
static void sign(const lf_state* s, param* p, unsigned np, char out[33]) {
    char* buf = malloc((size_t)np * (sizeof p->name + sizeof p->value) + sizeof s->secret + 1u);
    size_t n = 0;
    unsigned i;
    if (buf == NULL) {
        out[0] = '\0';
        return;
    }
    qsort(p, np, sizeof *p, param_cmp);
    for (i = 0; i < np; i++) {
        n += (size_t)sprintf(buf + n, "%s%s", p[i].name, p[i].value);
    }
    n += (size_t)sprintf(buf + n, "%s", s->secret);
    s->ih.md5_hex(s->ih.ctx, buf, n, out);
    free(buf);
}

/* Signs p[0..np) (sorting it), POSTs it with format=json and copies the reply into `reply`. 1 when last.fm
 * answered 200 without an "error" member. */
static int signed_post(lf_state* s, param* p, unsigned np, char* reply, size_t reply_cap) {
    char sig[33];
    char* body = malloc((size_t)np * (3u * sizeof p->name + 3u * FIELD + 2u) + 64u);
    char enc[3 * FIELD + 4];
    unsigned i;
    size_t len = 0;
    int status = 0, ok = 0;
    if (body == NULL) {
        return 0;
    }
    sign(s, p, np, sig); /* sorts p */
    for (i = 0; i < np; i++) {
        pct_encode(p[i].name, enc, sizeof enc);
        len += (size_t)sprintf(body + len, "%s%s=", i == 0 ? "" : "&", enc);
        pct_encode(p[i].value, enc, sizeof enc);
        len += (size_t)sprintf(body + len, "%s", enc);
    }
    sprintf(body + len, "&api_sig=%s&format=json", sig);
    if (s->ih.http_post(s->ih.ctx, s->url, body, reply, reply_cap, &status) == 0 && status == 200 &&
        strstr(reply, "\"error\"") == NULL) {
        ok = 1;
    }
    free(body);
    return ok;
}

/* POSTs entries[0..n) as one track.scrobble. 1 when last.fm accepted the batch. */
static int post_batch(lf_state* s, const entry* en, unsigned n) {
    param* p = calloc(8u + 5u * BATCH_MAX, sizeof *p);
    char reply[2048];
    unsigned np = 0, i;
    int ok;

    if (p == NULL) {
        return 0;
    }
    add_param(p, &np, "method", "track.scrobble");
    add_param(p, &np, "api_key", s->key);
    add_param(p, &np, "sk", s->session);
    for (i = 0; i < n; i++) {
        char num[24];
        add_indexed(p, &np, "artist", i, en[i].artist);
        add_indexed(p, &np, "track", i, en[i].track);
        snprintf(num, sizeof num, "%ld", en[i].ts);
        add_indexed(p, &np, "timestamp", i, num);
        if (en[i].album[0] != '\0') {
            add_indexed(p, &np, "album", i, en[i].album);
        }
        if (en[i].dur_s != 0) {
            snprintf(num, sizeof num, "%u", en[i].dur_s);
            add_indexed(p, &np, "duration", i, num);
        }
    }
    ok = signed_post(s, p, np, reply, sizeof reply);
    if (ok) {
        s->posts++;
    }
    free(p);
    return ok;
}

/* Sends the queue oldest first in batches; whatever is not accepted stays queued with its timestamp. */
static void flush(lf_state* s) {
    unsigned n = 0, sent = 0;
    entry* all = queue_load(s, &n);
    if (all == NULL) {
        return;
    }
    while (sent < n) {
        unsigned b = n - sent < BATCH_MAX ? n - sent : BATCH_MAX;
        if (!post_batch(s, all + sent, b)) {
            s->fails++;
            s->retry_at_ms = s->host->clock_ms(s->host) + (60000u << (s->fails < 6u ? s->fails - 1u : 5u));
            break;
        }
        sent += b;
        s->fails = 0;
    }
    if (sent > 0) {
        queue_store(s, all + sent, n - sent);
    }
    s->pending = n - sent;
    free(all);
}

static void* lf_open(const sel_host* host, const sel_int_host* ih) {
    lf_state* s = calloc(1, sizeof *s);
    char home[PATH_MAX_LEN - 40];
    char def[PATH_MAX_LEN];
    unsigned n = 0;
    entry* all;
    if (s == NULL) {
        return NULL;
    }
    s->ih = *ih;
    s->host = host;
    cfg(host, "scrobble.lastfm_key", "SELECTA_LASTFM_KEY", NULL, s->key, sizeof s->key);
    cfg(host, "scrobble.lastfm_secret", "SELECTA_LASTFM_SECRET", NULL, s->secret, sizeof s->secret);
    cfg(host, "scrobble.lastfm_session", "SELECTA_LASTFM_SESSION", NULL, s->session, sizeof s->session);
    cfg(host, "scrobble.lastfm_url", "SELECTA_LASTFM_URL", "https://ws.audioscrobbler.com/2.0/", s->url,
        sizeof s->url);
    cfg(host, "scrobble.fallback_artist", "SELECTA_SCROBBLE_ARTIST", NULL, s->fallback_artist,
        sizeof s->fallback_artist);
    cfg(host, "scrobble.queue_home", "HOME", ".", home, sizeof home);
    snprintf(def, sizeof def, "%s/.selecta-scrobble.queue", home);
    cfg(host, "scrobble.queue", "SELECTA_SCROBBLE_QUEUE", def, s->queue, sizeof s->queue);
    s->enabled = s->key[0] != '\0' && s->secret[0] != '\0' && s->session[0] != '\0';
    if (s->enabled) {
        all = queue_load(s, &n);
        free(all);
        s->pending = n;
        if (n > 0) {
            flush(s); /* a queue left by an earlier run drains as soon as the network answers */
        }
    }
    return s;
}

static void lf_on_event(void* state, const sel_message* evt, const sel_state* snap) {
    lf_state* s = state;
    entry en;
    entry* all;
    unsigned n = 0;
    const char* v;
    if (evt->id != SEL_EVT_SCROBBLE_DUE) {
        return;
    }
    s->due++;
    if (!s->enabled || snap->song.meta == NULL) {
        return;
    }
    memset(&en, 0, sizeof en);
    v = (const char*)sel_tag_find(snap->song.meta, SEL_META_AUTHOR, 0);
    clean(en.artist, v != NULL && v[0] != '\0' ? v : s->fallback_artist);
    v = (const char*)sel_tag_find(snap->song.meta, SEL_META_TITLE, 0);
    clean(en.track, v);
    v = (const char*)sel_tag_find(snap->song.meta, SEL_META_RELEASE, 0);
    clean(en.album, v);
    if (en.artist[0] == '\0' || en.track[0] == '\0') {
        s->host->log(s->host, SEL_LOG_INFO, "scrobble skipped: song has no artist or title");
        return;
    }
    en.dur_s = snap->song.duration_ms / 1000u;
    en.ts = now_s() - (long)(snap->song.position_ms / 1000u); /* when the song started */
    all = queue_load(s, &n);
    if (all == NULL) {
        return;
    }
    if (n == QUEUE_MAX) {
        memmove(all, all + 1, (QUEUE_MAX - 1u) * sizeof *all);
        n--;
    }
    all[n++] = en;
    queue_store(s, all, n); /* durable before the first attempt to send */
    free(all);
    s->pending = n;
    flush(s);
}

static void lf_poll(void* state) {
    lf_state* s = state;
    if (s->enabled && s->pending > 0 && (int32_t)(s->host->clock_ms(s->host) - s->retry_at_ms) >= 0) {
        flush(s);
    }
}

static void lf_close(void* state) {
    lf_state* s = state;
    printf("scrobble_due=%u scrobble_posts=%u scrobble_pending=%u\n", s->due, s->posts, s->pending);
    free(s);
}

/* The string member `name` of a flat JSON reply ("name":"value"), escapes kept as sent. 1 when found. */
static int json_str(const char* json, const char* name, char* out, size_t cap) {
    char pat[32];
    const char* v;
    size_t n = 0;
    snprintf(pat, sizeof pat, "\"%s\":\"", name);
    v = strstr(json, pat);
    if (v == NULL) {
        return 0;
    }
    for (v += strlen(pat); *v != '\0' && *v != '"' && n + 1 < cap; v++) {
        out[n++] = *v;
    }
    out[n] = '\0';
    return n != 0;
}

/* INT_LOGIN: the desktop-application auth flow (auth.getToken, the person authorises the token on last.fm,
 * auth.getSession). Both calls are signed with the shared secret. */
static int lf_login(const sel_host* host, const sel_int_host* ih, const char* token, char* out, size_t cap) {
    lf_state s;
    param p[4];
    char reply[2048];
    char a[FIELD], b[FIELD];
    unsigned np = 0;
    memset(&s, 0, sizeof s);
    s.ih = *ih;
    cfg(host, "scrobble.lastfm_key", "SELECTA_LASTFM_KEY", NULL, s.key, sizeof s.key);
    cfg(host, "scrobble.lastfm_secret", "SELECTA_LASTFM_SECRET", NULL, s.secret, sizeof s.secret);
    cfg(host, "scrobble.lastfm_url", "SELECTA_LASTFM_URL", "https://ws.audioscrobbler.com/2.0/", s.url,
        sizeof s.url);
    if (s.key[0] == '\0' || s.secret[0] == '\0') {
        snprintf(out, cap, "no API key or secret (scrobble.lastfm_key / scrobble.lastfm_secret)");
        return 1;
    }
    add_param(p, &np, "api_key", s.key);
    add_param(p, &np, "method", token == NULL ? "auth.getToken" : "auth.getSession");
    if (token != NULL) {
        add_param(p, &np, "token", token);
    }
    if (!signed_post(&s, p, np, reply, sizeof reply)) {
        if (!json_str(reply, "message", out, cap)) {
            snprintf(out, cap, "last.fm did not answer");
        }
        return 1;
    }
    if (token == NULL) {
        if (!json_str(reply, "token", a, sizeof a)) {
            snprintf(out, cap, "no token in the reply");
            return 1;
        }
        snprintf(out, cap, "token=%s url=https://www.last.fm/api/auth/?api_key=%s&token=%s", a, s.key, a);
        return 0;
    }
    if (!json_str(reply, "key", a, sizeof a) || !json_str(reply, "name", b, sizeof b)) {
        snprintf(out, cap, "no session in the reply");
        return 1;
    }
    snprintf(out, cap, "session=%s user=%s", a, b);
    return 0;
}

static sel_tag g_hooks[7];
static sel_tag g_desc[5];

static void set(sel_tag* t, uint32_t tag, uintptr_t value) {
    t->tag = tag;
    t->value = value;
}

const sel_tag* SEL_PLUGIN_ENTRY(uint32_t host_api) {
    if (host_api < 1u) {
        return NULL;
    }
    set(&g_hooks[0], SEL_INT_NAME, (uintptr_t) "last.fm scrobbler");
    set(&g_hooks[1], SEL_INT_OPEN, SEL_TAG_FN(lf_open));
    set(&g_hooks[2], SEL_INT_ON_EVENT, SEL_TAG_FN(lf_on_event));
    set(&g_hooks[3], SEL_INT_POLL, SEL_TAG_FN(lf_poll));
    set(&g_hooks[4], SEL_INT_CLOSE, SEL_TAG_FN(lf_close));
    set(&g_hooks[5], SEL_INT_LOGIN, SEL_TAG_FN(lf_login));
    set(&g_hooks[6], SEL_TAG_END, 0);

    set(&g_desc[0], SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_INTEGRATION);
    set(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "lastfm");
    set(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    set(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    set(&g_desc[4], SEL_TAG_END, 0);
    return g_desc;
}
