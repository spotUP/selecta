/* CoreAudio output (design doc section 5, plan Phase 6, I1039): a HAL output AudioUnit. The unit plays to the device
 * the pref output.device names (a CoreAudio device uid), or follows the system default output when the pref is empty,
 * also when the default changes mid-play (an AirPlay speaker picked in the system menu moves playback). The unit's
 * render callback runs on the system audio thread and pulls int16 stereo frames from the engine through sel_pcm_pull.
 * The callback allocates nothing and takes no locks: it reads the pull source (set before the unit starts) and bumps
 * two atomic counters. OUT_PUMP starts the unit on first use, re-targets it when the pref or the default changed, and
 * sleeps until the requested frames have been rendered, so the front end paces itself on the device clock.
 * Every CoreAudio call outside the render callback goes through ca_backend, so the host test drives the same code with
 * a fake device list and a counter instead of a sound card (tests/coreaudio_device_test.c).
 *
 * AirPlay route mode (I1039, research row D of thoughts/shared/research/2026-11-01-airplay-chromecast.md): HAL output
 * never reaches a HomePod the app picks in AVRoutePickerView; an AVSampleBufferAudioRenderer does. OUT_DEVICES lists
 * one extra entry, SEL_OUT_DEVICE_AIRPLAY_ROUTE. When the pref names it, ca_bind stops the HAL unit and the pump feeds
 * the renderer instead (coreaudio_av.m, through ca_av_backend so the test fakes it). Any other pick returns to the HAL
 * unit. HAL stays the default: it has the lower latency and the direct device pick; the AV path only runs on the pick.
 * OS headers are allowed here by RULES.md rule 3 (plugins, os subdirectory). */
#include <selecta/output.h>

#include "coreaudio_av.h"

#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudio/CoreAudio.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct ca_out ca_out;

/* The device layer. The real one talks to CoreAudio; a test installs a fake. */
typedef struct ca_backend {
    uint32_t (*list)(sel_out_device* out, uint32_t max);
    int (*bind)(ca_out* o, const char* uid);  /* point the unit at device `uid` ("" = system default); 1 = ok */
    int (*start)(ca_out* o);
    void (*stop)(ca_out* o);
    void (*watch_default)(ca_out* o, int on); /* sets o->default_changed when the system default output changes */
    uint32_t (*latency_ms)(ca_out* o);        /* real latency of the bound device */
    void (*idle)(ca_out* o);                  /* one wait step of the pump; the real one sleeps 1 ms */
} ca_backend;

struct ca_out {
    const sel_host* host;
    AudioUnit unit;
    sel_pcm_pull pull;
    void* pull_ctx;
    uint32_t rate;
    int started;
    int inited;         /* the unit is initialised: a device change must uninitialise it first */
    uint32_t callbacks; /* atomic */
    uint32_t frames;    /* atomic */
    sel_tag stats[5];
    char uid[SEL_OUT_DEVICE_UID_MAX]; /* the pref value the unit is bound to ("" = system default, followed) */
    AudioDeviceID dev;                /* the device the unit plays to */
    uint32_t default_changed;         /* atomic: the system default output moved since the last bind */
    uint32_t binds;                   /* how many times the unit was bound to a device (the first open counts) */
    uint32_t latency_ms;              /* real latency of the bound device */
    /* AirPlay route mode (I1143): a worker thread owns every renderer call except create, because setRate:time: and
     * setRate:0 wait on the AirPlay device's IO cycle (AudioQueuePause) and the UI thread never waits on the device.
     * The UI thread writes `av` and `av_doomed` under av_lock; the worker reads them under it and makes the calls
     * outside it. With a test device layer (g_be set) the pump drives the renderer inline, as before, so the host test
     * stays deterministic. */
    void* av;           /* the renderer while the AirPlay route entry is picked, else NULL */
    void* av_doomed;    /* a renderer the UI thread left (another device picked); the worker destroys it */
    int av_anchored;    /* the synchronizer clock runs (worker-owned while the worker runs) */
    int64_t av_pts;     /* frames queued so far: the presentation time of the next buffer (worker-owned likewise) */
    int16_t* av_buf;    /* CA_AV_CHUNK stereo frames, allocated at open */
    uint32_t av_buffers; /* atomic: sample buffers queued (the --stats sentinel out_av_buffers) */
    uint32_t av_flushes; /* atomic: OUT_FLUSH requests applied to the renderer queue (out_av_flushes) */
    uint32_t av_flush_req; /* atomic: OUT_FLUSH requests from the UI thread; the worker applies each */
    pthread_mutex_t av_lock;
    pthread_cond_t av_wake;
    pthread_t av_thread;
    int av_thread_on;   /* the worker runs (UI-thread view) */
    int av_quit;        /* under av_lock: the worker exits */
};

/* Queue ahead of the synchronizer clock: enough to ride out the pump's scheduling, small enough that a seek is heard soon. */
#define CA_AV_AHEAD_MS 200u
#define CA_AV_CHUNK 1024u

