/* CoreAudio output (design doc section 5, plan Phase 6): default output
 * AudioUnit. The unit's render callback runs on the system audio thread and
 * pulls int16 stereo frames from the engine through sel_pcm_pull. The
 * callback allocates nothing and takes no locks: it reads the pull source
 * (set before the unit starts) and bumps two atomic counters. OUT_PUMP
 * starts the unit on first use and sleeps until the requested frames have
 * been rendered, so the front end paces itself on the device clock.
 * OS headers are allowed here by RULES.md rule 3 (plugins, os subdirectory). */
#include <selecta/output.h>

#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudio/CoreAudio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

typedef struct ca_out {
    const sel_host* host;
    AudioUnit unit;
    sel_pcm_pull pull;
    void* pull_ctx;
    uint32_t rate;
    int started;
    uint32_t callbacks; /* atomic */
    uint32_t frames;    /* atomic */
    sel_tag stats[3];
} ca_out;

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

static int ca_has_output_device(void) {
    AudioObjectPropertyAddress addr = {kAudioHardwarePropertyDefaultOutputDevice,
                                       kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain};
    AudioDeviceID dev = kAudioObjectUnknown;
    UInt32 size = sizeof dev;
    if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &addr, 0, NULL, &size, &dev) != noErr) {
        return 0;
    }
    return dev != kAudioObjectUnknown;
}

static void* ca_open(const sel_host* host, uint32_t rate, uint32_t channels) {
    AudioComponentDescription desc;
    AudioComponent comp;
    AudioStreamBasicDescription fmt;
    AURenderCallbackStruct cb;
    ca_out* o;

    if (channels != 2 || rate == 0 || !ca_has_output_device()) {
        return NULL;
    }
    memset(&desc, 0, sizeof desc);
    desc.componentType = kAudioUnitType_Output;
    desc.componentSubType = kAudioUnitSubType_DefaultOutput;
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;
    comp = AudioComponentFindNext(NULL, &desc);
    if (comp == NULL) {
        return NULL;
    }
    o = (ca_out*)host->mem_alloc(host, sizeof *o, SEL_MEM_FAST);
    if (o == NULL) {
        return NULL;
    }
    memset(o, 0, sizeof *o);
    o->host = host;
    o->rate = rate;
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
    if (AudioUnitSetProperty(o->unit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &fmt,
                             sizeof fmt) != noErr ||
        AudioUnitSetProperty(o->unit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0, &cb,
                             sizeof cb) != noErr ||
        AudioUnitInitialize(o->unit) != noErr) {
        AudioComponentInstanceDispose(o->unit);
        host->mem_free(host, o);
        return NULL;
    }
    return o;
}

static void ca_close(void* state) {
    ca_out* o = (ca_out*)state;
    if (o) {
        const sel_host* host = o->host;
        if (o->started) {
            AudioOutputUnitStop(o->unit);
        }
        AudioUnitUninitialize(o->unit);
        AudioComponentInstanceDispose(o->unit);
        host->mem_free(host, o);
    }
}

/* Set before the unit starts; the callback only reads it afterwards. */
static void ca_set_source(void* state, sel_pcm_pull fn, void* ctx) {
    ca_out* o = (ca_out*)state;
    o->pull_ctx = ctx;
    o->pull = fn;
}

static uint32_t ca_pump(void* state, uint32_t frames) {
    ca_out* o = (ca_out*)state;
    uint32_t start;
    uint32_t done = 0;
    uint32_t idle_ms = 0;
    struct timespec ms = {0, 1000000L};
    if (o->pull == NULL) {
        return 0;
    }
    start = __atomic_load_n(&o->frames, __ATOMIC_RELAXED);
    if (!o->started) {
        if (AudioOutputUnitStart(o->unit) != noErr) {
            return 0;
        }
        o->started = 1;
    }
    /* Wait on the device clock; give up if the device stops calling back. */
    while (done < frames && idle_ms < 2000u) {
        uint32_t now;
        nanosleep(&ms, NULL);
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

/* Filled at run time: a function pointer cast to an integer is not a
 * constant expression in C99. */
static sel_tag g_hooks[12];
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
    set(&g_hooks[3], SEL_OUT_LATENCY_MS, 20);
    set(&g_hooks[4], SEL_OUT_OPEN, SEL_TAG_FN(ca_open));
    set(&g_hooks[5], SEL_OUT_CLOSE, SEL_TAG_FN(ca_close));
    set(&g_hooks[6], SEL_OUT_SET_SOURCE, SEL_TAG_FN(ca_set_source));
    set(&g_hooks[7], SEL_OUT_PUMP, SEL_TAG_FN(ca_pump));
    set(&g_hooks[8], SEL_OUT_STATS, SEL_TAG_FN(ca_stats));
    set(&g_hooks[9], SEL_TAG_END, 0);

    set(&g_desc[0], SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_OUTPUT);
    set(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "coreaudio");
    set(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    set(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    set(&g_desc[4], SEL_TAG_END, 0);
    return g_desc;
}
