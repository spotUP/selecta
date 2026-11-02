/* I892c2: RAOP sender driver for the raop-stream test. tests/net/raop_server.py starts this program against its fixture
 * receiver: `raop_stream_test <port> <pcm file>`. Plays the whole file (interleaved int16 stereo, whole packets) through
 * raop_session and prints raop_packets_sent; the server compares what it decrypted with the file. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "raop/raop_session.h"

/* Deterministic xorshift32; the production caller supplies a mbedtls DRBG. */
static int test_rng(void *ctx, unsigned char *out, size_t len) {
    uint32_t *s = ctx;
    size_t i;
    for (i = 0; i < len; i++) {
        *s ^= *s << 13;
        *s ^= *s >> 17;
        *s ^= *s << 5;
        out[i] = (unsigned char)(*s >> 11);
    }
    return 0;
}

int main(int argc, char **argv) {
    static raop_session s;
    static int16_t pcm[RAOP_FRAMES * 2];
    uint32_t seed = 0x9E3779B9u;
    raop_stats st;
    FILE *f;
    int rc;
    unsigned n = 0;
    struct timespec gap = {0, 2000000};

    if (argc < 3) return 2;
    f = fopen(argv[2], "rb");
    if (!f) return 2;
    rc = raop_session_open(&s, "127.0.0.1", (unsigned)atoi(argv[1]), test_rng, &seed);
    if (rc != 0) {
        printf("open failed %d\n", rc);
        return 1;
    }
    if (raop_session_set_volume(&s, -150) != 0) return 1;
    while (fread(pcm, sizeof pcm, 1, f) == 1) {
        if (raop_session_send(&s, pcm) != 0) return 1;
        n++;
        nanosleep(&gap, NULL);
    }
    if (raop_session_flush(&s) != 0) return 1;
    raop_session_close(&s);
    raop_stats_get(&st);
    printf("raop_packets_sent=%u latency=%u audio_port=%u raop_resends=%u raop_syncs=%u raop_timing_replies=%u latency_ms=%u\n",
           st.raop_packets_sent, s.latency, s.audio_port, st.raop_resends, st.raop_syncs, st.raop_timing_replies,
           raop_session_latency_ms(&s));
    fclose(f);
    return n == st.raop_packets_sent ? 0 : 1;
}