static const ca_backend* g_be;
static const ca_av_backend* g_av; /* a test fake; NULL = AVFoundation */

static const ca_av_backend* av_be(void) {
    return g_av != NULL ? g_av : &ca_av_real;
}

/* ---- I1143 test knob: a slow AirPlay route ----
 * SELECTA_CA_AV_FAKE_BLOCK_MS=<ms> swaps the AVFoundation renderer for one that plays nothing, whose clock runs on wall
 * time while started, and whose start (setRate:time:) and flush (setRate:0) sleep <ms>. That models the live AirPlay
 * route, where AudioQueuePause inside setRate waits on the device's IO cycle (sample in the I1143 report). Only the
 * coreaudio-av-nowait test sets it; one renderer exists at a time, so the state is static. */
static struct {
    uint32_t rate;
    uint32_t block_ms;
    int running;
    int64_t anchor_pts;
    uint64_t anchor_ns;
} g_slow;

static uint64_t ca_now_ns(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint64_t)t.tv_sec * 1000000000ull + (uint64_t)t.tv_nsec;
}

static void ca_slow_block(void) {
    struct timespec t;
    t.tv_sec = (time_t)(g_slow.block_ms / 1000u);
    t.tv_nsec = (long)(g_slow.block_ms % 1000u) * 1000000L;
    nanosleep(&t, NULL);
}

static void* ca_slow_create(uint32_t rate) {
    g_slow.rate = rate;
    g_slow.running = 0;
    return &g_slow;
}

static void ca_slow_destroy(void* av) {
    (void)av;
    ca_slow_block();
    g_slow.running = 0;
}

static int ca_slow_ready(void* av) {
    (void)av;
    return 1;
}

static int ca_slow_enqueue(void* av, const int16_t* pcm, uint32_t frames, int64_t pts) {
    (void)av;
    (void)pcm;
    (void)frames;
    (void)pts;
    return 1;
}

static int64_t ca_slow_played(void* av) {
    (void)av;
    if (!g_slow.running) {
        return g_slow.anchor_pts;
    }
    return g_slow.anchor_pts + (int64_t)((ca_now_ns() - g_slow.anchor_ns) * g_slow.rate / 1000000000ull);
}

static void ca_slow_start(void* av, int64_t pts) {
    (void)av;
    ca_slow_block();
    g_slow.anchor_pts = pts;
    g_slow.anchor_ns = ca_now_ns();
    g_slow.running = 1;
}

static void ca_slow_flush(void* av) {
    g_slow.anchor_pts = ca_slow_played(av);
    ca_slow_block();
    g_slow.running = 0;
}

static const ca_av_backend g_av_slow = {ca_slow_create, ca_slow_destroy, ca_slow_ready, ca_slow_enqueue,
                                        ca_slow_played, ca_slow_start, ca_slow_flush};

/* Installs the slow fake when the knob is set (real device layer only; the host test brings its own fakes). */
static void ca_slow_knob(void) {
    const char* v = getenv("SELECTA_CA_AV_FAKE_BLOCK_MS");
    if (g_be == NULL && v != NULL && v[0] != '\0') {
        g_slow.block_ms = (uint32_t)strtoul(v, NULL, 10);
        g_av = &g_av_slow;
    }
}

static OSStatus ca_render(void* ref, AudioUnitRenderActionFlags* flags, const AudioTimeStamp* ts,
                          UInt32 bus, UInt32 frames, AudioBufferList* io) {
    ca_out* o = (ca_out*)ref;
    int16_t* dst = (int16_t*)io->mBuffers[0].mData;
    sel_pcm_pull pull = o->pull;
    uint32_t got = 0;
    (void)flags;
    (void)ts;
    (void)bus;
    if (pull != NULL) {
        got = pull(o->pull_ctx, dst, frames);
    }
    if (got < frames) {
        memset(dst + (size_t)got * 2u, 0, (size_t)(frames - got) * 2u * sizeof(int16_t));
    }
    __atomic_fetch_add(&o->callbacks, 1u, __ATOMIC_RELAXED);
    __atomic_fetch_add(&o->frames, frames, __ATOMIC_RELAXED);
    return noErr;
}

/* ---- Real CoreAudio device layer ---- */

static AudioObjectPropertyAddress ca_addr(AudioObjectPropertySelector sel, AudioObjectPropertyScope scope) {
    AudioObjectPropertyAddress a;
    a.mSelector = sel;
    a.mScope = scope;
    a.mElement = kAudioObjectPropertyElementMain;
    return a;
}

static AudioDeviceID ca_default_device(void) {
    AudioObjectPropertyAddress addr = ca_addr(kAudioHardwarePropertyDefaultOutputDevice, kAudioObjectPropertyScopeGlobal);
    AudioDeviceID dev = kAudioObjectUnknown;
    UInt32 size = sizeof dev;
    if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &addr, 0, NULL, &size, &dev) != noErr) {
        return kAudioObjectUnknown;
    }
    return dev;
}

