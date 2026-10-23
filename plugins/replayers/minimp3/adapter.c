/* minimp3 stream decoder (design doc section 4, D-M3-8): a RENDER-kind replayer over an MP3 byte stream.
 * minimp3 decodes frame by frame from the source bytes; the adapter skips the ID3v2 tag, resamples to the
 * output rate (stream_pcm.h) and reports the duration from a header walk. Float synthesis inside minimp3, so
 * the plugin carries SEL_RP_NEEDS_FLOAT and the M2 machine filter hides it on a machine without an FPU.
 *
 * Live streams (I195b): the plugin declares SEL_RP_STREAMING, so an http(s) source arrives as a sel_byte_stream
 * while it still downloads. The adapter then decodes from a small window refilled without blocking; when the
 * download runs dry it renders silence for the missing frames (the audio thread never waits) and carries on once
 * bytes arrive. A stream has no known length: no duration, no size, no seek. */
#include <minimp3.h>

#include <selecta/plugin.h>
#include <selecta/replayer.h>

#include "../stream/id3.h"
#include "../stream/stream_pcm.h"

#include <stdlib.h>
#include <string.h>

#define MP3_WINDOW 32768u /* stream mode: bytes buffered ahead of the decoder */
#define MP3_NEED 3072u    /* stream mode: bytes that always hold one whole frame (longest Layer III frame is 1441) */

typedef struct mp3_state {
    const sel_host* host;
    const uint8_t* data;
    size_t size, start, pos;
    mp3dec_t dec;
    uint32_t hz;
    int channels;
    uint32_t total_frames; /* source frames, from the header walk */
    uint32_t rate;
    sel_stream_pcm pcm;
    int16_t frame[MINIMP3_MAX_SAMPLES_PER_FRAME];
    uint32_t left, off, frame_ch; /* undelivered frames of `frame` */
    sel_id3 tag;
    sel_tag meta[8];
    const sel_byte_stream* in; /* live stream, or NULL when `data` is the whole file */
    uint8_t win[MP3_WINDOW];
} mp3_state;

/* ID3v2 header: "ID3", version, flags, 4 syncsafe size bytes; returns the tag length or 0. */
static size_t id3v2_len(const uint8_t* d, size_t n) {
    if (n >= 10u && d[0] == 'I' && d[1] == 'D' && d[2] == '3' && d[3] < 0xFFu && d[4] < 0xFFu &&
        ((d[6] | d[7] | d[8] | d[9]) & 0x80u) == 0u) {
        return 10u + (((size_t)d[6] << 21) | ((size_t)d[7] << 14) | ((size_t)d[8] << 7) | d[9]) +
               ((d[5] & 0x10u) != 0u ? 10u : 0u);
    }
    return 0;
}

/* Length of the Layer III frame whose header is at `h`, 0 if it is not a fixed-bitrate Layer III header. */
static size_t frame_len(const uint8_t* h) {
    static const uint8_t br1[16] = {0, 4, 5, 6, 7, 8, 10, 12, 14, 16, 20, 24, 28, 32, 40, 0}; /* x8 kbit/s */
    static const uint8_t br2[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 12, 14, 16, 18, 20, 0};   /* x8 kbit/s */
    static const uint16_t sr[3] = {44100, 48000, 32000};
    unsigned ver = (h[1] >> 3) & 3u, idx = (unsigned)(h[2] >> 4), rate = (h[2] >> 2) & 3u;
    unsigned bitrate, hz;
    if (h[0] != 0xFFu || (h[1] & 0xE0u) != 0xE0u || ver == 1u || (h[1] & 6u) != 2u || idx == 0u || idx == 15u ||
        rate == 3u) {
        return 0;
    }
    bitrate = (ver == 3u ? br1[idx] : br2[idx]) * 8000u;
    hz = sr[rate] >> (ver == 3u ? 0 : (ver == 2u ? 1 : 2));
    return (size_t)((ver == 3u ? 144u : 72u) * bitrate / hz) + ((h[2] >> 1) & 1u);
}

