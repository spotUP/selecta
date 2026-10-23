/* Passive FTP client (plan 2026-10-25-m3-library.md, I155, D-M3-5). Anonymous login, binary
 * mode, SIZE, PASV, RETR; the body streams through a sink and nothing allocates. Same
 * socket table, guard and map as the HTTP client, in both tiers.
 *
 * The data connection goes to the address of the PASV reply as given (not to the control
 * host), so a server behind NAT that answers a private address fails with SEL_FTP_E_DATA. */
#ifndef SELECTA_LIBRARY_NET_FTP_H
#define SELECTA_LIBRARY_NET_FTP_H

#include <stddef.h>

typedef enum sel_ftp_err {
    SEL_FTP_OK = 0,
    SEL_FTP_E_URL,
    SEL_FTP_E_BLOCKED,
    SEL_FTP_E_CONNECT,
    SEL_FTP_E_LOGIN,
    SEL_FTP_E_NO_SUCH_FILE,
    SEL_FTP_E_PASV,
    SEL_FTP_E_DATA,
    SEL_FTP_E_TIMEOUT,
    SEL_FTP_E_IO,
    SEL_FTP_E_SINK
} sel_ftp_err;

typedef struct sel_ftp_res {
    unsigned retr;    /* RETR commands that completed with 226 */
    long size;        /* SIZE reply, -1 when none */
    unsigned passive; /* PASV commands that succeeded */
    unsigned long body_bytes;
    unsigned long elapsed_ms;
} sel_ftp_res;

/* Same contract as sel_http_sink: return 0 to abort. */
typedef int (*sel_ftp_sink)(void* ctx, const void* data, size_t len);

/* url: ftp://host[:port]/path. total_ms 0 selects 30000. */
sel_ftp_err sel_ftp_get(const char* url, unsigned total_ms, sel_ftp_sink sink, void* ctx, sel_ftp_res* res);
const char* sel_ftp_err_name(sel_ftp_err e);

#endif