static void ca_cfstring(CFStringRef s, char* out, size_t cap) {
    out[0] = '\0';
    if (s != NULL) {
        if (!CFStringGetCString(s, out, (CFIndex)cap, kCFStringEncodingUTF8)) {
            out[0] = '\0';
        }
        CFRelease(s);
    }
}

static void ca_device_text(AudioDeviceID dev, AudioObjectPropertySelector sel, char* out, size_t cap) {
    AudioObjectPropertyAddress addr = ca_addr(sel, kAudioObjectPropertyScopeGlobal);
    CFStringRef s = NULL;
    UInt32 size = sizeof s;
    if (AudioObjectGetPropertyData(dev, &addr, 0, NULL, &size, &s) != noErr) {
        s = NULL;
    }
    ca_cfstring(s, out, cap);
}

static int ca_has_output(AudioDeviceID dev) {
    AudioObjectPropertyAddress addr = ca_addr(kAudioDevicePropertyStreams, kAudioObjectPropertyScopeOutput);
    UInt32 size = 0;
    return AudioObjectGetPropertyDataSize(dev, &addr, 0, NULL, &size) == noErr && size > 0;
}

static uint32_t ca_transport_of(AudioDeviceID dev) {
    AudioObjectPropertyAddress addr = ca_addr(kAudioDevicePropertyTransportType, kAudioObjectPropertyScopeGlobal);
    UInt32 t = 0;
    UInt32 size = sizeof t;
    if (AudioObjectGetPropertyData(dev, &addr, 0, NULL, &size, &t) != noErr) {
        return SEL_OUT_TRANSPORT_OTHER;
    }
    switch (t) {
    case kAudioDeviceTransportTypeAirPlay:
        return SEL_OUT_TRANSPORT_AIRPLAY;
    case kAudioDeviceTransportTypeBuiltIn:
        return SEL_OUT_TRANSPORT_BUILTIN;
    case kAudioDeviceTransportTypeUSB:
        return SEL_OUT_TRANSPORT_USB;
    case kAudioDeviceTransportTypeBluetooth:
    case kAudioDeviceTransportTypeBluetoothLE:
        return SEL_OUT_TRANSPORT_BLUETOOTH;
    case kAudioDeviceTransportTypeHDMI:
    case kAudioDeviceTransportTypeDisplayPort:
        return SEL_OUT_TRANSPORT_HDMI;
    default:
        return SEL_OUT_TRANSPORT_OTHER;
    }
}

static uint32_t ca_real_list(sel_out_device* out, uint32_t max) {
    AudioObjectPropertyAddress addr = ca_addr(kAudioHardwarePropertyDevices, kAudioObjectPropertyScopeGlobal);
    AudioDeviceID ids[64];
    UInt32 size = sizeof ids;
    UInt32 i;
    uint32_t n = 0;
    AudioDeviceID dflt = ca_default_device();
    if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &addr, 0, NULL, &size, ids) != noErr) {
        return 0;
    }
    for (i = 0; i < size / sizeof(AudioDeviceID) && n < max; i++) {
        sel_out_device* d = &out[n];
        if (!ca_has_output(ids[i])) {
            continue;
        }
        memset(d, 0, sizeof *d);
        ca_device_text(ids[i], kAudioDevicePropertyDeviceUID, d->uid, sizeof d->uid);
        ca_device_text(ids[i], kAudioObjectPropertyName, d->name, sizeof d->name);
        if (d->uid[0] == '\0') {
            continue;
        }
        d->transport = ca_transport_of(ids[i]);
        d->is_default = ids[i] == dflt;
        n++;
    }
    return n;
}

/* The device id for a uid ("" or an unknown uid: the system default, so a vanished AirPlay speaker falls back). */
static AudioDeviceID ca_device_for_uid(const char* uid) {
    AudioObjectPropertyAddress addr = ca_addr(kAudioHardwarePropertyDevices, kAudioObjectPropertyScopeGlobal);
    AudioDeviceID ids[64];
    UInt32 size = sizeof ids;
    UInt32 i;
    if (uid[0] != '\0' && AudioObjectGetPropertyData(kAudioObjectSystemObject, &addr, 0, NULL, &size, ids) == noErr) {
        for (i = 0; i < size / sizeof(AudioDeviceID); i++) {
            char u[SEL_OUT_DEVICE_UID_MAX];
            ca_device_text(ids[i], kAudioDevicePropertyDeviceUID, u, sizeof u);
            if (strcmp(u, uid) == 0 && ca_has_output(ids[i])) {
                return ids[i];
            }
        }
    }
    return ca_default_device();
}

