/* CoreAudio output (design doc section 5, plan Phase 6, I1039): a HAL output AudioUnit. The unit plays to the device
 * the pref output.device names (a CoreAudio device uid), or follows the system default output when the pref is empty,
 * also when the default changes mid-play (an AirPlay speaker picked in the system menu moves playback). The unit's
 * render callback runs on the system audio thread and pulls int16 stereo frames from the engine through sel_pcm_pull.
 * The callback allocates nothing and takes no locks: it reads the pull source (set before the unit starts) and bumps
 * two atomic counters. OUT_PUMP starts the unit on first use, re-targets it when the pref or the default changed, and
 * sleeps until the requested frames have been rendered, so the front end paces itself on the device clock.
 * Every CoreAudio call outside the render callback goes through ca_backend, so the host test drives the same code with
 * a fake device list and a counter instead of a sound card (tests/coreaudio_device_test.c).
 * OS headers are allowed here by RULES.md rule 3 (plugins, os subdirectory). */
#include <selecta/output.h>

#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudio/CoreAudio.h>
#include <stdint.h>
#include <stdio.h>
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
    sel_tag stats[3];
    char uid[SEL_OUT_DEVICE_UID_MAX]; /* the pref value the unit is bound to ("" = system default, followed) */
    AudioDeviceID dev;                /* the device the unit plays to */
    uint32_t default_changed;         /* atomic: the system default output moved since the last bind */
    uint32_t binds;                   /* how many times the unit was bound to a device (the first open counts) */
    uint32_t latency_ms;              /* real latency of the bound device */
};

static const ca_backend* g_be;

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

/* Binds the unit to `uid`. A running unit is stopped and uninitialised first (CurrentDevice cannot change under it). */
static int ca_bind(ca_out* o, const char* uid) {
    int was_started = o->started;
    if (was_started) {
        be()->stop(o);
        o->started = 0;
    }
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
    o = (ca_out*)host->mem_alloc(host, sizeof *o, SEL_MEM_FAST);
    if (o == NULL) {
        return NULL;
    }
    memset(o, 0, sizeof *o);
    o->host = host;
    o->rate = rate;
    if (g_be != NULL) { /* a test backend owns the device layer: no AudioUnit */
        if (!ca_bind(o, ca_wanted_uid(host))) {
            host->mem_free(host, o);
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
        host->mem_free(host, o);
        return NULL;
    }
    if (AudioComponentInstanceNew(comp, &o->unit) != noErr) {
        host->mem_free(host, o);
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
        host->mem_free(host, o);
        return NULL;
    }
    o->inited = 1;
    be()->watch_default(o, 1);
    o->latency_ms = be()->latency_ms(o);
    return o;
}

static void ca_close(void* state) {
    ca_out* o = (ca_out*)state;
    if (o) {
        const sel_host* host = o->host;
        be()->watch_default(o, 0);
        if (o->started) {
            be()->stop(o);
        }
        if (g_be == NULL) {
            AudioUnitUninitialize(o->unit);
            AudioComponentInstanceDispose(o->unit);
        }
        host->mem_free(host, o);
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

static uint32_t ca_pump(void* state, uint32_t frames) {
    ca_out* o = (ca_out*)state;
    uint32_t start;
    uint32_t done = 0;
    uint32_t idle_ms = 0;
    if (o->pull == NULL) {
        return 0;
    }
    ca_retarget(o);
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
    o->stats[2].tag = SEL_TAG_END;
    o->stats[2].value = 0;
    return o->stats;
}

static uint32_t ca_devices(sel_out_device* out, uint32_t max) {
    return be()->list(out, max);
}

/* The live latency of the bound device, re-read after every bind. */
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
    set(&g_hooks[11], SEL_TAG_END, 0);

    set(&g_desc[0], SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_OUTPUT);
    set(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "coreaudio");
    set(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    set(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    set(&g_desc[4], SEL_TAG_END, 0);
    return g_desc;
}
