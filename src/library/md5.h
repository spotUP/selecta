/* MD5 (RFC 1321), portable C99, for cache verification against the catalogue's md5 column
 * (Modland allmods_md5). Not a security measure. */
#ifndef SELECTA_LIBRARY_MD5_H
#define SELECTA_LIBRARY_MD5_H

#include <stddef.h>
#include <stdint.h>

typedef struct sel_md5 {
    uint32_t state[4];
    uint64_t bytes;
    uint8_t buf[64];
} sel_md5;

void sel_md5_init(sel_md5* c);
void sel_md5_update(sel_md5* c, const void* data, size_t len);
/* 32 lower-case hex characters plus NUL. */
void sel_md5_hex(sel_md5* c, char out[33]);

#endif