/* Two consecutive valid headers within the first 4 KB after the tag. */
static int rp_probe(const uint8_t* data, size_t size, const char* name) {
    size_t i = id3v2_len(data, size), limit = i + 4096u;
    (void)name;
    for (; i + 4u <= size && i < limit; i++) {
        size_t n = frame_len(data + i);
        if (n != 0u && i + n + 4u <= size && frame_len(data + i + n) != 0u) {
            return SEL_PROBE_YES;
        }
    }
    return SEL_PROBE_NO;
}

/* Header walk without decoding: total source frames. */
static uint32_t walk_frames(mp3_state* s) {
    mp3dec_t dec;
    mp3dec_frame_info_t info;
    size_t pos = s->start;
    uint32_t total = 0;
    mp3dec_init(&dec);
    while (pos < s->size) {
        int samples = mp3dec_decode_frame(&dec, s->data + pos, (int)(s->size - pos), NULL, &info);
        if (info.frame_bytes == 0) {
            break;
        }
        total += (uint32_t)samples;
        pos += (size_t)info.frame_bytes;
    }
    return total;
}

/* Stream mode open: consumes the ID3v2 tag from the stream (it may be longer than the head the probe saw), then
 * waits for enough bytes to decode the first frame, which fixes the rate and channel count. */
static int open_live(mp3_state* s, const sel_song_source* src, mp3dec_frame_info_t* fi) {
    const sel_byte_stream* in = s->in;
    size_t skip = id3v2_len(src->data, src->size);
    while (skip > 0u) {
        uint8_t junk[512];
        size_t n = in->read_wait(in->ctx, junk, skip < sizeof junk ? skip : sizeof junk);
        if (n == 0u) {
            return 0;
        }
        skip -= n;
    }
    while (s->size < MP3_NEED && !in->ended(in->ctx)) {
        size_t n = in->read_wait(in->ctx, s->win + s->size, MP3_NEED - s->size);
        if (n == 0u) {
            break;
        }
        s->size += n;
    }
    s->data = s->win;
    s->pos = 0;
    mp3dec_init(&s->dec);
    return s->size > 0u && mp3dec_decode_frame(&s->dec, s->win, (int)s->size, s->frame, fi) != 0;
}

/* Stream mode: tops the window up from the ring without waiting; consumed bytes are squeezed out first. */
static void refill(mp3_state* s) {
    if (s->pos > 0u) {
        memmove(s->win, s->win + s->pos, s->size - s->pos);
        s->size -= s->pos;
        s->pos = 0;
    }
    if (s->size < MP3_WINDOW) {
        s->size += s->in->read(s->in->ctx, s->win + s->size, MP3_WINDOW - s->size);
    }
}

static void* rp_open(const sel_song_source* src, const sel_host* host, sel_rp_info* info) {
    mp3_state* s;
    mp3dec_frame_info_t fi;
    s = (mp3_state*)host->mem_alloc(host, sizeof *s, SEL_MEM_FAST);
    if (s == NULL) {
        return NULL;
    }
    memset(s, 0, sizeof *s);
    s->host = host;
    s->in = src->stream;
    if (s->in != NULL) {
        if (!open_live(s, src, &fi)) {
            host->mem_free(host, s);
            return NULL;
        }
        s->hz = (uint32_t)fi.hz;
        s->channels = fi.channels;
        sel_id3_read(src->data, src->size, &s->tag);
        info->voice_count = 0;
        info->subsong_count = 1;
        info->default_subsong = 0;
        info->timebase_hz = 0;
        return s;
    }
    s->data = src->data;
    s->size = src->size;
    s->start = id3v2_len(src->data, src->size);
    if (s->start > s->size) {
        s->start = s->size;
    }
    s->pos = s->start;
    mp3dec_init(&s->dec);
    /* The first decodable frame fixes the rate and channel count. */
    if (mp3dec_decode_frame(&s->dec, s->data + s->start, (int)(s->size - s->start), s->frame, &fi) == 0) {
        host->mem_free(host, s);
        return NULL;
    }
    s->hz = (uint32_t)fi.hz;
    s->channels = fi.channels;
    s->total_frames = walk_frames(s);
    sel_id3_read(src->data, src->size, &s->tag);
    info->voice_count = 0;
    info->subsong_count = 1;
    info->default_subsong = 0;
    info->timebase_hz = 0;
    return s;
}