static int ca_real_bind(ca_out* o, const char* uid) {
    AudioDeviceID dev = ca_device_for_uid(uid);
    int ok;
    if (dev == kAudioObjectUnknown) {
        return 0;
    }
    if (o->inited) {
        AudioUnitUninitialize(o->unit);
    }
    ok = AudioUnitSetProperty(o->unit, kAudioOutputUnitProperty_CurrentDevice, kAudioUnitScope_Global, 0, &dev,
                              sizeof dev) == noErr;
    if (ok) {
        o->dev = dev;
    }
    if (o->inited) {
        ok = AudioUnitInitialize(o->unit) == noErr && ok;
    }
    return ok;
}

static UInt32 ca_u32_prop(AudioObjectID obj, AudioObjectPropertySelector sel, AudioObjectPropertyScope scope) {
    AudioObjectPropertyAddress addr = ca_addr(sel, scope);
    UInt32 v = 0;
    UInt32 size = sizeof v;
    if (AudioObjectGetPropertyData(obj, &addr, 0, NULL, &size, &v) != noErr) {
        return 0;
    }
    return v;
}

/* Device latency + first output stream latency + safety offset + buffer, in milliseconds at the device rate. */
static uint32_t ca_real_latency_ms(ca_out* o) {
    AudioObjectPropertyAddress rate_addr = ca_addr(kAudioDevicePropertyNominalSampleRate, kAudioObjectPropertyScopeGlobal);
    AudioObjectPropertyAddress streams = ca_addr(kAudioDevicePropertyStreams, kAudioObjectPropertyScopeOutput);
    Float64 rate = 0;
    UInt32 size = sizeof rate;
    UInt32 frames;
    AudioStreamID sid[8];
    UInt32 ssize = sizeof sid;
    if (o->dev == kAudioObjectUnknown ||
        AudioObjectGetPropertyData(o->dev, &rate_addr, 0, NULL, &size, &rate) != noErr || rate <= 0) {
        return 0;
    }
    frames = ca_u32_prop(o->dev, kAudioDevicePropertyLatency, kAudioObjectPropertyScopeOutput);
    frames += ca_u32_prop(o->dev, kAudioDevicePropertySafetyOffset, kAudioObjectPropertyScopeOutput);
    frames += ca_u32_prop(o->dev, kAudioDevicePropertyBufferFrameSize, kAudioObjectPropertyScopeGlobal);
    if (AudioObjectGetPropertyData(o->dev, &streams, 0, NULL, &ssize, sid) == noErr && ssize >= sizeof sid[0]) {
        frames += ca_u32_prop(sid[0], kAudioStreamPropertyLatency, kAudioObjectPropertyScopeGlobal);
    }
    return (uint32_t)((double)frames * 1000.0 / rate + 0.5);
}

static OSStatus ca_default_listener(AudioObjectID obj, UInt32 n, const AudioObjectPropertyAddress* a, void* ref) {
    ca_out* o = (ca_out*)ref;
    (void)obj;
    (void)n;
    (void)a;
    __atomic_store_n(&o->default_changed, 1u, __ATOMIC_RELEASE);
    return noErr;
}

static void ca_real_watch(ca_out* o, int on) {
    AudioObjectPropertyAddress addr = ca_addr(kAudioHardwarePropertyDefaultOutputDevice, kAudioObjectPropertyScopeGlobal);
    if (on) {
        AudioObjectAddPropertyListener(kAudioObjectSystemObject, &addr, ca_default_listener, o);
    } else {
        AudioObjectRemovePropertyListener(kAudioObjectSystemObject, &addr, ca_default_listener, o);
    }
}

static int ca_real_start(ca_out* o) {
    return AudioOutputUnitStart(o->unit) == noErr;
}

static void ca_real_stop(ca_out* o) {
    AudioOutputUnitStop(o->unit);
}

static void ca_real_idle(ca_out* o) {
    struct timespec ms = {0, 1000000L};
    (void)o;
    nanosleep(&ms, NULL);
}

static const ca_backend g_real = {ca_real_list, ca_real_bind, ca_real_start, ca_real_stop,
                                  ca_real_watch, ca_real_latency_ms, ca_real_idle};

/* ---- Output ---- */

static const ca_backend* be(void) {
    return g_be != NULL ? g_be : &g_real;
}

/* The device the pref names; "" follows the system default. */
static const char* ca_wanted_uid(const sel_host* host) {
    const char* v = host->config_get != NULL ? host->config_get(host, "output.device", "") : "";
    return v != NULL ? v : "";
}

static int ca_is_route(const char* uid) {
    return strcmp(uid, SEL_OUT_DEVICE_AIRPLAY_ROUTE) == 0;
}

/* Drops the renderer. While the worker runs it destroys the renderer (destroy calls setRate:0, which waits on the
 * device); otherwise no thread uses it and the caller destroys it here. */
