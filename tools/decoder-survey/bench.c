/* I725j decoder bench: decode a whole in-memory file `reps` times, discard PCM, print real-time factor.
 * One source file, one decoder per build (-DDEC_STB_VORBIS | DEC_TREMOR | DEC_TREMOLO | DEC_OPUS | DEC_FAAD2 |
 * DEC_FDK | DEC_DRFLAC | DEC_LIBFLAC). Usage: bench <file> [reps]. Output line:
 *   dec=<name> frames=<n> rate=<hz> ch=<c> reps=<r> sec=<wall> rtf=<wall/audio> sum=<pcm checksum>
 * RTF below 1.0 plays in real time (same definition as m3-I194-rig-timing-report.md). Wall time is time() on
 * AmigaOS (1 s resolution, so pick reps for >= 20 s) and clock_gettime on the Mac. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#ifdef __amigaos__
static double now(void) { return (double)time(NULL); }
#else
static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }
#endif

static unsigned char *g_buf; static size_t g_len;
static unsigned long long g_frames, g_sum; static int g_rate, g_ch;
static void sum16(const short *p, size_t n) { size_t i; for (i = 0; i < n; i += 97) g_sum += (unsigned short)p[i]; }

#if defined(DEC_STB_VORBIS)
#define STB_VORBIS_NO_STDIO
#include "stb_vorbis.c"
static const char *NAME = "stb_vorbis";
static int run(void) {
    int err = 0; short pcm[4096 * 2]; int n;
    stb_vorbis *v = stb_vorbis_open_memory(g_buf, (int)g_len, &err, NULL);
    if (!v) return -1;
    g_rate = (int)v->sample_rate; g_ch = v->channels;
    while ((n = stb_vorbis_get_samples_short_interleaved(v, 2, pcm, 4096 * 2)) > 0) { g_frames += n; sum16(pcm, (size_t)n * 2); }
    stb_vorbis_close(v); return 0;
}
#elif defined(DEC_TREMOR) || defined(DEC_TREMOLO)
#include "ivorbisfile.h"
#ifdef DEC_TREMOR
static const char *NAME = "tremor";
#define VI (vf.vi)
#else
static const char *NAME = "tremolo";
#define VI (&vf.vi)
#endif
static size_t mpos;
static size_t rd(void *p, size_t s, size_t n, void *d) { size_t w = s * n; (void)d; if (mpos + w > g_len) w = g_len - mpos; memcpy(p, g_buf + mpos, w); mpos += w; return s ? w / s : 0; }
static int sk(void *d, ogg_int64_t o, int w) { (void)d; if (w == 0) mpos = (size_t)o; else if (w == 1) mpos += (size_t)o; else mpos = g_len + (size_t)o; return 0; }
static int cl(void *d) { (void)d; return 0; }
static long tl(void *d) { (void)d; return (long)mpos; }
static int run(void) {
    OggVorbis_File vf; ov_callbacks cb = { rd, sk, cl, tl }; char buf[8192]; long n; int bs;
    mpos = 0;
    { int rc = ov_open_callbacks(&mpos, &vf, NULL, 0, cb); if (rc < 0) { fprintf(stderr, "ov_open rc=%d\n", rc); return -1; } }
    g_rate = (int)VI->rate; g_ch = VI->channels;
    while ((n = ov_read(&vf, buf, sizeof buf, &bs)) > 0) { g_frames += (unsigned long long)n / (2 * g_ch); sum16((short *)buf, (size_t)n / 2); }
    ov_clear(&vf); return 0;
}
#elif defined(DEC_OPUS)
#include <ogg/ogg.h>
#include <opus.h>
static const char *NAME = "opus";
static unsigned char **pk; static int *pl; static int npk;
static void demux(void) { /* untimed: Ogg packets out of the file, the timed part is opus_decode only */
    ogg_sync_state oy; ogg_stream_state os; ogg_page og; ogg_packet op; int init = 0, cap = 4096, k = 0;
    pk = malloc(cap * sizeof *pk); pl = malloc(cap * sizeof *pl);
    ogg_sync_init(&oy); { char *b = ogg_sync_buffer(&oy, (long)g_len); memcpy(b, g_buf, g_len); ogg_sync_wrote(&oy, (long)g_len); }
    while (ogg_sync_pageout(&oy, &og) == 1) {
        if (!init) { ogg_stream_init(&os, ogg_page_serialno(&og)); init = 1; }
        ogg_stream_pagein(&os, &og);
        while (ogg_stream_packetout(&os, &op) == 1) {
            if (k++ < 2) continue; /* OpusHead, OpusTags */
            if (npk == cap) { cap *= 2; pk = realloc(pk, cap * sizeof *pk); pl = realloc(pl, cap * sizeof *pl); }
            pk[npk] = malloc((size_t)op.bytes); memcpy(pk[npk], op.packet, (size_t)op.bytes); pl[npk++] = (int)op.bytes;
        }
    }
}
static int run(void) {
    int err, i, n; short pcm[5760 * 2]; OpusDecoder *d = opus_decoder_create(48000, 2, &err);
    if (!d) return -1;
    g_rate = 48000; g_ch = 2;
    if (!npk) demux();
    for (i = 0; i < npk; i++) { n = opus_decode(d, pk[i], pl[i], pcm, 5760, 0); if (n < 0) return -2; g_frames += n; sum16(pcm, (size_t)n * 2); }
    opus_decoder_destroy(d); return 0;
}
#elif defined(DEC_FAAD2)
#include "neaacdec.h"
static const char *NAME = "faad2";
static int run(void) {
    NeAACDecHandle h = NeAACDecOpen(); NeAACDecFrameInfo fi; unsigned long sr; unsigned char ch; size_t pos; void *o;
    long b = NeAACDecInit(h, g_buf, (unsigned long)g_len, &sr, &ch);
    if (b < 0) return -1;
    g_rate = (int)sr; g_ch = ch; pos = (size_t)b;
    while (pos < g_len && (o = NeAACDecDecode(h, &fi, g_buf + pos, (unsigned long)(g_len - pos)))) {
        if (fi.error) { if (fi.bytesconsumed == 0) break; }
        pos += fi.bytesconsumed; g_frames += fi.samples / (fi.channels ? fi.channels : 1); sum16((short *)o, fi.samples);
    }
    NeAACDecClose(h); return 0;
}
#elif defined(DEC_FDK)
#include "aacdecoder_lib.h"
static const char *NAME = "fdk-aac";
static int run(void) {
    HANDLE_AACDECODER d = aacDecoder_Open(TT_MP4_ADTS, 1); INT_PCM pcm[2048 * 8]; UCHAR *p = g_buf; UINT sz = (UINT)g_len, valid = (UINT)g_len;
    if (!d) return -1;
    aacDecoder_Fill(d, &p, &sz, &valid);
    for (;;) {
        AAC_DECODER_ERROR e = aacDecoder_DecodeFrame(d, pcm, sizeof pcm / sizeof *pcm, 0);
        if (e == AAC_DEC_NOT_ENOUGH_BITS) break;
        if (e == AAC_DEC_OK) { CStreamInfo *si = aacDecoder_GetStreamInfo(d); g_rate = si->sampleRate; g_ch = si->numChannels; g_frames += si->frameSize; sum16((short *)pcm, (size_t)si->frameSize * si->numChannels); }
    }
    aacDecoder_Close(d); return 0;
}
#elif defined(DEC_DRFLAC)
#include "dr_flac_impl.c"
static const char *NAME = "dr_flac";
static int run(void) {
    drflac *f = drflac_open_memory(g_buf, g_len, NULL); drflac_int32 pcm[4096 * 2]; drflac_uint64 n;
    if (!f) return -1;
    g_rate = (int)f->sampleRate; g_ch = f->channels;
    while ((n = drflac_read_pcm_frames_s32(f, 4096 / 2, pcm)) > 0) { g_frames += n; g_sum += (unsigned)pcm[0]; }
    drflac_close(f); return 0;
}
#elif defined(DEC_LIBFLAC)
#include <FLAC/stream_decoder.h>
static const char *NAME = "libFLAC";
static size_t fpos;
static FLAC__StreamDecoderReadStatus frd(const FLAC__StreamDecoder *d, FLAC__byte *b, size_t *n, void *c) { (void)d; (void)c;
    if (fpos >= g_len) { *n = 0; return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM; }
    if (*n > g_len - fpos) *n = g_len - fpos; memcpy(b, g_buf + fpos, *n); fpos += *n; return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE; }
