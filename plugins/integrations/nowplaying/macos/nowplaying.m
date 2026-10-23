/* macOS Now Playing integration (plan M1.2 phase 6, row I81). Publishes title,
 * artist, duration, position and play state to MPNowPlayingInfoCenter and
 * turns the media keys and Control Center buttons (MPRemoteCommandCenter)
 * into protocol lines. Objective-C lives only in this plugin.
 *
 * Remote command handlers run on the main thread's run loop. INT_POLL spins
 * that run loop for zero seconds once per shell turn, so every handler runs
 * inside INT_POLL and queues its line there, never from INT_ON_EVENT. */
#import <Foundation/Foundation.h>
#import <MediaPlayer/MediaPlayer.h>

#include <selecta/engine.h>
#include <selecta/host.h>
#include <selecta/integration.h>
#include <selecta/plugin.h>
#include <selecta/tags.h>

#include <stdio.h>
#include <stdlib.h>

typedef struct np_state {
    sel_int_host ih;
    __strong id targets[6]; /* command handler tokens, removed on close */
} np_state;

static void publish(const sel_state* snap) {
    MPNowPlayingInfoCenter* center = [MPNowPlayingInfoCenter defaultCenter];
    NSMutableDictionary* info = [NSMutableDictionary dictionary];
    const char* title = snap->song.meta != NULL ? (const char*)sel_tag_find(snap->song.meta, SEL_META_TITLE, 0) : NULL;
    const char* author =
        snap->song.meta != NULL ? (const char*)sel_tag_find(snap->song.meta, SEL_META_AUTHOR, 0) : NULL;
    if (snap->song.path == NULL) {
        center.nowPlayingInfo = nil;
        center.playbackState = MPNowPlayingPlaybackStateStopped;
        return;
    }
    if (title != NULL && title[0] != '\0') {
        info[MPMediaItemPropertyTitle] = [NSString stringWithUTF8String:title];
    } else {
        info[MPMediaItemPropertyTitle] = [[NSString stringWithUTF8String:snap->song.path] lastPathComponent];
    }
    if (author != NULL && author[0] != '\0') {
        info[MPMediaItemPropertyArtist] = [NSString stringWithUTF8String:author];
    }
    if (snap->song.duration_ms > 0) {
        info[MPMediaItemPropertyPlaybackDuration] = @(snap->song.duration_ms / 1000.0);
    }
    info[MPNowPlayingInfoPropertyElapsedPlaybackTime] = @(snap->song.position_ms / 1000.0);
    info[MPNowPlayingInfoPropertyPlaybackRate] = @(snap->play == SEL_PLAYING ? 1.0 : 0.0);
    center.nowPlayingInfo = info;
    center.playbackState = snap->play == SEL_PLAYING   ? MPNowPlayingPlaybackStatePlaying
                           : snap->play == SEL_PAUSED ? MPNowPlayingPlaybackStatePaused
                                                      : MPNowPlayingPlaybackStateStopped;
}

static void* np_open(const sel_host* host, const sel_int_host* ih) {
    np_state* s = calloc(1, sizeof *s);
    MPRemoteCommandCenter* rc;
    sel_int_host hostcopy;
    (void)host;
    if (s == NULL) {
        return NULL;
    }
    s->ih = *ih;
    hostcopy = *ih;
    rc = [MPRemoteCommandCenter sharedCommandCenter];
    typedef struct {
        MPRemoteCommand* cmd;
        const char* line;
    } binding;
    binding b[5] = {{rc.playCommand, "PLAY"},
                    {rc.pauseCommand, "PAUSE_PLAYBACK on"},
                    {rc.togglePlayPauseCommand, "PAUSE"},
                    {rc.nextTrackCommand, "NEXT"},
                    {rc.previousTrackCommand, "PREV"}};
    for (int i = 0; i < 5; i++) {
        const char* line = b[i].line;
        b[i].cmd.enabled = YES;
        s->targets[i] = [b[i].cmd addTargetWithHandler:^MPRemoteCommandHandlerStatus(MPRemoteCommandEvent* ev) {
          (void)ev;
          return hostcopy.send_command(hostcopy.ctx, line) == 0 ? MPRemoteCommandHandlerStatusSuccess
                                                                 : MPRemoteCommandHandlerStatusCommandFailed;
        }];
    }
    rc.changePlaybackPositionCommand.enabled = YES;
    s->targets[5] = [rc.changePlaybackPositionCommand
        addTargetWithHandler:^MPRemoteCommandHandlerStatus(MPRemoteCommandEvent* ev) {
          char line[48];
          MPChangePlaybackPositionCommandEvent* pe = (MPChangePlaybackPositionCommandEvent*)ev;
          snprintf(line, sizeof line, "SEEK %u", (unsigned)(pe.positionTime * 1000.0));
          return hostcopy.send_command(hostcopy.ctx, line) == 0 ? MPRemoteCommandHandlerStatusSuccess
                                                                 : MPRemoteCommandHandlerStatusCommandFailed;
        }];
    return s;
}

static void np_on_event(void* state, const sel_message* evt, const sel_state* snap) {
    (void)state;
    if (evt->id == SEL_EVT_STATE || evt->id == SEL_EVT_SONG_OPENED || evt->id == SEL_EVT_SEEKED ||
        evt->id == SEL_EVT_SONG_ENDED) {
        publish(snap);
    }
}

static void np_poll(void* state) {
    (void)state;
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0, true);
}

static void np_close(void* state) {
    np_state* s = state;
    MPRemoteCommandCenter* rc = [MPRemoteCommandCenter sharedCommandCenter];
    MPRemoteCommand* cmds[6] = {rc.playCommand,          rc.pauseCommand,         rc.togglePlayPauseCommand,
                                rc.nextTrackCommand,     rc.previousTrackCommand, rc.changePlaybackPositionCommand};
    for (int i = 0; i < 6; i++) {
        if (s->targets[i] != nil) {
            [cmds[i] removeTarget:s->targets[i]];
            s->targets[i] = nil;
        }
    }
    [MPNowPlayingInfoCenter defaultCenter].nowPlayingInfo = nil;
    free(s);
}

static sel_tag g_hooks[7];
static sel_tag g_desc[5];

static void set(sel_tag* t, uint32_t tag, uintptr_t value) {
    t->tag = tag;
    t->value = value;
}

const sel_tag* SEL_PLUGIN_ENTRY(uint32_t host_api) {
    if (host_api < 1u) {
        return NULL;
    }
    set(&g_hooks[0], SEL_INT_NAME, (uintptr_t) "macOS Now Playing");
    set(&g_hooks[1], SEL_INT_OPEN, SEL_TAG_FN(np_open));
    set(&g_hooks[2], SEL_INT_ON_EVENT, SEL_TAG_FN(np_on_event));
    set(&g_hooks[3], SEL_INT_POLL, SEL_TAG_FN(np_poll));
    set(&g_hooks[4], SEL_INT_CLOSE, SEL_TAG_FN(np_close));
    set(&g_hooks[5], SEL_TAG_END, 0);

    set(&g_desc[0], SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_INTEGRATION);
    set(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "nowplaying");
    set(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    set(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    set(&g_desc[4], SEL_TAG_END, 0);
    return g_desc;
}