static void ca_av_drop(ca_out* o) {
    void* old = NULL;
    if (o->av != NULL) {
        if (o->av_thread_on) {
            pthread_mutex_lock(&o->av_lock);
            /* The worker may still be inside a call on the last doomed renderer: only it may destroy that one. A second
             * pick inside one worker turn is the only way here, so this wait is rare. */
            while (o->av_doomed != NULL) {
                pthread_cond_wait(&o->av_wake, &o->av_lock);
            }
            o->av_doomed = o->av;
            o->av = NULL;
            pthread_cond_broadcast(&o->av_wake);
            pthread_mutex_unlock(&o->av_lock);
        } else {
            old = o->av;
            o->av = NULL;
            o->av_anchored = 0;
            o->av_pts = 0;
        }
    }
    if (old != NULL) {
        av_be()->destroy(old);
    }
}

/* Binds the unit to `uid`. A running unit is stopped and uninitialised first (CurrentDevice cannot change under it).
 * The AirPlay route entry stops the unit and creates the renderer; any other uid drops the renderer. Create stays on
 * the calling thread (it allocates the renderer and does not wait on the device), so a failed create still fails the
 * bind. */
static int ca_bind(ca_out* o, const char* uid) {
    int was_started = o->started;
    if (was_started) {
        be()->stop(o);
        o->started = 0;
    }
    if (ca_is_route(uid)) {
        if (o->av == NULL) {
            void* av = av_be()->create(o->rate);
            if (av == NULL) {
                return 0;
            }
            if (o->av_thread_on) {
                pthread_mutex_lock(&o->av_lock);
                o->av = av;
                pthread_cond_broadcast(&o->av_wake);
                pthread_mutex_unlock(&o->av_lock);
            } else {
                o->av = av;
                o->av_anchored = 0;
                o->av_pts = 0;
            }
        }
        snprintf(o->uid, sizeof o->uid, "%s", uid);
        o->binds++;
        o->latency_ms = CA_AV_AHEAD_MS;
        return 1;
    }
    ca_av_drop(o);
    if (!be()->bind(o, uid)) {
        return 0;
    }
    snprintf(o->uid, sizeof o->uid, "%s", uid);
    o->binds++;
    o->latency_ms = be()->latency_ms(o);
    if (was_started && be()->start(o)) {
        o->started = 1;
    }
    return 1;
}

/* Stops the worker (OUT_CLOSE only). The join waits for the worker's call in flight, at most one renderer call. */
static void ca_av_stop_worker(ca_out* o) {
    if (!o->av_thread_on) {
        return;
    }
    pthread_mutex_lock(&o->av_lock);
    o->av_quit = 1;
    pthread_cond_broadcast(&o->av_wake);
    pthread_mutex_unlock(&o->av_lock);
    pthread_join(o->av_thread, NULL);
    o->av_thread_on = 0;
}

static void ca_free(ca_out* o) {
    const sel_host* host = o->host;
    ca_av_stop_worker(o);
    if (o->av_doomed != NULL) {
        av_be()->destroy(o->av_doomed);
        o->av_doomed = NULL;
    }
    ca_av_drop(o);
    pthread_cond_destroy(&o->av_wake);
    pthread_mutex_destroy(&o->av_lock);
    host->mem_free(host, o->av_buf);
    host->mem_free(host, o);
}

static int ca_has_output_device(void) {
    return ca_default_device() != kAudioObjectUnknown;
}

static void* ca_open(const sel_host* host, uint32_t rate, uint32_t channels) {
    AudioComponentDescription desc;
    AudioComponent comp;
    AudioStreamBasicDescription fmt;
    AURenderCallbackStruct cb;
    ca_out* o;

    if (channels != 2 || rate == 0 || (g_be == NULL && !ca_has_output_device())) {
        return NULL;
    }
    ca_slow_knob();
    o = (ca_out*)host->mem_alloc(host, sizeof *o, SEL_MEM_FAST);
    if (o == NULL) {
        return NULL;
    }
    memset(o, 0, sizeof *o);
    o->host = host;
    o->rate = rate;
    pthread_mutex_init(&o->av_lock, NULL);
    pthread_cond_init(&o->av_wake, NULL);
    o->av_buf = (int16_t*)host->mem_alloc(host, (size_t)CA_AV_CHUNK * 2u * sizeof(int16_t), SEL_MEM_FAST);
    if (o->av_buf == NULL) {
        pthread_cond_destroy(&o->av_wake);
        pthread_mutex_destroy(&o->av_lock);
        host->mem_free(host, o);
        return NULL;
    }
    if (g_be != NULL) { /* a test backend owns the device layer: no AudioUnit */
        if (!ca_bind(o, ca_wanted_uid(host))) {
            ca_free(o);
            return NULL;
        }
        g_be->watch_default(o, 1);
        return o;
    }
    memset(&desc, 0, sizeof desc);
    desc.componentType = kAudioUnitType_Output;
    desc.componentSubType = kAudioUnitSubType_HALOutput;
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;
    comp = AudioComponentFindNext(NULL, &desc);
    if (comp == NULL) {
        ca_free(o);
        return NULL;
    }
    if (AudioComponentInstanceNew(comp, &o->unit) != noErr) {
        ca_free(o);
        return NULL;
    }
    memset(&fmt, 0, sizeof fmt);
    fmt.mSampleRate = (Float64)rate;
    fmt.mFormatID = kAudioFormatLinearPCM;
    fmt.mFormatFlags = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
    fmt.mFramesPerPacket = 1;
    fmt.mChannelsPerFrame = 2;
    fmt.mBitsPerChannel = 16;
    fmt.mBytesPerFrame = 4;
    fmt.mBytesPerPacket = 4;
    cb.inputProc = ca_render;
    cb.inputProcRefCon = o;
    {
        UInt32 off = 0;
        UInt32 on = 1;
        /* A HAL unit has an input bus enabled by default: switch it off, enable the output bus. */
        AudioUnitSetProperty(o->unit, kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Input, 1, &off, sizeof off);
        AudioUnitSetProperty(o->unit, kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Output, 0, &on, sizeof on);
    }
    if (!ca_bind(o, ca_wanted_uid(host)) ||
        AudioUnitSetProperty(o->unit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &fmt,
                             sizeof fmt) != noErr ||
        AudioUnitSetProperty(o->unit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0, &cb,
                             sizeof cb) != noErr ||
        AudioUnitInitialize(o->unit) != noErr) {
        AudioComponentInstanceDispose(o->unit);
        ca_free(o);
        return NULL;
    }
    o->inited = 1;
    be()->watch_default(o, 1);
    if (o->av == NULL) {
        o->latency_ms = be()->latency_ms(o);
    }
    return o;
}

