/* Shared by the stream decoder plugins (minimp3, stb_vorbis; D-M3-8): a decoder yields stereo int16 frames at
 * its own rate, RP_RENDER must deliver them at the rate given to RP_START. Same rate: the frames pass through
 * untouched. Different rate: linear interpolation with a 16.16 position, enough for a codec whose own band
 * limit sits below the output Nyquist; the engine's resamplers serve the VOICE kind, not this one.
 * Header only because a plugin compiles against include/ and its own directory (T7). */
#ifndef SELECTA_STREAM_PCM_H
#define SELECTA_STREAM_PCM_H

#include <stdint.h>
#include <string.h>

#define SEL_STREAM_CHUNK 256u

/* Fills `out` with up to `frames` stereo frames; returns how many, 0 at the end of the stream. */
typedef uint32_t (*sel_stream_pull_fn)(void* ctx, int16_t* out, uint32_t frames);

typedef struct sel_stream_pcm {
    sel_stream_pull_fn pull;
    void* ctx;
    uint32_t src_rate, dst_rate;
    uint32_t step;     /* source frames per output frame, 16.16 */
    uint32_t frac;     /* position between prev and next, 16.16 */
    int primed, ended; /* ended: the source has no frame after `next` */
    int done;          /* the last interpolated frame was delivered */
    int16_t prev[2], next[2];
    int16_t buf[SEL_STREAM_CHUNK * 2];
    uint32_t buf_n, buf_i;
} sel_stream_pcm;

/* Also used after a seek: the decoder moved, so the interpolation state restarts. */
static inline void sel_stream_pcm_reset(sel_stream_pcm* p, sel_stream_pull_fn pull, void* ctx, uint32_t src_rate,
                                        uint32_t dst_rate) {
    memset(p, 0, sizeof *p);
    p->pull = pull;
    p->ctx = ctx;
    p->src_rate = src_rate;
    p->dst_rate = dst_rate;
    p->step = dst_rate != 0u ? (uint32_t)(((uint64_t)src_rate << 16) / dst_rate) : 65536u;
}

/* One source frame through the chunk buffer; 0 at the end of the stream. */
static inline int sel_stream_pcm_frame(sel_stream_pcm* p, int16_t f[2]) {
    if (p->buf_i >= p->buf_n) {
        p->buf_n = p->pull(p->ctx, p->buf, SEL_STREAM_CHUNK);
        p->buf_i = 0;
        if (p->buf_n == 0u) {
            return 0;
        }
    }
    f[0] = p->buf[p->buf_i * 2u];
    f[1] = p->buf[p->buf_i * 2u + 1u];
    p->buf_i++;
    return 1;
}

static inline uint32_t sel_stream_pcm_read(sel_stream_pcm* p, int16_t* out, uint32_t frames) {
    uint32_t n = 0;
    if (p->src_rate == p->dst_rate) {
        return p->pull(p->ctx, out, frames);
    }
    if (p->done) {
        return 0;
    }
    if (!p->primed) {
        p->primed = 1;
        if (!sel_stream_pcm_frame(p, p->prev)) {
            p->ended = 1;
            return 0;
        }
        if (!sel_stream_pcm_frame(p, p->next)) {
            p->next[0] = p->prev[0];
            p->next[1] = p->prev[1];
            p->ended = 1;
        }
    }
    while (n < frames) {
        uint32_t c;
        for (c = 0; c < 2u; c++) {
            int32_t a = p->prev[c], b = p->next[c];
            out[n * 2u + c] = (int16_t)(a + (((b - a) * (int32_t)(p->frac >> 1)) >> 15));
        }
        n++;
        p->frac += p->step;
        while (p->frac >= 65536u) {
            p->frac -= 65536u;
            if (p->ended) {
                p->done = 1;
                return n;
            }
            p->prev[0] = p->next[0];
            p->prev[1] = p->next[1];
            if (!sel_stream_pcm_frame(p, p->next)) {
                p->ended = 1;
            }
        }
    }
    return n;
}

#endif /* SELECTA_STREAM_PCM_H */
