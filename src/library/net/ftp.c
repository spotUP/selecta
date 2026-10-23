/* Passive FTP client; see ftp.h. */
#include "ftp.h"

#include "net.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FTP_CONNECT_MS 3000u
#define FTP_TOTAL_MS 30000u
#define FTP_LINE_CAP 512

typedef struct ftp_ctl {
    int h;
    unsigned long deadline;
    char buf[FTP_LINE_CAP]; /* unread bytes from the control connection */
    size_t have;
} ftp_ctl;

static unsigned left(unsigned long deadline) {
    unsigned long now = sel_net_now_ms();
    return now >= deadline ? 0u : (unsigned)(deadline - now);
}

static sel_ftp_err map_net(int rc) {
    if (rc == SEL_NET_E_BLOCKED) {
        return SEL_FTP_E_BLOCKED;
    }
    return rc == SEL_NET_E_TIMEOUT ? SEL_FTP_E_TIMEOUT : SEL_FTP_E_IO;
}

/* Read one reply line (without CRLF) into line. Returns SEL_FTP_OK or an error. */
static sel_ftp_err read_line(ftp_ctl* c, char* line, size_t cap) {
    for (;;) {
        char* nl = memchr(c->buf, '\n', c->have);
        int n;
        if (nl != NULL) {
            size_t len = (size_t)(nl - c->buf);
            size_t copy = len < cap - 1 ? len : cap - 1;
            memcpy(line, c->buf, copy);
            while (copy > 0 && line[copy - 1] == '\r') {
                copy--;
            }
            line[copy] = '\0';
            memmove(c->buf, nl + 1, c->have - len - 1);
            c->have -= len + 1;
            return SEL_FTP_OK;
        }
        if (c->have == sizeof c->buf) {
            c->have = 0; /* over-long line: drop it, keep reading */
        }
        n = sel_net_recv(c->h, c->buf + c->have, sizeof c->buf - c->have, left(c->deadline));
        if (n <= 0) {
            return n == SEL_NET_E_TIMEOUT ? SEL_FTP_E_TIMEOUT : SEL_FTP_E_IO;
        }
        c->have += (size_t)n;
    }
}

/* Read a full (possibly multi-line) reply. Returns its code or a negative sel_ftp_err. */
static int read_reply(ftp_ctl* c, char* text, size_t cap) {
    char line[FTP_LINE_CAP];
    int code = 0;
    for (;;) {
        sel_ftp_err e = read_line(c, line, sizeof line);
        int lc;
        if (e != SEL_FTP_OK) {
            return -(int)e;
        }
        if (strlen(line) < 4 || line[0] < '1' || line[0] > '5') {
            continue; /* continuation text inside a multi-line reply */
        }
        lc = atoi(line);
        if (code == 0) {
            code = lc;
            snprintf(text, cap, "%s", line + 4);
        }
        if (lc == code && line[3] == ' ') {
            return code;
        }
    }
}

static sel_ftp_err command(ftp_ctl* c, const char* cmd, const char* arg, int* code, char* text, size_t cap) {
    char out[FTP_LINE_CAP];
    int n = arg != NULL ? snprintf(out, sizeof out, "%s %s\r\n", cmd, arg) : snprintf(out, sizeof out, "%s\r\n", cmd);
    int rc;
    if (n <= 0 || (size_t)n >= sizeof out) {
        return SEL_FTP_E_URL;
    }
    rc = sel_net_send(c->h, out, (size_t)n, left(c->deadline));
    if (rc != 0) {
        return map_net(rc);
    }
    rc = read_reply(c, text, cap);
    if (rc < 0) {
        return (sel_ftp_err)(-rc);
    }
    *code = rc;
    return SEL_FTP_OK;
}

/* Parse "(h1,h2,h3,h4,p1,p2)" from a 227 text. */
static int parse_pasv(const char* text, char* addr, size_t cap, unsigned* port) {
    unsigned v[6];
    const char* p = strchr(text, '(');
    int i;
    if (p == NULL) {
        return 0;
    }
    p++;
    for (i = 0; i < 6; i++) {
        char* end;
        unsigned long x = strtoul(p, &end, 10);
        if (end == p || x > 255) {
            return 0;
        }
        v[i] = (unsigned)x;
        p = end;
        if (i < 5) {
            if (*p != ',') {
                return 0;
            }
            p++;
        }
    }
    snprintf(addr, cap, "%u.%u.%u.%u", v[0], v[1], v[2], v[3]);
    *port = v[4] * 256u + v[5];
    return 1;
}

static unsigned connect_ms(unsigned long deadline) {
    unsigned l = left(deadline);
    return FTP_CONNECT_MS < l ? FTP_CONNECT_MS : l;
}