static FLAC__StreamDecoderWriteStatus fwr(const FLAC__StreamDecoder *d, const FLAC__Frame *f, const FLAC__int32 *const b[], void *c) { (void)d; (void)c;
    g_frames += f->header.blocksize; g_rate = (int)f->header.sample_rate; g_ch = (int)f->header.channels; g_sum += (unsigned)b[0][0]; return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE; }
static void fer(const FLAC__StreamDecoder *d, FLAC__StreamDecoderErrorStatus s, void *c) { (void)d; (void)s; (void)c; }
static int run(void) {
    FLAC__StreamDecoder *d = FLAC__stream_decoder_new(); fpos = 0;
    if (FLAC__stream_decoder_init_stream(d, frd, NULL, NULL, NULL, NULL, fwr, NULL, fer, NULL) != FLAC__STREAM_DECODER_INIT_STATUS_OK) return -1;
    FLAC__stream_decoder_process_until_end_of_stream(d); FLAC__stream_decoder_delete(d); return 0;
}
#else
#error pick a decoder
#endif

int main(int argc, char **argv) {
    FILE *f; int reps = argc > 2 ? atoi(argv[2]) : 1, i; double t0, t1, audio;
    if (argc < 2 || !(f = fopen(argv[1], "rb"))) { fprintf(stderr, "usage: bench <file> [reps]\n"); return 2; }
    fseek(f, 0, SEEK_END); g_len = (size_t)ftell(f); fseek(f, 0, SEEK_SET);
    g_buf = malloc(g_len); if (fread(g_buf, 1, g_len, f) != g_len) return 2; fclose(f);
    if (run() != 0) { fprintf(stderr, "decode failed\n"); return 1; } /* warm-up run, also fills rate/ch */
    g_frames = 0; g_sum = 0;
    t0 = now(); for (i = 0; i < reps; i++) if (run() != 0) return 1; t1 = now();
    audio = (double)g_frames / g_rate;
    printf("dec=%s frames=%llu rate=%d ch=%d reps=%d sec=%.3f rtf=%.5f sum=%llu\n", NAME, g_frames / reps, g_rate, g_ch, reps, t1 - t0, (t1 - t0) / audio, g_sum / reps);
    return 0;
}
