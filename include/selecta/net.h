/* Network service for source plugins (I517 D2). `sel_host.net` returns the table, NULL when the build has no
 * network stack. The calls are the ones sync_net.h offered the built-in sources: GET to a file, Range GET, HEAD,
 * a polite pause between requests. `requests` and `bytes` accumulate across calls so a sync can report them. */
#ifndef SELECTA_NET_H
#define SELECTA_NET_H

#include <selecta/host.h>

SEL_EXTERN_C_BEGIN

/* Content check for sel_net.get_file_checked: gets the downloaded file's path, returns 1 to keep it, 0 to reject. */
typedef int (*sel_net_check)(const char* path);

/* Returned by get_file_checked when the server answered 200 but the check refused the body (an HTML error page
 * saved under a data file's name, I718). The file is removed. */
#define SEL_NET_CONTENT_REJECTED (-2)

typedef struct sel_net {
    /* GETs `url` into `path`. Returns the HTTP status (0 on a transport failure); anything but 200 removes the file.
     * `*requests` gains the requests made (redirects count), `*bytes` is the body size. */
    int (*get_file)(const sel_host* host, const char* url, const char* path, uint32_t* requests, unsigned long* bytes);
    /* Bytes `from`..`to` (inclusive) with one Range request. Returns the status; anything but 206 removes the file. */
    int (*get_range)(const sel_host* host, const char* url, const char* path, unsigned long from, unsigned long to,
                     uint32_t* requests, unsigned long* bytes);
    /* HEAD: 1 and `*length` = Content-Length on a 200 that carries one, else 0. */
    int (*head_length)(const sel_host* host, const char* url, uint32_t* requests, unsigned long* length);
    /* Waits `ms` between two requests to one host. */
    void (*pause)(const sel_host* host, uint32_t ms);
    /* One GET into the caller's buffer, for sources that page through an API. Returns the status (0: transport
     * failure); `*got` is the body bytes stored (at most `cap`), truncated when the body is longer. */
    int (*request)(const sel_host* host, const char* url, void* buf, uint32_t cap, uint32_t* got, uint32_t* requests);
    /* get_file, then `check(path)` on a 200 body: a refused body removes the file and the call returns
     * SEL_NET_CONTENT_REJECTED. Any other status is returned as get_file returns it. A NULL `check` is get_file. */
    int (*get_file_checked)(const sel_host* host, const char* url, const char* path, sel_net_check check,
                            uint32_t* requests, unsigned long* bytes);
    /* Like `request`, with the caller's own User-Agent (NULL keeps the default "Selecta/<version> (+URL)") and
     * extra "Name: value" request headers (NULL-terminated array, or NULL). Appended last (I725p). */
    int (*request_ex)(const sel_host* host, const char* url, const char* user_agent, const char* const* headers,
                      void* buf, uint32_t cap, uint32_t* got, uint32_t* requests);
} sel_net;

SEL_EXTERN_C_END

#endif /* SELECTA_NET_H */
