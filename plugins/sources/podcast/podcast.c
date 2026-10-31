/* The `podcast` source descriptor (row I725f; plan 2026-10-30-open-streaming-sources, row 7): podcast episodes from
 * an RSS 2.0 feed (the open standard, https://www.rssboard.org/rss-specification). Each <item> with an <enclosure
 * url=".." type="audio/.."> is one episode; title, pubDate, itunes:duration and the feed image are read from it.
 *
 * Podcast Index (https://api.podcastindex.org/) search: needs a free key and secret from https://api.podcastindex.org/.
 * Every request carries X-Auth-Key, X-Auth-Date and Authorization = SHA-1(key + secret + date) in hex, and a descriptive
 * User-Agent. The Podcast Index terms of use could NOT be retrieved when this was written (the site answered 403 to
 * the fetch tool): the terms are UNVERIFIED and no line from them is quoted. Selecta only sends the documented
 * authentication headers and the descriptive User-Agent, makes one search request per sync, stores no key in the source
 * tree, and does not cache or redistribute audio.
 *
 * Configuration: prefs `sources.podcast_index_key` and `sources.podcast_index_secret`, else the environment
 * SELECTA_PODCAST_INDEX_KEY and SELECTA_PODCAST_INDEX_SECRET. Without them a `search:` sync prints "podcast: no Podcast
 * Index key, search off" on stderr and does nothing; subscribing by feed URL needs no key. SELECTA_PODCAST_INDEX_API
 * replaces the API base (default https://api.podcastindex.org; tests point it at a fixture).
 *
 * A "sync" takes one argument: a feed URL (the feed's episodes become the source's rows) or `search:<term>` (Podcast
 * Index /api/1.0/search/byterm, the first matching feed is then read like a feed URL). A song key is
 * `podcast:<show>/<episode>/<enclosure url>`; play_url returns the enclosure URL and the engine streams it with HTTP
 * Range (SEL_SOURCE_SEEKABLE), so seeking works. Nothing is written to the library or cache dir. */
#include <selecta/catalogue.h>
#include <selecta/host.h>
#include <selecta/net.h>
#include <selecta/plugin.h>
#include <selecta/source.h>
#include "source/descriptor.h"
#include "source/sync_stat.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define DEFAULT_API "https://api.podcastindex.org"
#define USER_AGENT "Selecta-Podcast/1 (+https://github.com/spotUP/Selecta)"
#define FEED_CAP (2u * 1024u * 1024u)
#define URL_BYTES 1100u
#define TEXT_BYTES 160u

static const sel_tag g_tags[] = {
    {SEL_TAG_END, 0},
};

/* ---- SHA-1 (FIPS 180-1): only for the Podcast Index Authorization header ---- */
typedef struct sha1 {
    uint32_t h[5];
    uint64_t len;
    unsigned char buf[64];
    size_t fill;
} sha1;

static uint32_t rol(uint32_t v, unsigned n) {
    return (v << n) | (v >> (32u - n));
}

static void sha1_block(sha1* s, const unsigned char* p) {
    uint32_t w[80], a, b, c, d, e, t;
    int i;

    for (i = 0; i < 16; i++) {
        w[i] = ((uint32_t)p[i * 4] << 24) | ((uint32_t)p[i * 4 + 1] << 16) | ((uint32_t)p[i * 4 + 2] << 8) | p[i * 4 + 3];
    }
    for (i = 16; i < 80; i++) {
        w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    }
    a = s->h[0];
    b = s->h[1];
    c = s->h[2];
    d = s->h[3];
    e = s->h[4];
    for (i = 0; i < 80; i++) {
        uint32_t f, k;

        if (i < 20) {
            f = (b & c) | (~b & d);
            k = 0x5A827999u;
        } else if (i < 40) {
            f = b ^ c ^ d;
            k = 0x6ED9EBA1u;
        } else if (i < 60) {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8F1BBCDCu;
        } else {
            f = b ^ c ^ d;
            k = 0xCA62C1D6u;
        }
        t = rol(a, 5) + f + e + k + w[i];
        e = d;
        d = c;
        c = rol(b, 30);
        b = a;
        a = t;
    }
    s->h[0] += a;
    s->h[1] += b;
    s->h[2] += c;
    s->h[3] += d;
    s->h[4] += e;
}

static void sha1_init(sha1* s) {
    s->h[0] = 0x67452301u;
    s->h[1] = 0xEFCDAB89u;
    s->h[2] = 0x98BADCFEu;
    s->h[3] = 0x10325476u;
    s->h[4] = 0xC3D2E1F0u;
    s->len = 0;
    s->fill = 0;
}

