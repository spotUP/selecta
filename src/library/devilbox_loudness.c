/* DEViLBOX loudness endpoint client (I188). See devilbox_loudness.h. */
#include "devilbox_loudness.h"

#include "md5.h"
#include "net/http.h"
#include "net/mirrors.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define REPLY_MAX 512u

typedef struct reply {
    char text[REPLY_MAX];
    size_t len;
} reply;

static int reply_sink(void* ctx, const void* data, size_t len) {
    reply* r = (reply*)ctx;

    if (r->len + len >= sizeof r->text) return 0;
    memcpy(r->text + r->len, data, len);
    r->len += len;
    r->text[r->len] = '\0';
    return 1;
}

int sel_dvb_loudness_file_hash(const char* path, char hex[33]) {
    FILE* f = fopen(path, "rb");
    sel_md5 c;
    unsigned char buf[4096];
    size_t n;

    if (f == NULL) return 0;
    sel_md5_init(&c);
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) sel_md5_update(&c, buf, n);
    fclose(f);
    sel_md5_hex(&c, hex);
    return 1;
}

/* Number after `"name":` in hundredths, rounded half away from zero. Returns 1 when found. */
static int json_centi(const char* json, const char* name, int32_t* out) {
    char pat[24];
    const char* p;
    int neg = 0;
    long whole = 0, frac = 0;
    int digits = 0, third = 0;

    snprintf(pat, sizeof pat, "\"%s\"", name);
    p = strstr(json, pat);
    if (p == NULL) return 0;
    p += strlen(pat);
    while (*p == ' ' || *p == ':') p++;
    if (*p == '-') {
        neg = 1;
        p++;
    }
    if (*p < '0' || *p > '9') return 0;
    while (*p >= '0' && *p <= '9') whole = whole * 10 + (*p++ - '0');
    if (*p == '.') {
        for (p++; *p >= '0' && *p <= '9'; p++) {
            if (digits < 2) {
                frac = frac * 10 + (*p - '0');
            } else if (digits == 2) {
                third = *p - '0';
            }
            digits++;
        }
    }
    while (digits < 2) {
        frac *= 10;
        digits++;
    }
    frac += third >= 5 ? 1 : 0;
    whole = whole * 100 + frac;
    *out = (int32_t)(neg ? -whole : whole);
    return 1;
}

sel_dvb_loudness_status sel_dvb_loudness_get(const char* hash, sel_dvb_loudness* out, unsigned* requests) {
    char url[256];
    reply r;
    sel_http_req req;
    sel_http_res res;
    sel_http_err err;
    int32_t n = 0;

    memset(&req, 0, sizeof req);
    memset(&res, 0, sizeof res);
    memset(&r, 0, sizeof r);
    snprintf(url, sizeof url, "%s/api/loudness/%s", sel_devilbox_base(), hash);
    req.url = url;
    err = sel_http_request(&req, reply_sink, &r, &res);
    if (requests != NULL) *requests += res.requests;
    if (err != SEL_HTTP_OK) return SEL_DVB_LOUDNESS_MISSING;
    if (res.status == 404 && strstr(r.text, "unknown hash") != NULL) return SEL_DVB_LOUDNESS_UNKNOWN;
    if (res.status != 200) return SEL_DVB_LOUDNESS_MISSING;
    memset(out, 0, sizeof *out);
    if (!json_centi(r.text, "lufs", &out->lufs_cdb)) return SEL_DVB_LOUDNESS_MISSING;
    if (!json_centi(r.text, "peakDb", &out->peak_cdb)) out->peak_cdb = 0;
    out->r128 = strstr(r.text, "\"r128\"") != NULL;
    if (json_centi(r.text, "count", &n)) out->count = n / 100; /* json_centi returns hundredths */
    return SEL_DVB_LOUDNESS_HIT;
}

static void centi_text(char* out, size_t cap, int32_t cdb) {
    int32_t a = cdb < 0 ? -cdb : cdb;

    snprintf(out, cap, "%s%d.%02d", cdb < 0 ? "-" : "", (int)(a / 100), (int)(a % 100));
}

int sel_dvb_loudness_put(const char* hash, const char* token, const sel_dvb_loudness* in, const char* analyser) {
    char url[256], lufs[24], peak[24], body[256], auth[1100];
    const char* headers[3];
    reply r;
    sel_http_req req;
    sel_http_res res;
    int n;

    memset(&req, 0, sizeof req);
    memset(&res, 0, sizeof res);
    memset(&r, 0, sizeof r);
    snprintf(url, sizeof url, "%s/api/loudness/%s", sel_devilbox_base(), hash);
    centi_text(lufs, sizeof lufs, in->lufs_cdb);
    centi_text(peak, sizeof peak, in->peak_cdb);
    n = snprintf(body, sizeof body, "{\"lufs\":%s,\"peakDb\":%s,\"method\":\"%s\",\"analyser\":\"%s\"}", lufs, peak,
                 in->r128 ? "r128" : "rms", analyser);
    snprintf(auth, sizeof auth, "Authorization: Bearer %s", token);
    headers[0] = auth;
    headers[1] = "Content-Type: application/json";
    headers[2] = NULL;
    req.method = "PUT";
    req.url = url;
    req.headers = headers;
    req.body = body;
    req.body_len = (size_t)n;
    if (sel_http_request(&req, reply_sink, &r, &res) != SEL_HTTP_OK) return 0;
    return res.status;
}

int sel_dvb_loudness_token_read(const char* config_dir, char* out, size_t cap) {
    char path[1024];
    FILE* f;
    size_t n;

    if (config_dir == NULL || cap < 2) return 0;
    snprintf(path, sizeof path, "%s/devilbox.token", config_dir);
    f = fopen(path, "rb");
    if (f == NULL) return 0;
    n = fread(out, 1, cap - 1, f);
    fclose(f);
    while (n > 0 && (out[n - 1] == '\n' || out[n - 1] == '\r' || out[n - 1] == ' ' || out[n - 1] == '\t')) n--;
    out[n] = '\0';
    return n > 0;
}

int32_t sel_dvb_loudness_gain_cdb(int32_t lufs_cdb) {
    int32_t g = SEL_DVB_LOUDNESS_TARGET_CDB - lufs_cdb;

    if (lufs_cdb <= -7000) return 0; /* silence: no gain, as the analyser */

    if (g > SEL_DVB_LOUDNESS_GAIN_LIMIT_CDB) g = SEL_DVB_LOUDNESS_GAIN_LIMIT_CDB;
    if (g < -SEL_DVB_LOUDNESS_GAIN_LIMIT_CDB) g = -SEL_DVB_LOUDNESS_GAIN_LIMIT_CDB;
    return g;
}