static void ca_close(void* state) {
    ca_out* o = (ca_out*)state;
    if (o) {
        ca_av_stop_worker(o); /* before the unit and the pull source go away */
        be()->watch_default(o, 0);
        if (o->started) {
            be()->stop(o);
        }
        if (g_be == NULL) {
            AudioUnitUninitialize(o->unit);
            AudioComponentInstanceDispose(o->unit);
        }
        ca_free(o);
    }
}

/* Set before the unit starts; the callback only reads it afterwards. */
static void ca_set_source(void* state, sel_pcm_pull fn, void* ctx) {
    ca_out* o = (ca_out*)state;
    o->pull_ctx = ctx;
    o->pull = fn;
}

/* Between pump turns (never on the audio thread): re-target the unit when the pref names another device, or when
 * the pref is empty and the system default moved. An unknown uid binds the default, so the unit follows it too. */
static void ca_retarget(ca_out* o) {
    const char* want = ca_wanted_uid(o->host);
    int moved = __atomic_exchange_n(&o->default_changed, 0u, __ATOMIC_ACQUIRE);
    if (strcmp(want, o->uid) != 0 || (moved && want[0] == '\0')) {
        ca_bind(o, want);
    }
}

/* AirPlay route mode: pull on the pump thread and queue sample buffers up to CA_AV_AHEAD_MS ahead of the synchronizer
 * clock, so the front end paces itself on the route's clock as it does on the HAL device clock. The clock starts at the
 * first queued buffer and is re-anchored when it ran past everything queued (an underrun, or a pause that stopped the
 * pump), so the next buffer is heard instead of dropped as late. Volume is the engine's software gain on the pulled PCM. */
/* One step of the AirPlay route feed: pull up to `n` frames and queue them, unless the renderer already holds
 * CA_AV_AHEAD_MS or is not ready. 1 = queued, 0 = nothing to do now, -1 = enqueue failed, 2 = a flush was requested
 * while pulling (`flush_seen` != the request count), so the chunk belongs to the old position and is dropped. */
static int ca_av_step(ca_out* o, void* rend, uint32_t n, const uint32_t* flush_seen) {
    const ca_av_backend* av = av_be();
    int64_t ahead = (int64_t)o->rate * CA_AV_AHEAD_MS / 1000;
    int64_t played = av->played(rend);
    int64_t pts = o->av_pts;
    uint32_t got;
    if ((o->av_anchored && played < pts && pts - played >= ahead) || !av->ready(rend)) {
        return 0;
    }
    got = o->pull(o->pull_ctx, o->av_buf, n);
    if (got < n) {
        memset(o->av_buf + (size_t)got * 2u, 0, (size_t)(n - got) * 2u * sizeof(int16_t));
    }
    if (flush_seen != NULL && __atomic_load_n(&o->av_flush_req, __ATOMIC_ACQUIRE) != *flush_seen) {
        return 2;
    }
    if (!av->enqueue(rend, o->av_buf, n, pts)) {
        return -1;
    }
    o->av_pts = pts + n;
    if (!o->av_anchored || played >= pts) {
        av->start(rend, pts);
        o->av_anchored = 1;
    }
    __atomic_fetch_add(&o->frames, n, __ATOMIC_RELAXED);
    __atomic_fetch_add(&o->callbacks, 1u, __ATOMIC_RELAXED);
    __atomic_fetch_add(&o->av_buffers, 1u, __ATOMIC_RELAXED);
    return 1;
}

