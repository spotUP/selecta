/* I1039: the real AVFoundation layer of the CoreAudio output (see coreaudio_av.h). Compiled with ARC.
 * The pattern (s16 LPCM format description, one CMBlockBuffer per chunk, a presentation time in frames, the
 * synchronizer started at the first queued time) is the one the 2026-11-03 probe proved on a HomePod.
 * OS headers are allowed here by RULES.md rule 3 (plugins, os subdirectory). */
#include "coreaudio_av.h"

#import <AVFoundation/AVFoundation.h>
#import <CoreMedia/CoreMedia.h>

@interface SelAvOut : NSObject
@property(nonatomic, strong) AVSampleBufferAudioRenderer* renderer;
@property(nonatomic, strong) AVSampleBufferRenderSynchronizer* sync;
@property(nonatomic) CMAudioFormatDescriptionRef fmt;
@property(nonatomic) int32_t rate;
@end

@implementation SelAvOut
- (void)dealloc {
    if (_fmt != NULL) {
        CFRelease(_fmt);
    }
}
@end

static void* av_create(uint32_t rate) {
    @autoreleasepool {
        SelAvOut* o = [[SelAvOut alloc] init];
        AudioStreamBasicDescription asbd;
        CMAudioFormatDescriptionRef fmt = NULL;
        memset(&asbd, 0, sizeof asbd);
        asbd.mSampleRate = (Float64)rate;
        asbd.mFormatID = kAudioFormatLinearPCM;
        asbd.mFormatFlags = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
        asbd.mBytesPerPacket = 4;
        asbd.mFramesPerPacket = 1;
        asbd.mBytesPerFrame = 4;
        asbd.mChannelsPerFrame = 2;
        asbd.mBitsPerChannel = 16;
        if (CMAudioFormatDescriptionCreate(kCFAllocatorDefault, &asbd, 0, NULL, 0, NULL, NULL, &fmt) != noErr) {
            return NULL;
        }
        o.fmt = fmt;
        o.rate = (int32_t)rate;
        o.renderer = [[AVSampleBufferAudioRenderer alloc] init];
        o.sync = [[AVSampleBufferRenderSynchronizer alloc] init];
        [o.sync addRenderer:o.renderer];
        return (__bridge_retained void*)o;
    }
}

static void av_destroy(void* av) {
    @autoreleasepool {
        SelAvOut* o = (__bridge_transfer SelAvOut*)av;
        [o.sync setRate:0 time:kCMTimeZero];
        [o.renderer flush];
        [o.sync removeRenderer:o.renderer atTime:kCMTimeInvalid completionHandler:nil];
        o = nil;
    }
}

static int av_ready(void* av) {
    SelAvOut* o = (__bridge SelAvOut*)av;
    return o.renderer.isReadyForMoreMediaData ? 1 : 0;
}

static int av_enqueue(void* av, const int16_t* pcm, uint32_t frames, int64_t pts) {
    @autoreleasepool {
        SelAvOut* o = (__bridge SelAvOut*)av;
        size_t bytes = (size_t)frames * 4u;
        CMBlockBufferRef block = NULL;
        CMSampleBufferRef sb = NULL;
        OSStatus st = CMBlockBufferCreateWithMemoryBlock(kCFAllocatorDefault, NULL, bytes, kCFAllocatorDefault, NULL, 0,
                                                         bytes, kCMBlockBufferAssureMemoryNowFlag, &block);
        if (st != noErr) {
            return 0;
        }
        st = CMBlockBufferReplaceDataBytes(pcm, block, 0, bytes);
        if (st == noErr) {
            st = CMAudioSampleBufferCreateReadyWithPacketDescriptions(kCFAllocatorDefault, block, o.fmt,
                                                                      (CMItemCount)frames, CMTimeMake(pts, o.rate),
                                                                      NULL, &sb);
        }
        CFRelease(block);
        if (st != noErr || sb == NULL) {
            return 0;
        }
        [o.renderer enqueueSampleBuffer:sb];
        CFRelease(sb);
        return 1;
    }
}

static int64_t av_played(void* av) {
    SelAvOut* o = (__bridge SelAvOut*)av;
    CMTime t = CMTimeConvertScale(o.sync.currentTime, o.rate, kCMTimeRoundingMethod_RoundTowardZero);
    return CMTIME_IS_VALID(t) ? t.value : 0;
}

static void av_start(void* av, int64_t pts) {
    SelAvOut* o = (__bridge SelAvOut*)av;
    [o.sync setRate:1 time:CMTimeMake(pts, o.rate)];
}

const ca_av_backend ca_av_real = {av_create, av_destroy, av_ready, av_enqueue, av_played, av_start};
