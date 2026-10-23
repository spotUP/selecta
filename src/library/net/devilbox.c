/* DEViLBOX account and ratings client, see devilbox.h. Portable C over the HTTP client. */
#include "devilbox.h"

#include "db_internal.h"
#include "http.h"
#include "mirrors.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if !defined(__amigaos__) && !defined(AMIGA)
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#define BODY_CAP 32768u

static char g_dir[256];
/* The push thread (rating_push.c, I173c) and the caller both count; builds without threads count plainly (no libatomic). */
#if defined(SEL_LIBRARY_NO_THREAD)
#define COUNT_ADD(p) (*(p) += 1u)
#define COUNT_GET(p) (*(p))
#else
#define COUNT_ADD(p) __atomic_fetch_add((p), 1u, __ATOMIC_RELAXED)
#define COUNT_GET(p) __atomic_load_n((p), __ATOMIC_RELAXED)
#endif
static unsigned g_pushes;
static unsigned g_batches;

typedef struct text_buf {
    char* text;
    size_t cap;
    size_t len;
} text_buf;

static int text_sink(void* ctx, const void* data, size_t len) {
    text_buf* b = (text_buf*)ctx;

    if (len > b->cap - 1 - b->len) return 0;
    memcpy(b->text + b->len, data, len);
    b->len += len;
    b->text[b->len] = '\0';
    return 1;
}

void sel_devilbox_set_config_dir(const char* dir) {
    if (dir != NULL && strlen(dir) < sizeof g_dir) {
        strcpy(g_dir, dir);
    } else {
        g_dir[0] = '\0';
    }
}

static int token_path(char* out, size_t cap) {
    int n;

    if (g_dir[0] == '\0') return 0;
    n = snprintf(out, cap, "%s/devilbox.token", g_dir);
    return n > 0 && (size_t)n < cap;
}

int sel_devilbox_token(char* out, size_t cap) {
    char path[300];
    FILE* f;
    size_t n;

    out[0] = '\0';
    if (!token_path(path, sizeof path) || (f = fopen(path, "rb")) == NULL) return 0;
    n = fread(out, 1, cap - 1, f);
    fclose(f);
    while (n > 0 && (out[n - 1] == '\n' || out[n - 1] == '\r' || out[n - 1] == ' ')) n--;
    out[n] = '\0';
    return n > 0;
}

/* Writes the token file with mode 0600 (the permission is set before any byte is written). */
static int token_store(const char* token) {
    char path[300];
    size_t len = strlen(token);

    if (!token_path(path, sizeof path)) return 0;
#if defined(__amigaos__) || defined(AMIGA)
    {
        FILE* f = fopen(path, "wb");
        int ok = f != NULL && fwrite(token, 1, len, f) == len;

        if (f != NULL) fclose(f);
        return ok;
    }
#else
    {
        int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
        int ok;

        if (fd < 0) return 0;
        ok = fchmod(fd, 0600) == 0 && write(fd, token, len) == (ssize_t)len;
        return close(fd) == 0 && ok;
    }
#endif
}

/* Appends `s` as the inside of a JSON string. */
static size_t json_put(char* out, size_t cap, size_t n, const char* s) {
    for (; *s != '\0' && n + 7 < cap; s++) {
        unsigned char c = (unsigned char)*s;

        if (c == '"' || c == '\\') {
            out[n++] = '\\';
            out[n++] = (char)c;
        } else if (c < 0x20) {
            n += (size_t)snprintf(out + n, cap - n, "\\u%04x", c);
        } else {
            out[n++] = (char)c;
        }
    }
    return n;
}

int sel_devilbox_login(const char* user, const char* password, int* status) {
    char body[1024];
    char url[SEL_MIRROR_URL_MAX + 32];
    char reply[2048];
    char token[SEL_DEVILBOX_TOKEN_MAX];
    const char* headers[] = {"Content-Type: application/json", NULL};
    text_buf b;
    sel_http_req req;
    sel_http_res res;
    size_t n = 0;

    if (status != NULL) *status = 0;
    n = (size_t)snprintf(body, sizeof body, "{\"username\":\"");
    n = json_put(body, sizeof body, n, user);
    n += (size_t)snprintf(body + n, sizeof body - n, "\",\"password\":\"");
    n = json_put(body, sizeof body, n, password);
    if (n + 3 >= sizeof body) return 0;
    memcpy(body + n, "\"}", 3);
    n += 2;
    snprintf(url, sizeof url, "%s/api/auth/login", sel_devilbox_base());
    memset(&req, 0, sizeof req);
    memset(&res, 0, sizeof res);
    b.text = reply;
    b.cap = sizeof reply;
    b.len = 0;
    reply[0] = '\0';
    req.method = "POST";
    req.url = url;
    req.headers = headers;
    req.body = body;
    req.body_len = n;
    (void)sel_http_request(&req, text_sink, &b, &res);
    memset(body, 0, sizeof body); /* the password does not stay in memory longer than the request */
    if (status != NULL) *status = res.status;
    return res.status == 200 && sel_json_string(reply, "token", token, sizeof token) && token[0] != '\0' &&
           token_store(token);
}

int sel_devilbox_item(const char* key, char* source, size_t cap, const char** path) {
    static const char* const names[] = {"modland", "hvsc"};
    unsigned i;

    for (i = 0; i < 2; i++) {
        size_t n = strlen(names[i]);

        if (strncmp(key, names[i], n) == 0 && key[n] == ':' && key[n + 1] != '\0' && n < cap) {
            memcpy(source, names[i], n + 1);
            *path = key + n + 1;
            return 1;
        }
    }
    return 0;
}