/* Inline feed (test device layer only): the pump queues `frames` itself, as ca_pump did before the worker. */
static uint32_t ca_av_pump_inline(ca_out* o, uint32_t frames) {
    uint32_t done = 0;
    uint32_t idle_ms = 0;
    while (done < frames && idle_ms < 2000u) {
        uint32_t n = frames - done;
        int r;
        if (n > CA_AV_CHUNK) {
            n = CA_AV_CHUNK;
        }
        r = ca_av_step(o, o->av, n, NULL);
        if (r < 0) {
            break;
        }
        if (r == 0) {
            be()->idle(o);
            idle_ms++;
            continue;
        }
        done += n;
        idle_ms = 0;
    }
    return done;
}

/* The worker: adopts the renderer the UI thread bound, destroys the ones it left, applies flush requests and keeps the
 * renderer CA_AV_AHEAD_MS ahead of the synchronizer clock. Every call that waits on the device runs here. */
static void* ca_av_worker(void* arg) {
    ca_out* o = (ca_out*)arg;
    const ca_av_backend* av = av_be();
    void* cur = NULL;
    uint32_t flush_seen = __atomic_load_n(&o->av_flush_req, __ATOMIC_ACQUIRE);
    pthread_mutex_lock(&o->av_lock);
    for (;;) {
        void* rend;
        uint32_t req;
        int r;
        if (o->av_doomed != NULL) {
            void* d = o->av_doomed;
            pthread_mutex_unlock(&o->av_lock);
            av->destroy(d);
            pthread_mutex_lock(&o->av_lock);
            o->av_doomed = NULL;
            pthread_cond_broadcast(&o->av_wake);
            continue;
        }
        if (o->av_quit) {
            break;
        }
        if (o->av == NULL) {
            pthread_cond_wait(&o->av_wake, &o->av_lock);
            continue;
        }
        rend = o->av;
        pthread_mutex_unlock(&o->av_lock);
        if (rend != cur) { /* a fresh renderer: its clock starts at media time 0 */
            cur = rend;
            o->av_pts = 0;
            o->av_anchored = 0;
        }
        req = __atomic_load_n(&o->av_flush_req, __ATOMIC_ACQUIRE);
        if (req != flush_seen) {
            flush_seen = req;
            av->flush(rend);
            o->av_anchored = 0;
            __atomic_fetch_add(&o->av_flushes, 1u, __ATOMIC_RELAXED);
        }
        r = ca_av_step(o, rend, CA_AV_CHUNK, &flush_seen);
        if (r == 0 || r < 0) {
            be()->idle(o);
        }
        pthread_mutex_lock(&o->av_lock);
    }
    pthread_mutex_unlock(&o->av_lock);
    return NULL;
}

/* OUT_FLUSH: a seek or a user song change. On the AirPlay route path the renderer holds up to CA_AV_AHEAD_MS of the old
 * position; drop it and stop the clock, so the next buffer re-anchors the clock and the new position is heard at once.
 * The flush calls setRate:0, which waits on the device, so the UI thread only posts the request and the worker applies
 * it (I1143). The HAL path queues nothing ahead of its render callback. */
static void ca_flush(void* state) {
    ca_out* o = (ca_out*)state;
    if (o->av == NULL) {
        return;
    }
    if (o->av_thread_on) {
        __atomic_fetch_add(&o->av_flush_req, 1u, __ATOMIC_RELEASE);
        return;
    }
    if (g_be == NULL) { /* the worker has not started yet: nothing is queued or running */
        return;
    }
    av_be()->flush(o->av);
    o->av_anchored = 0;
    __atomic_fetch_add(&o->av_flushes, 1u, __ATOMIC_RELAXED);
}

/* AirPlay route pump on the UI thread: wait until the worker queued `frames` more, so the front end paces itself on the
 * route's clock as on the HAL device clock, but never longer than two turns' worth of wall time, so a renderer call
 * that waits on the device (a re-anchor after a flush, a slow route) never holds the UI. */
static uint32_t ca_av_pump(ca_out* o, uint32_t frames) {
    const sel_host* h = o->host;
    uint32_t start = __atomic_load_n(&o->frames, __ATOMIC_RELAXED);
    uint32_t cap_ms = (uint32_t)((uint64_t)frames * 2000u / o->rate) + 1u;
    uint32_t t0 = h->clock_ms(h);
    uint32_t done = 0;
    if (!o->av_thread_on) {
        if (pthread_create(&o->av_thread, NULL, ca_av_worker, o) != 0) {
            return 0;
        }
        o->av_thread_on = 1;
    }
    while (done < frames && h->clock_ms(h) - t0 < cap_ms) {
        be()->idle(o);
        done = __atomic_load_n(&o->frames, __ATOMIC_RELAXED) - start;
    }
    return done < frames ? done : frames;
}