static void rp_close(void* state) {
    mp3_state* s = (mp3_state*)state;
    if (s != NULL) {
        const sel_host* host = s->host;
        host->mem_free(host, s);
    }
}

/* Source frames: decoded MP3 frames are handed out in pieces (a frame holds up to 1152 frames, the caller
 * asks for less), mono duplicated to both channels. */
static uint32_t pull(void* ctx, int16_t* out, uint32_t frames) {
    mp3_state* s = (mp3_state*)ctx;
    uint32_t n = 0;
    while (n < frames) {
        if (s->left == 0u) {
            mp3dec_frame_info_t fi;
            int got;
            if (s->in != NULL) {
                int ended;
                if (s->size - s->pos < MP3_NEED) {
                    refill(s);
                }
                ended = s->in->ended(s->in->ctx);
                if (s->size - s->pos < MP3_NEED && !ended) {
                    /* Underrun: the download is behind. Silence for what is missing, never a wait. */
                    memset(out + n * 2u, 0, (size_t)(frames - n) * 2u * sizeof out[0]);
                    n = frames;
                    break;
                }
            }
            if (s->pos >= s->size) {
                break;
            }
            got = mp3dec_decode_frame(&s->dec, s->data + s->pos, (int)(s->size - s->pos), s->frame, &fi);
            if (fi.frame_bytes == 0) {
                s->pos = s->size;
                if (s->in != NULL && !s->in->ended(s->in->ctx)) {
                    continue; /* garbage with more to come: drop it and resync on the next bytes */
                }
                break;
            }
            s->pos += (size_t)fi.frame_bytes;
            if (got <= 0) {
                continue;
            }
            s->left = (uint32_t)got;
            s->off = 0;
            s->frame_ch = (uint32_t)fi.channels;
        }
        out[n * 2u] = s->frame[s->off * s->frame_ch];
        out[n * 2u + 1u] = s->frame[s->off * s->frame_ch + (s->frame_ch - 1u)];
        s->off++;
        s->left--;
        n++;
    }
    return n;
}

static int rp_start(void* state, uint32_t subsong, uint32_t rate, const sel_voices* voices) {
    mp3_state* s = (mp3_state*)state;
    (void)subsong;
    (void)voices;
    if (rate == 0u) {
        return 0;
    }
    s->rate = rate;
    if (s->in == NULL) {
        s->pos = s->start; /* a live stream cannot rewind: it plays on from where the open left it */
    }
    s->left = 0;
    mp3dec_init(&s->dec);
    sel_stream_pcm_reset(&s->pcm, pull, s, s->hz, rate);
    return 1;
}

static uint32_t rp_render(void* state, int16_t* out, uint32_t frames) {
    mp3_state* s = (mp3_state*)state;
    return sel_stream_pcm_read(&s->pcm, out, frames);
}

/* Header walk to the frame holding the target, then back two frames so the bit reservoir refills; the
 * decoded lead-in up to the target is dropped. */
static int rp_seek(void* state, uint32_t ms) {
    mp3_state* s = (mp3_state*)state;
    if (s->in != NULL) {
        return 0; /* no seek in a live stream */
    }
    uint64_t target = (uint64_t)ms * s->hz / 1000u, at = 0, from = 0, from1 = 0;
    size_t pos = s->start, pos1 = s->start, pos2 = s->start;
    mp3dec_t dec;
    mp3dec_frame_info_t fi;
    mp3dec_init(&dec);
    while (pos < s->size && at < target) {
        int samples = mp3dec_decode_frame(&dec, s->data + pos, (int)(s->size - pos), NULL, &fi);
        if (fi.frame_bytes == 0) {
            break;
        }
        pos2 = pos1;
        from = from1;
        pos1 = pos;
        from1 = at;
        at += (uint64_t)samples;
        pos += (size_t)fi.frame_bytes;
    }
    s->pos = pos2;
    s->left = 0;
    mp3dec_init(&s->dec);
    sel_stream_pcm_reset(&s->pcm, pull, s, s->hz, s->rate);
    while (from < target) {
        int16_t junk[SEL_STREAM_CHUNK * 2];
        uint64_t left = target - from;
        uint32_t got = pull(s, junk, left < SEL_STREAM_CHUNK ? (uint32_t)left : SEL_STREAM_CHUNK);
        if (got == 0u) {
            break;
        }
        from += got;
    }
    return 1;
}

