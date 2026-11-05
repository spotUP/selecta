/* Shared entropy pool (I899a). Desktop: /dev/urandom. Amiga: no urandom, so a SHA-256 pool fed by timing samples
 * (E-clock jitter around input/UI wakeups, beam position) plus a persisted seed file. The pool is not ready until
 * SEL_ENTROPY_THRESHOLD samples were added; consumers (cast HTTP token, later TLS) refuse to run before that.
 * Single-threaded: call from the main thread only. */
#ifndef SELECTA_LIBRARY_ENTROPY_H
#define SELECTA_LIBRARY_ENTROPY_H

#include <stddef.h>
#include <stdint.h>

#define SEL_ENTROPY_THRESHOLD 256u

/* Mixes one timing sample into the pool and counts it. */
void sel_entropy_add(uint32_t sample);
/* Samples counted so far (the pool backend; the POSIX backend does not need them). */
unsigned sel_entropy_samples(void);
/* 1 when sel_entropy_bytes can deliver: POSIX always, Amiga after the threshold. */
int sel_entropy_ready(void);
/* Fills buf with n bytes. 0 on success, -1 when not ready (buf untouched). */
int sel_entropy_bytes(void* buf, size_t n);

/* Mixes the seed file into the pool (it does not count as samples: the seed alone never makes the pool ready) and
 * remembers the path for sel_entropy_seed_save. 0 when read, -1 when absent. */
int sel_entropy_seed_load(const char* path);
/* Writes 32 fresh output bytes (never the raw pool) to the remembered path. 0 on success, -1 otherwise, including
 * when not ready or no path. */
int sel_entropy_seed_save(void);

/* SHA-256 of a buffer (I751 support files); the digest the pool already uses. */
void sel_sha256(const void* data, size_t n, uint8_t out[32]);

/* Test hook: 0 normal, 1 force "not ready", 2 force the pool backend on a POSIX host. Also clears the pool. */
void sel_entropy_test_mode(int mode);

#endif