static void sha1_add(sha1* s, const void* data, size_t n) {
    const unsigned char* p = (const unsigned char*)data;

    s->len += n;
    while (n > 0) {
        size_t take = 64u - s->fill < n ? 64u - s->fill : n;

        memcpy(s->buf + s->fill, p, take);
        s->fill += take;
        p += take;
        n -= take;
        if (s->fill == 64u) {
            sha1_block(s, s->buf);
            s->fill = 0;
        }
    }
}

static void sha1_hex(sha1* s, char out[41]) {
    static const char hex[] = "0123456789abcdef";
    uint64_t bits = s->len * 8u;
    unsigned char pad = 0x80;
    unsigned char lenb[8];
    int i;

    sha1_add(s, &pad, 1);
    pad = 0;
    while (s->fill != 56u) {
        sha1_add(s, &pad, 1);
    }
    for (i = 0; i < 8; i++) {
        lenb[i] = (unsigned char)(bits >> (56 - 8 * i));
    }
    sha1_add(s, lenb, 8);
    for (i = 0; i < 20; i++) {
        unsigned char b = (unsigned char)(s->h[i / 4] >> (24 - 8 * (i % 4)));

        out[i * 2] = hex[b >> 4];
        out[i * 2 + 1] = hex[b & 15u];
    }
    out[40] = '\0';
}

/* ---- RSS scan: a small tag walker, enough for <item> fields; not a general XML parser ---- */
typedef struct episode {
    char title[TEXT_BYTES];
    char url[URL_BYTES - 64u];
    char date[48];
    char duration[24];
} episode;

/* Copies the text between `<tag ...>` and `</tag>` inside [s, end) to out, decoding CDATA and the five XML entities. */
static int tag_text(const char* s, const char* end, const char* tag, char* out, size_t n) {
    char open[32];
    const char* p = s;
    size_t ol = (size_t)snprintf(open, sizeof open, "<%s", tag), o = 0;

    while ((p = memchr(p, '<', (size_t)(end - p))) != NULL) {
        if ((size_t)(end - p) > ol && strncmp(p, open, ol) == 0 && (p[ol] == '>' || p[ol] == ' ' || p[ol] == '/')) {
            break;
        }
        p++;
    }
    if (p == NULL || (p = memchr(p, '>', (size_t)(end - p))) == NULL || p[-1] == '/') {
        return 0;
    }
    for (p++; p < end && o + 1 < n;) {
        if (strncmp(p, "</", 2) == 0 && (size_t)(end - p) >= 2u) {
            break;
        }
        if ((size_t)(end - p) >= 9u && strncmp(p, "<![CDATA[", 9) == 0) {
            p += 9;
            while (p < end && strncmp(p, "]]>", 3) != 0 && o + 1 < n) {
                out[o++] = *p++;
            }
            p += p < end ? 3 : 0;
        } else if (*p == '&') {
            static const struct {
                const char* e;
                char c;
            } ent[] = {{"&amp;", '&'}, {"&lt;", '<'}, {"&gt;", '>'}, {"&quot;", '"'}, {"&apos;", '\''}};
            size_t i;
            int hit = 0;

            for (i = 0; i < 5 && !hit; i++) {
                size_t l = strlen(ent[i].e);

                if ((size_t)(end - p) >= l && strncmp(p, ent[i].e, l) == 0) {
                    out[o++] = ent[i].c;
                    p += l;
                    hit = 1;
                }
            }
            if (!hit) {
                out[o++] = *p++;
            }
        } else {
            out[o++] = *p++;
        }
    }
    out[o] = '\0';
    while (o > 0 && isspace((unsigned char)out[o - 1])) {
        out[--o] = '\0';
    }
    return o > 0;
}

/* The value of attribute `attr` of the first `<tag ...>` in [s, end). */
static int tag_attr(const char* s, const char* end, const char* tag, const char* attr, char* out, size_t n) {
    char open[32], needle[24];
    const char* p = s;
    const char* close;
    size_t ol = (size_t)snprintf(open, sizeof open, "<%s ", tag), o = 0, nl = (size_t)snprintf(needle, sizeof needle, "%s=\"", attr);

    while ((p = memchr(p, '<', (size_t)(end - p))) != NULL) {
        if ((size_t)(end - p) > ol && strncmp(p, open, ol) == 0) {
            break;
        }
        p++;
    }
    if (p == NULL || (close = memchr(p, '>', (size_t)(end - p))) == NULL) {
        return 0;
    }
    for (p += ol; p + nl < close; p++) {
        if (strncmp(p, needle, nl) == 0 && (p[-1] == ' ' || p[-1] == '\n' || p[-1] == '\t')) {
            p += nl;
            while (p < close && *p != '"' && o + 1 < n) {
                if (strncmp(p, "&amp;", 5) == 0) {
                    out[o++] = '&';
                    p += 5;
                } else {
                    out[o++] = *p++;
                }
            }
            out[o] = '\0';
            return o > 0;
        }
    }
    return 0;
}