sel_ftp_err sel_ftp_get(const char* url, unsigned total_ms, sel_ftp_sink sink, void* ctx, sel_ftp_res* res) {
    char host[128];
    char path[256];
    char text[FTP_LINE_CAP];
    char addr[32];
    unsigned port = 21, dport = 0;
    const char* p;
    const char* slash;
    ftp_ctl c;
    sel_ftp_err err;
    int code = 0, rc, dh = -1;
    unsigned long start = sel_net_now_ms();
    long want = -1;
    unsigned long got = 0;

    memset(res, 0, sizeof *res);
    res->size = -1;
    if (strncmp(url, "ftp://", 6) != 0) {
        return SEL_FTP_E_URL;
    }
    p = url + 6;
    slash = strchr(p, '/');
    if (slash == NULL || slash == p || (size_t)(slash - p) >= sizeof host || strlen(slash) >= sizeof path) {
        return SEL_FTP_E_URL;
    }
    memcpy(host, p, (size_t)(slash - p));
    host[slash - p] = '\0';
    snprintf(path, sizeof path, "%s", slash);
    {
        char* colon = strchr(host, ':');
        if (colon != NULL) {
            *colon = '\0';
            port = (unsigned)strtoul(colon + 1, NULL, 10);
        }
    }
    memset(&c, 0, sizeof c);
    c.deadline = start + (total_ms != 0 ? total_ms : FTP_TOTAL_MS);
    c.h = sel_net_open(host, port, connect_ms(c.deadline));
    if (c.h < 0) {
        return c.h == SEL_NET_E_BLOCKED ? SEL_FTP_E_BLOCKED : SEL_FTP_E_CONNECT;
    }
    rc = read_reply(&c, text, sizeof text);
    if (rc < 0 || rc != 220) {
        err = rc < 0 ? (sel_ftp_err)(-rc) : SEL_FTP_E_LOGIN;
        goto done;
    }
    if ((err = command(&c, "USER", "anonymous", &code, text, sizeof text)) != SEL_FTP_OK) {
        goto done;
    }
    if (code == 331 && (err = command(&c, "PASS", "selecta@localhost", &code, text, sizeof text)) != SEL_FTP_OK) {
        goto done;
    }
    if (code != 230) {
        err = SEL_FTP_E_LOGIN;
        goto done;
    }
    if ((err = command(&c, "TYPE", "I", &code, text, sizeof text)) != SEL_FTP_OK) {
        goto done;
    }
    if ((err = command(&c, "SIZE", path, &code, text, sizeof text)) != SEL_FTP_OK) {
        goto done;
    }
    if (code == 213) {
        want = atol(text);
        res->size = want;
    } else if (code == 550) {
        err = SEL_FTP_E_NO_SUCH_FILE;
        goto done;
    }
    if ((err = command(&c, "PASV", NULL, &code, text, sizeof text)) != SEL_FTP_OK) {
        goto done;
    }
    if (code != 227 || !parse_pasv(text, addr, sizeof addr, &dport)) {
        err = SEL_FTP_E_PASV;
        goto done;
    }
    res->passive++;
    dh = sel_net_open(addr, dport, connect_ms(c.deadline));
    if (dh < 0) {
        err = dh == SEL_NET_E_BLOCKED ? SEL_FTP_E_BLOCKED : SEL_FTP_E_DATA;
        dh = -1;
        goto done;
    }
    if ((err = command(&c, "RETR", path, &code, text, sizeof text)) != SEL_FTP_OK) {
        goto done;
    }
    if (code == 550) {
        err = SEL_FTP_E_NO_SUCH_FILE;
        goto done;
    }
    if (code != 150 && code != 125) {
        err = SEL_FTP_E_DATA;
        goto done;
    }
    for (;;) {
        char buf[4096];
        int n = sel_net_recv(dh, buf, sizeof buf, left(c.deadline));
        if (n > 0) {
            got += (unsigned long)n;
            if (!sink(ctx, buf, (size_t)n)) {
                err = SEL_FTP_E_SINK;
                goto done;
            }
        } else if (n == SEL_NET_E_CLOSED) {
            break;
        } else {
            err = n == SEL_NET_E_TIMEOUT ? SEL_FTP_E_TIMEOUT : SEL_FTP_E_IO;
            goto done;
        }
    }
    res->body_bytes = got;
    sel_net_close(dh);
    dh = -1;
    rc = read_reply(&c, text, sizeof text);
    if (rc < 0) {
        err = (sel_ftp_err)(-rc);
    } else if (rc != 226 || (want >= 0 && (unsigned long)want != got)) {
        err = SEL_FTP_E_DATA;
    } else {
        res->retr++;
        err = SEL_FTP_OK;
        (void)sel_net_send(c.h, "QUIT\r\n", 6, 200u);
    }
done:
    if (dh >= 0) {
        sel_net_close(dh);
    }
    sel_net_close(c.h);
    res->elapsed_ms = sel_net_now_ms() - start;
    return err;
}

const char* sel_ftp_err_name(sel_ftp_err e) {
    switch (e) {
    case SEL_FTP_OK: return "ok";
    case SEL_FTP_E_URL: return "bad_url";
    case SEL_FTP_E_BLOCKED: return "net_blocked";
    case SEL_FTP_E_CONNECT: return "connect_failed";
    case SEL_FTP_E_LOGIN: return "login_failed";
    case SEL_FTP_E_NO_SUCH_FILE: return "no_such_file";
    case SEL_FTP_E_PASV: return "pasv_failed";
    case SEL_FTP_E_DATA: return "data_failed";
    case SEL_FTP_E_TIMEOUT: return "timeout";
    case SEL_FTP_E_IO: return "io_error";
    case SEL_FTP_E_SINK: return "sink_aborted";
    }
    return "unknown";
}