static const sel_tag* rp_metadata(void* state) {
    mp3_state* s = (mp3_state*)state;
    uint32_t n = 0;
    if (s->tag.title[0] != '\0') {
        s->meta[n].tag = SEL_META_TITLE;
        s->meta[n++].value = (uintptr_t)s->tag.title;
    }
    if (s->tag.artist[0] != '\0') {
        s->meta[n].tag = SEL_META_AUTHOR;
        s->meta[n++].value = (uintptr_t)s->tag.artist;
    }
    if (s->tag.album[0] != '\0') {
        s->meta[n].tag = SEL_META_RELEASE;
        s->meta[n++].value = (uintptr_t)s->tag.album;
    }
    s->meta[n].tag = SEL_META_FORMAT;
    s->meta[n++].value = (uintptr_t) "MP3";
    s->meta[n].tag = SEL_META_CHANNELS;
    s->meta[n++].value = (uintptr_t)s->channels;
    if (s->in == NULL) { /* a live stream has neither a length nor a size */
        s->meta[n].tag = SEL_META_DURATION_MS;
        s->meta[n++].value = (uintptr_t)((uint64_t)s->total_frames * 1000u / s->hz);
        s->meta[n].tag = SEL_META_SIZE;
        s->meta[n++].value = (uintptr_t)s->size;
    }
    s->meta[n].tag = SEL_TAG_END;
    s->meta[n].value = 0;
    return s->meta;
}

static sel_tag g_hooks[15];
static sel_tag g_desc[5];

static void set(sel_tag* t, uint32_t tag, uintptr_t value) {
    t->tag = tag;
    t->value = value;
}

const sel_tag* SEL_PLUGIN_ENTRY(uint32_t host_api) {
    if (host_api < 1u) {
        return NULL;
    }
    set(&g_hooks[0], SEL_RP_NAME, (uintptr_t) "minimp3");
    set(&g_hooks[1], SEL_RP_VERSION, 0x00010000);
    set(&g_hooks[2], SEL_RP_KIND, SEL_RP_KIND_RENDER);
    set(&g_hooks[3], SEL_RP_EXTENSIONS, (uintptr_t) "mp3,mp2");
    set(&g_hooks[4], SEL_RP_PROBE, SEL_TAG_FN(rp_probe));
    set(&g_hooks[5], SEL_RP_OPEN, SEL_TAG_FN(rp_open));
    set(&g_hooks[6], SEL_RP_CLOSE, SEL_TAG_FN(rp_close));
    set(&g_hooks[7], SEL_RP_START, SEL_TAG_FN(rp_start));
    set(&g_hooks[8], SEL_RP_RENDER, SEL_TAG_FN(rp_render));
    set(&g_hooks[9], SEL_RP_SEEK, SEL_TAG_FN(rp_seek));
    set(&g_hooks[10], SEL_RP_METADATA, SEL_TAG_FN(rp_metadata));
    set(&g_hooks[11], SEL_RP_MIN_CPU, 68020);
    set(&g_hooks[12], SEL_RP_NEEDS_FLOAT, 1);
    set(&g_hooks[13], SEL_RP_STREAMING, 1);
    set(&g_hooks[14], SEL_TAG_END, 0);

    set(&g_desc[0], SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_REPLAYER);
    set(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "minimp3");
    set(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    set(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    set(&g_desc[4], SEL_TAG_END, 0);
    return g_desc;
}