static void clean_part(char* s) {
    for (; *s != '\0'; s++) {
        if (*s == '/' || *s == '|' || (unsigned char)*s < 32u) {
            *s = ' ';
        }
    }
}

typedef struct run {
    const sel_host* host;
    const sel_catalogue* cat;
    char show[TEXT_BYTES];
    uint32_t rows, skipped;
} run;

static void add_episode(run* r, const char* item, const char* end) {
    episode e;
    char path[URL_BYTES + 2u * TEXT_BYTES], url[URL_BYTES - 64u], format[96];
    sel_catalogue_song song;
    size_t i, j = 0;

    memset(&e, 0, sizeof e);
    tag_text(item, end, "title", e.title, sizeof e.title);
    tag_text(item, end, "pubDate", e.date, sizeof e.date);
    tag_text(item, end, "itunes:duration", e.duration, sizeof e.duration);
    if (!tag_attr(item, end, "enclosure", "url", e.url, sizeof e.url) || e.title[0] == '\0') {
        r->skipped++;
        return;
    }
    for (i = 0; e.url[i] != '\0' && j + 4u < sizeof url; i++) { /* '|' separates catalogue fields: escape it */
        if (e.url[i] == '|') {
            j += (size_t)snprintf(url + j, sizeof url - j, "%%7C");
        } else {
            url[j++] = e.url[i];
        }
    }
    url[j] = '\0';
    clean_part(e.title);
    snprintf(path, sizeof path, "%s/%s/%s", r->show, e.title, url);
    snprintf(format, sizeof format, "Podcast%s%s", e.date[0] != '\0' ? " - " : "", e.date);
    memset(&song, 0, sizeof song);
    song.path = path;
    song.format = format;
    song.artist = r->show;
    song.album = "";
    song.name = e.title;
    song.md5 = "";
    if (!r->cat->song(r->host, &song)) {
        r->skipped++;
        return;
    }
    r->rows++;
}

static void read_feed(run* r, const char* xml, size_t n) {
    const char* end = xml + n;
    const char* first = strstr(xml, "<item");
    const char* p = first;

    if (!tag_text(xml, first != NULL ? first : end, "title", r->show, sizeof r->show)) {
        snprintf(r->show, sizeof r->show, "Podcast");
    }
    clean_part(r->show);
    while (p != NULL && (p = strstr(p, "<item")) != NULL) {
        const char* close = strstr(p, "</item>");

        if (close == NULL) {
            break;
        }
        if (p[5] == '>' || p[5] == ' ') {
            add_episode(r, p, close);
        }
        p = close + 7;
    }
}

static const char* api_base(void) {
    const char* env = getenv("SELECTA_PODCAST_INDEX_API");

    return env != NULL && env[0] != '\0' ? env : DEFAULT_API;
}

static const char* secret_of(const sel_host* host, const char* pref, const char* envname) {
    const char* v = host->config_get != NULL ? host->config_get(host, pref, NULL) : NULL;

    return v != NULL && v[0] != '\0' ? v : getenv(envname);
}

static void url_encode(const char* s, char* out, size_t n) {
    size_t o = 0;

    for (; *s != '\0' && o + 4u < n; s++) {
        if (isalnum((unsigned char)*s) || *s == '-' || *s == '_' || *s == '.') {
            out[o++] = *s;
        } else {
            o += (size_t)snprintf(out + o, n - o, "%%%02X", (unsigned char)*s);
        }
    }
    out[o] = '\0';
}