static uint32_t ca_pump(void* state, uint32_t frames) {
    ca_out* o = (ca_out*)state;
    uint32_t start;
    uint32_t done = 0;
    uint32_t idle_ms = 0;
    if (o->pull == NULL) {
        return 0;
    }
    ca_retarget(o);
    if (o->av != NULL) {
        return g_be != NULL ? ca_av_pump_inline(o, frames) : ca_av_pump(o, frames);
    }
    start = __atomic_load_n(&o->frames, __ATOMIC_RELAXED);
    if (!o->started) {
        if (!be()->start(o)) {
            return 0;
        }
        o->started = 1;
    }
    /* Wait on the device clock; give up if the device stops calling back. */
    while (done < frames && idle_ms < 2000u) {
        uint32_t now;
        be()->idle(o);
        now = __atomic_load_n(&o->frames, __ATOMIC_RELAXED) - start;
        if (now == done) {
            idle_ms++;
        } else {
            idle_ms = 0;
        }
        done = now;
    }
    return done < frames ? done : frames;
}

static const sel_tag* ca_stats(void* state) {
    ca_out* o = (ca_out*)state;
    o->stats[0].tag = SEL_OUT_STAT_FRAMES;
    o->stats[0].value = __atomic_load_n(&o->frames, __ATOMIC_RELAXED);
    o->stats[1].tag = SEL_OUT_STAT_CALLBACKS;
    o->stats[1].value = __atomic_load_n(&o->callbacks, __ATOMIC_RELAXED);
    o->stats[2].tag = SEL_OUT_STAT_AV_BUFFERS;
    o->stats[2].value = __atomic_load_n(&o->av_buffers, __ATOMIC_RELAXED);
    o->stats[3].tag = SEL_OUT_STAT_AV_FLUSHES;
    o->stats[3].value = __atomic_load_n(&o->av_flushes, __ATOMIC_RELAXED);
    o->stats[4].tag = SEL_TAG_END;
    o->stats[4].value = 0;
    return o->stats;
}

/* The HAL devices, then the AirPlay route entry (always last, so the list keeps the HAL order users know). */
static uint32_t ca_devices(sel_out_device* out, uint32_t max) {
    uint32_t n;
    if (max == 0u) {
        return 0u;
    }
    n = be()->list(out, max - 1u);
    memset(&out[n], 0, sizeof out[n]);
    snprintf(out[n].uid, sizeof out[n].uid, "%s", SEL_OUT_DEVICE_AIRPLAY_ROUTE);
    snprintf(out[n].name, sizeof out[n].name, "%s", "AirPlay Speakers");
    out[n].transport = SEL_OUT_TRANSPORT_AIRPLAY;
    return n + 1u;
}

/* The live latency of the bound device, re-read after every bind; on the AirPlay route the queue ahead of the clock
 * (the route's own buffering is not public, I1042). */
static uint32_t ca_latency_query(void* state) {
    return ((ca_out*)state)->latency_ms;
}

/* Filled at run time: a function pointer cast to an integer is not a
 * constant expression in C99. */
static sel_tag g_hooks[14];
static sel_tag g_desc[5];

static void set(sel_tag* t, uint32_t tag, uintptr_t value) {
    t->tag = tag;
    t->value = value;
}

const sel_tag* SEL_PLUGIN_ENTRY(uint32_t host_api) {
    if (host_api < 1u) {
        return NULL;
    }
    set(&g_hooks[0], SEL_OUT_NAME, (uintptr_t) "CoreAudio");
    set(&g_hooks[1], SEL_OUT_RATES, (uintptr_t) "any");
    set(&g_hooks[2], SEL_OUT_CHANNELS, 2);
    set(&g_hooks[3], SEL_OUT_LATENCY_MS, 20); /* nominal; SEL_OUT_LATENCY_QUERY has the bound device's real value */
    set(&g_hooks[4], SEL_OUT_OPEN, SEL_TAG_FN(ca_open));
    set(&g_hooks[5], SEL_OUT_CLOSE, SEL_TAG_FN(ca_close));
    set(&g_hooks[6], SEL_OUT_SET_SOURCE, SEL_TAG_FN(ca_set_source));
    set(&g_hooks[7], SEL_OUT_PUMP, SEL_TAG_FN(ca_pump));
    set(&g_hooks[8], SEL_OUT_STATS, SEL_TAG_FN(ca_stats));
    set(&g_hooks[9], SEL_OUT_DEVICES, SEL_TAG_FN(ca_devices));
    set(&g_hooks[10], SEL_OUT_LATENCY_QUERY, SEL_TAG_FN(ca_latency_query));
    set(&g_hooks[11], SEL_OUT_FLUSH, SEL_TAG_FN(ca_flush));
    set(&g_hooks[12], SEL_TAG_END, 0);

    set(&g_desc[0], SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_OUTPUT);
    set(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "coreaudio");
    set(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    set(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    set(&g_desc[4], SEL_TAG_END, 0);
    return g_desc;
}
