/* The UHC search request (row I1405, shared since I1407). The index answers in about 0.2 s; give up early so a dead index
 * costs 5 s, not the client default. */
#include "source/uhc.h"
#include "source/url_path.h"

#include <selecta/catalogue.h>
#include <selecta/net.h>

#include <stdio.h>
#include <stdlib.h>

#define UHC_ROWS 500 /* HippoPlayer's default UHC search limit */
#define UHC_FIRST_BYTE_MS 5000u

int sel_uhc_search(const sel_host* host, const char* cache_id, const char* base_env, const char* searchtype, const char* query,
                   sel_uhc_row_fn row, void* ctx) {
    const sel_net* net = host->net != NULL ? host->net(host) : NULL;
    const sel_catalogue* cat = host->catalogue != NULL ? host->catalogue(host) : NULL;
    const char* env = base_env != NULL ? getenv(base_env) : NULL;
    char dir[512];
    char path[600];
    char enc[384];
    char url[1000];
    uint32_t rq = 0;
    unsigned long got = 0;
    FILE* f;
    int rows;

    if (net == NULL || cat == NULL || query == NULL || query[0] == '\0' || !sel_url_query_encode(query, enc, sizeof enc) ||
        !cat->cache_dir(host, cache_id, dir, sizeof dir)) {
        return -1;
    }
    snprintf(path, sizeof path, "%s/uhc.json", dir);
    snprintf(url, sizeof url, "%s/uhcsearch?format=json&searchtype=%s&searchlimit=%d&searchwords=%s",
             env != NULL && env[0] != '\0' ? env : "https://uhc.megaburken.net", searchtype, UHC_ROWS, enc);
    if (net->get_file_wait(host, url, path, UHC_FIRST_BYTE_MS, &rq, &got) != 200) {
        remove(path);
        return -1;
    }
    f = fopen(path, "rb");
    rows = f != NULL ? sel_uhc_parse(f, row, ctx) : -1;
    if (f != NULL) {
        fclose(f);
    }
    remove(path);
    return rows;
}