/* Podcast Index search: the first result feed's "url" value. 0 when there is no key or no hit. */
static int search_feed(const sel_host* host, const sel_net* net, const char* term, char* feed, size_t n, uint32_t* rq,
                       unsigned long* bytes, int64_t now) {
    const char* key = secret_of(host, "sources.podcast_index_key", "SELECTA_PODCAST_INDEX_KEY");
    const char* secret = secret_of(host, "sources.podcast_index_secret", "SELECTA_PODCAST_INDEX_SECRET");
    char date[24], hash[41], h1[160], h2[64], h3[96], q[300], url[600];
    const char* hdr[4];
    char* buf;
    uint32_t got = 0;
    sha1 s;
    int status, ok = 0;
    const char* u;
    size_t i;

    if (key == NULL || key[0] == '\0' || secret == NULL || secret[0] == '\0') {
        fputs("podcast: no Podcast Index key, search off (set prefs sources.podcast_index_key and "
              "sources.podcast_index_secret; free key at https://api.podcastindex.org/)\n",
              stderr);
        return 0;
    }
    snprintf(date, sizeof date, "%lld", (long long)(now > 0 ? now : (int64_t)time(NULL)));
    sha1_init(&s);
    sha1_add(&s, key, strlen(key));
    sha1_add(&s, secret, strlen(secret));
    sha1_add(&s, date, strlen(date));
    sha1_hex(&s, hash);
    snprintf(h1, sizeof h1, "X-Auth-Key: %s", key);
    snprintf(h2, sizeof h2, "X-Auth-Date: %s", date);
    snprintf(h3, sizeof h3, "Authorization: %s", hash);
    hdr[0] = h1;
    hdr[1] = h2;
    hdr[2] = h3;
    hdr[3] = NULL;
    url_encode(term, q, sizeof q);
    snprintf(url, sizeof url, "%s/api/1.0/search/byterm?q=%s", api_base(), q);
    buf = (char*)malloc(256u * 1024u);
    if (buf == NULL) {
        return 0;
    }
    status = net->request_ex(host, url, USER_AGENT, hdr, buf, 256u * 1024u - 1u, &got, rq);
    *bytes += got;
    if (status == 200) {
        buf[got] = '\0';
        u = strstr(buf, "\"url\"");
        if (u != NULL) {
            for (u += 5; *u == ' ' || *u == ':'; u++) {
            }
            for (u += *u == '"' ? 1 : 0, i = 0; *u != '\0' && *u != '"' && i + 1 < n; u++) {
                if (u[0] == '\\' && u[1] == '/') {
                    continue;
                }
                feed[i++] = *u;
            }
            feed[i] = '\0';
            ok = i > 0;
        }
    }
    free(buf);
    return ok;
}

static int pc_sync(const sel_host* host, const char* arg, int64_t now, sel_tag* stats, uint32_t cap) {
    const sel_net* net = host->net != NULL ? host->net(host) : NULL;
    const sel_catalogue* cat = host->catalogue != NULL ? host->catalogue(host) : NULL;
    char feed[URL_BYTES];
    uint32_t rq = 0, got = 0;
    unsigned long bytes = 0;
    char* buf;
    run r;
    int status, ok = 0;

    if (net == NULL || net->request_ex == NULL || cat == NULL || arg == NULL) {
        return 0;
    }
    if (strncmp(arg, "search:", 7) == 0) {
        if (!search_feed(host, net, arg + 7, feed, sizeof feed, &rq, &bytes, now)) {
            return 0;
        }
    } else if (strlen(arg) < sizeof feed) {
        snprintf(feed, sizeof feed, "%s", arg);
    } else {
        return 0;
    }
    memset(&r, 0, sizeof r);
    r.host = host;
    r.cat = cat;
    buf = (char*)malloc(FEED_CAP);
    if (buf == NULL || !cat->begin(host, "podcast")) {
        free(buf);
        return 0;
    }
    status = net->request_ex(host, feed, USER_AGENT, NULL, buf, FEED_CAP - 1u, &got, &rq);
    bytes += got;
    if (status == 200) {
        buf[got] = '\0';
        read_feed(&r, buf, got);
        ok = 1;
    }
    free(buf);
    if (ok) {
        ok = cat->commit(host);
    } else {
        cat->abort(host);
    }
    sel_sync_stat(stats, cap, SEL_SYNC_LISTING_ROWS, r.rows);
    sel_sync_stat(stats, cap, SEL_SYNC_SKIPPED_ROWS, r.skipped);
    sel_sync_stat(stats, cap, SEL_SYNC_NET_BYTES, (uint32_t)bytes);
    sel_sync_stat(stats, cap, SEL_SYNC_NET_REQUESTS, rq);
    return ok;
}

/* The enclosure URL is everything after the second '/'. */
static int pc_play_url(const sel_host* host, const char* path, char* out, size_t n) {
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
    static const char fmt[] = "Podcast";
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

static const sel_source g_podcast = {
    "podcast", "Podcast", "https://podcastindex.org/",
    "Podcast episodes from an RSS feed URL or Podcast Index search (search needs your own free key).",
    g_tags, NULL, parse, NULL, NULL, NULL, NULL, NULL, pc_sync,
    SEL_SOURCE_REMOTE | SEL_SOURCE_SYNCS | SEL_SOURCE_STREAMS | SEL_SOURCE_SEEKABLE, "https://podcastindex.org/", pc_play_url,
    NULL, /* search (I406) */
};

const sel_tag* SEL_PLUGIN_ENTRY(uint32_t host_api) {
    (void)host_api;
    return sel_source_plugin_descriptor("podcast", &g_podcast);
}