int sel_devilbox_rating_push(const char* key, int rating) {
    char source[16];
    const char* path;
    char token[SEL_DEVILBOX_TOKEN_MAX];
    char auth[SEL_DEVILBOX_TOKEN_MAX + 32];
    char url[SEL_MIRROR_URL_MAX + 32];
    char body[SEL_MIRROR_PATH_MAX * 2 + 96];
    char reply[512];
    const char* headers[3];
    text_buf b;
    sel_http_req req;
    sel_http_res res;
    size_t n;

    if (!sel_devilbox_item(key, source, sizeof source, &path) || rating < 0 || rating > 5) return -1;
    if (!sel_devilbox_token(token, sizeof token)) return 0;
    snprintf(auth, sizeof auth, "Authorization: Bearer %s", token);
    memset(token, 0, sizeof token);
    headers[0] = auth;
    headers[1] = "Content-Type: application/json";
    headers[2] = NULL;
    n = (size_t)snprintf(body, sizeof body, "{\"source\":\"%s\",\"itemKey\":\"", source);
    n = json_put(body, sizeof body, n, path);
    if (rating > 0) {
        n += (size_t)snprintf(body + n, sizeof body - n, "\",\"rating\":%d}", rating);
    } else {
        n += (size_t)snprintf(body + n, sizeof body - n, "\"}");
    }
    if (n >= sizeof body) return -1;
    snprintf(url, sizeof url, "%s/api/ratings", sel_devilbox_base());
    memset(&req, 0, sizeof req);
    memset(&res, 0, sizeof res);
    b.text = reply;
    b.cap = sizeof reply;
    b.len = 0;
    reply[0] = '\0';
    req.method = rating > 0 ? "PUT" : "DELETE";
    req.url = url;
    req.headers = headers;
    req.body = body;
    req.body_len = n;
    (void)sel_http_request(&req, text_sink, &b, &res);
    memset(auth, 0, sizeof auth);
    if (res.status == 200) {
        COUNT_ADD(&g_pushes);
        return 1;
    }
    return res.status == 400 ? -1 : 0;
}

/* The user's own rating inside the answer for `path`: the object after `"<path>":` up to its closing brace. */
static int user_rating_of(const char* json, const char* path) {
    char needle[SEL_MIRROR_PATH_MAX * 2];
    size_t n = 0;
    const char* p;
    const char* end;

    needle[n++] = '"';
    n = json_put(needle, sizeof needle - 4, n, path);
    memcpy(needle + n, "\":{", 4);
    p = strstr(json, needle);
    if (p == NULL) return 0;
    p += n + 3;
    end = strchr(p, '}');
    p = strstr(p, "\"userRating\":");
    if (p == NULL || (end != NULL && p > end)) return 0;
    return atoi(p + 13);
}

unsigned sel_devilbox_ratings_read(const char* source, const char* const* paths, unsigned n, sel_devilbox_rating_fn fn,
                                   void* ctx) {
    char token[SEL_DEVILBOX_TOKEN_MAX];
    char auth[SEL_DEVILBOX_TOKEN_MAX + 32];
    const char* headers[2] = {NULL, NULL};
    char* url = (char*)malloc(SEL_HTTP_URL_MAX);
    char* reply = (char*)malloc(BODY_CAP);
    unsigned done = 0;
    unsigned i = 0;

    if (url == NULL || reply == NULL) {
        free(url);
        free(reply);
        return 0;
    }
    if (sel_devilbox_token(token, sizeof token)) {
        snprintf(auth, sizeof auth, "Authorization: Bearer %s", token);
        headers[0] = auth;
    }
    memset(token, 0, sizeof token);
    while (i < n) {
        size_t len = (size_t)snprintf(url, SEL_HTTP_URL_MAX, "%s/api/ratings/batch?source=%s&keys=", sel_devilbox_base(), source);
        unsigned first = i;
        text_buf b;
        sel_http_req req;
        sel_http_res res;
        unsigned k;

        while (i < n && i - first < SEL_DEVILBOX_BATCH_MAX) {
            char enc[SEL_MIRROR_PATH_MAX * 3];
            size_t el;

            if (!sel_url_encode(paths[i], enc, sizeof enc)) {
                i++; /* a path no URL can carry */
                continue;
            }
            el = strlen(enc);
            if (len + el + 2 >= SEL_HTTP_URL_MAX) break;
            if (i > first) url[len++] = ',';
            memcpy(url + len, enc, el + 1);
            len += el;
            i++;
        }
        if (i == first) break;
        memset(&req, 0, sizeof req);
        memset(&res, 0, sizeof res);
        b.text = reply;
        b.cap = BODY_CAP;
        b.len = 0;
        reply[0] = '\0';
        req.url = url;
        req.headers = headers[0] != NULL ? headers : NULL;
        COUNT_ADD(&g_batches);
        (void)sel_http_request(&req, text_sink, &b, &res);
        if (res.status != 200) break;
        done++;
        for (k = first; k < i; k++) {
            int r = user_rating_of(reply, paths[k]);

            if (r >= 1 && r <= 5 && fn != NULL) fn(ctx, k, r);
        }
    }
    memset(auth, 0, sizeof auth);
    free(url);
    free(reply);
    return done;
}

unsigned sel_devilbox_pushes(void) {
    return COUNT_GET(&g_pushes);
}

unsigned sel_devilbox_batches(void) {
    return COUNT_GET(&g_batches);
}
