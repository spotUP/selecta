/* `selecta-cli fetch <url>` (T67, T68): one HTTP or passive FTP request through the library's HTTP
 * client and socket table, the same path the sources take. Prints counters as
 * `name=value` lines; exit 0 when a response arrived, 1 on a client error, 2 on usage. */
#include "cli.h"
#include "net/ftp.h"
#include "net/http.h"
#include "net/net.h"
#include "net/tls.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FETCH_MAX_HEADERS 8

typedef struct fetch_sink {
    FILE* out;
} fetch_sink;

static int on_body(void* ctx, const void* data, size_t len) {
    fetch_sink* s = ctx;
    return s->out == NULL || fwrite(data, 1, len, s->out) == len;
}

static unsigned arg_ms(const char* s) {
    return (unsigned)strtoul(s, NULL, 10);
}

static void* read_file(const char* path, size_t* len) {
    FILE* f = fopen(path, "rb");
    long n;
    void* buf;
    if (f == NULL) {
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    buf = malloc(n > 0 ? (size_t)n : 1u);
    if (buf == NULL || (n > 0 && fread(buf, 1, (size_t)n, f) != (size_t)n)) {
        free(buf);
        fclose(f);
        return NULL;
    }
    fclose(f);
    *len = (size_t)n;
    return buf;
}

int sel_cli_fetch(int argc, char** argv) {
    sel_http_req req;
    sel_http_res res;
    sel_net_stats ns;
    sel_http_err err;
    fetch_sink sink;
    const char* headers[FETCH_MAX_HEADERS + 1];
    const char* out_path = NULL;
    void* body = NULL;
    int nh = 0, i;

    memset(&req, 0, sizeof req);
    sink.out = NULL;
    if (argc < 1 || argv[0][0] == '-') {
        fputs("usage: selecta-cli fetch <url> [--method M] [--range a-b] [--header 'N: v'] [--data file] [--out file]\n"
              "                    [--connect-ms n] [--first-byte-ms n] [--total-ms n]\n",
              stderr);
        return 2;
    }
    req.url = argv[0];
    for (i = 1; i < argc; i += 2) {
        const char* a = argv[i];
        const char* v = i + 1 < argc ? argv[i + 1] : NULL;
        if (v == NULL) {
            fprintf(stderr, "selecta-cli: fetch: %s needs a value\n", a);
            return 2;
        }
        if (strcmp(a, "--method") == 0) {
            req.method = v;
        } else if (strcmp(a, "--range") == 0) {
            char* dash;
            req.has_range = 1;
            req.range_from = strtoul(v, &dash, 10);
            req.range_to = *dash == '-' && dash[1] != '\0' ? (long)strtoul(dash + 1, NULL, 10) : -1;
        } else if (strcmp(a, "--header") == 0 && nh < FETCH_MAX_HEADERS) {
            headers[nh++] = v;
        } else if (strcmp(a, "--data") == 0) {
            body = read_file(v, &req.body_len);
            if (body == NULL) {
                fprintf(stderr, "selecta-cli: fetch: cannot read %s\n", v);
                return 2;
            }
            req.body = body;
        } else if (strcmp(a, "--out") == 0) {
            out_path = v;
        } else if (strcmp(a, "--connect-ms") == 0) {
            req.connect_ms = arg_ms(v);
        } else if (strcmp(a, "--first-byte-ms") == 0) {
            req.first_byte_ms = arg_ms(v);
        } else if (strcmp(a, "--total-ms") == 0) {
            req.total_ms = arg_ms(v);
        } else {
            fprintf(stderr, "selecta-cli: fetch: unknown option %s\n", a);
            return 2;
        }
    }
    headers[nh] = NULL;
    req.headers = headers;
    if (out_path != NULL) {
        sink.out = fopen(out_path, "wb");
        if (sink.out == NULL) {
            fprintf(stderr, "selecta-cli: fetch: cannot write %s\n", out_path);
            free(body);
            return 2;
        }
    }
    if (strncmp(req.url, "ftp://", 6) == 0) {
        sel_ftp_res fr;
        sel_ftp_err fe = sel_ftp_get(req.url, req.total_ms, on_body, &sink, &fr);
        if (sink.out != NULL) {
            fclose(sink.out);
        }
        free(body);
        sel_net_stats_get(&ns);
        printf("ftp_error=%s\nftp_retr=%u\nftp_size=%ld\nftp_passive=%u\nbody_bytes=%lu\nelapsed_ms=%lu\n"
               "net_blocked=%u\nnet_mapped=%u\n",
               sel_ftp_err_name(fe), fr.retr, fr.size, fr.passive, fr.body_bytes, fr.elapsed_ms, ns.blocked, ns.mapped);
        return fe == SEL_FTP_OK ? 0 : 1;
    }
    err = sel_http_request(&req, on_body, &sink, &res);
    if (sink.out != NULL) {
        fclose(sink.out);
    }
    free(body);
    sel_net_stats_get(&ns);
    printf("http_status=%d\nhttp_error=%s\nhttp_requests=%u\nredirects=%u\nbody_bytes=%lu\ncontent_length=%ld\n"
           "elapsed_ms=%lu\ntls_handshakes=%u\nnet_blocked=%u\nnet_mapped=%u\n",
           res.status, sel_http_err_name(err), res.requests, res.redirects, res.body_bytes, res.content_length,
           res.elapsed_ms, sel_tls_handshakes(), ns.blocked, ns.mapped);
    return err == SEL_HTTP_OK ? 0 : 1;
}
