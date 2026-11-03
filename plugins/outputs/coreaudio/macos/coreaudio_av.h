/* I1039: the AVFoundation path of the CoreAudio output. An AVSampleBufferAudioRenderer driven by an
 * AVSampleBufferRenderSynchronizer plays the engine's int16 stereo PCM. Unlike the HAL AudioUnit, this renderer follows
 * the AirPlay route the app's own AVRoutePickerView picks (a HomePod), which HAL output never reaches (proven live by
 * Johan on 2026-11-03, research row D of thoughts/shared/research/2026-11-01-airplay-chromecast.md).
 * coreaudio.c drives it through this table so the host test can swap in a fake that plays nothing. */
#ifndef SEL_COREAUDIO_AV_H
#define SEL_COREAUDIO_AV_H

#include <stdint.h>

typedef struct ca_av_backend {
    void* (*create)(uint32_t rate);  /* renderer + synchronizer for s16 stereo at `rate`; NULL on failure */
    void (*destroy)(void* av);       /* stops, flushes and frees */
    int (*ready)(void* av);          /* the renderer takes more media now */
    int (*enqueue)(void* av, const int16_t* pcm, uint32_t frames, int64_t pts); /* pts in frames; 1 = queued */
    int64_t (*played)(void* av);     /* the synchronizer clock in frames */
    void (*start)(void* av, int64_t pts); /* run the clock from media time `pts` (frames) now */
    void (*flush)(void* av);         /* stop the clock and drop every queued buffer */
} ca_av_backend;

extern const ca_av_backend ca_av_real;

#endif
