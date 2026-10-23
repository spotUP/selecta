/* Linux MPRIS integration (plan M1.2 phase 6, row I81). Exposes
 * org.mpris.MediaPlayer2.selecta on the session bus through sd-bus: playback
 * status, metadata and position out, Play / Pause / PlayPause / Stop / Next /
 * Previous / Seek in as protocol lines. Built only when libsystemd is found.
 *
 * sd-bus method handlers run inside sd_bus_process, which INT_POLL calls once
 * per shell turn, so every command is queued from INT_POLL and never from
 * INT_ON_EVENT. */
#include <systemd/sd-bus.h>

#include <selecta/engine.h>
#include <selecta/host.h>
#include <selecta/integration.h>
#include <selecta/plugin.h>
#include <selecta/tags.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MPRIS_PATH "/org/mpris/MediaPlayer2"
#define MPRIS_ROOT "org.mpris.MediaPlayer2"
#define MPRIS_PLAYER "org.mpris.MediaPlayer2.Player"

typedef struct np_state {
    sel_int_host ih;
    sd_bus* bus;
    sd_bus_slot* slot_root;
    sd_bus_slot* slot_player;
    sel_play_state play;
    char title[128];
    char artist[128];
    uint32_t duration_ms;
    uint32_t position_ms;
} np_state;

static int send_line(sd_bus_message* m, void* ud, const char* line) {
    np_state* s = ud;
    s->ih.send_command(s->ih.ctx, line);
    return sd_bus_reply_method_return(m, NULL);
}

static int m_play(sd_bus_message* m, void* ud, sd_bus_error* e) {
    (void)e;
    return send_line(m, ud, "PLAY");
}
static int m_pause(sd_bus_message* m, void* ud, sd_bus_error* e) {
    (void)e;
    return send_line(m, ud, "PAUSE_PLAYBACK on");
}
static int m_playpause(sd_bus_message* m, void* ud, sd_bus_error* e) {
    (void)e;
    return send_line(m, ud, "PAUSE");
}
static int m_stop(sd_bus_message* m, void* ud, sd_bus_error* e) {
    (void)e;
    return send_line(m, ud, "STOP");
}
static int m_next(sd_bus_message* m, void* ud, sd_bus_error* e) {
    (void)e;
    return send_line(m, ud, "NEXT");
}
static int m_prev(sd_bus_message* m, void* ud, sd_bus_error* e) {
    (void)e;
    return send_line(m, ud, "PREV");
}
static int m_noop(sd_bus_message* m, void* ud, sd_bus_error* e) {
    (void)ud;
    (void)e;
    return sd_bus_reply_method_return(m, NULL);
}

static int m_seek(sd_bus_message* m, void* ud, sd_bus_error* e) {
    np_state* s = ud;
    int64_t offset_us = 0;
    char line[48];
    int64_t target;
    (void)e;
    if (sd_bus_message_read(m, "x", &offset_us) < 0) {
        return -1;
    }
    target = (int64_t)s->position_ms + offset_us / 1000;
    snprintf(line, sizeof line, "SEEK %u", (unsigned)(target < 0 ? 0 : target));
    return send_line(m, ud, line);
}

static int p_status(sd_bus* bus, const char* path, const char* iface, const char* prop, sd_bus_message* reply,
                    void* ud, sd_bus_error* err) {
    np_state* s = ud;
    (void)bus, (void)path, (void)iface, (void)prop, (void)err;
    return sd_bus_message_append(reply, "s",
                                 s->play == SEL_PLAYING ? "Playing" : s->play == SEL_PAUSED ? "Paused" : "Stopped");
}

static int p_position(sd_bus* bus, const char* path, const char* iface, const char* prop, sd_bus_message* reply,
                      void* ud, sd_bus_error* err) {
    np_state* s = ud;
    (void)bus, (void)path, (void)iface, (void)prop, (void)err;
    return sd_bus_message_append(reply, "x", (int64_t)s->position_ms * 1000);
}

static int p_metadata(sd_bus* bus, const char* path, const char* iface, const char* prop, sd_bus_message* reply,
                      void* ud, sd_bus_error* err) {
    np_state* s = ud;
    const char* artists[1];
    (void)bus, (void)path, (void)iface, (void)prop, (void)err;
    artists[0] = s->artist;
    sd_bus_message_open_container(reply, 'a', "{sv}");
    sd_bus_message_append(reply, "{sv}", "mpris:trackid", "o", "/org/selecta/track/current");
    sd_bus_message_append(reply, "{sv}", "xesam:title", "s", s->title);
    if (s->artist[0] != '\0') {
        sd_bus_message_open_container(reply, 'e', "sv");
        sd_bus_message_append(reply, "s", "xesam:artist");
        sd_bus_message_open_container(reply, 'v', "as");
        sd_bus_message_append_strv(reply, (char**)(void*)((const char*[]){artists[0], NULL}));
        sd_bus_message_close_container(reply);
        sd_bus_message_close_container(reply);
    }
    sd_bus_message_append(reply, "{sv}", "mpris:length", "x", (int64_t)s->duration_ms * 1000);
    return sd_bus_message_close_container(reply);
}

static int p_false(sd_bus* bus, const char* path, const char* iface, const char* prop, sd_bus_message* reply,
                   void* ud, sd_bus_error* err) {
    (void)bus, (void)path, (void)iface, (void)prop, (void)ud, (void)err;
    return sd_bus_message_append(reply, "b", 0);
}

static int p_identity(sd_bus* bus, const char* path, const char* iface, const char* prop, sd_bus_message* reply,
                      void* ud, sd_bus_error* err) {
    (void)bus, (void)path, (void)iface, (void)prop, (void)ud, (void)err;
    return sd_bus_message_append(reply, "s", "Selecta");
}

static const sd_bus_vtable root_vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("Raise", "", "", m_noop, 0),
    SD_BUS_METHOD("Quit", "", "", m_noop, 0),
    SD_BUS_PROPERTY("CanQuit", "b", p_false, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("CanRaise", "b", p_false, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("HasTrackList", "b", p_false, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("Identity", "s", p_identity, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_VTABLE_END};

static const sd_bus_vtable player_vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("Play", "", "", m_play, 0),
    SD_BUS_METHOD("Pause", "", "", m_pause, 0),
    SD_BUS_METHOD("PlayPause", "", "", m_playpause, 0),
    SD_BUS_METHOD("Stop", "", "", m_stop, 0),
    SD_BUS_METHOD("Next", "", "", m_next, 0),
    SD_BUS_METHOD("Previous", "", "", m_prev, 0),
    SD_BUS_METHOD("Seek", "x", "", m_seek, 0),
    SD_BUS_PROPERTY("PlaybackStatus", "s", p_status, 0, SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_PROPERTY("Metadata", "a{sv}", p_metadata, 0, SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_PROPERTY("Position", "x", p_position, 0, 0),
    SD_BUS_VTABLE_END};

static void* np_open(const sel_host* host, const sel_int_host* ih) {
    np_state* s = calloc(1, sizeof *s);
    (void)host;
    if (s == NULL) {
        return NULL;
    }
    s->ih = *ih;
    if (sd_bus_open_user(&s->bus) < 0 ||
        sd_bus_add_object_vtable(s->bus, &s->slot_root, MPRIS_PATH, MPRIS_ROOT, root_vtable, s) < 0 ||
        sd_bus_add_object_vtable(s->bus, &s->slot_player, MPRIS_PATH, MPRIS_PLAYER, player_vtable, s) < 0 ||
        sd_bus_request_name(s->bus, "org.mpris.MediaPlayer2.selecta", 0) < 0) {
        sd_bus_flush_close_unref(s->bus);
        free(s);
        return NULL; /* no session bus: the shell runs without the integration */
    }
    return s;
}

static void np_on_event(void* state, const sel_message* evt, const sel_state* snap) {
    np_state* s = state;
    const char* title = NULL;
    const char* author = NULL;
    if (evt->id != SEL_EVT_STATE && evt->id != SEL_EVT_SONG_OPENED && evt->id != SEL_EVT_SEEKED &&
        evt->id != SEL_EVT_SONG_ENDED) {
        return;
    }
    if (snap->song.meta != NULL) {
        title = (const char*)sel_tag_find(snap->song.meta, SEL_META_TITLE, 0);
        author = (const char*)sel_tag_find(snap->song.meta, SEL_META_AUTHOR, 0);
    }
    snprintf(s->title, sizeof s->title, "%s", title != NULL ? title : "");
    snprintf(s->artist, sizeof s->artist, "%s", author != NULL ? author : "");
    s->duration_ms = snap->song.duration_ms;
    s->position_ms = snap->song.position_ms;
    s->play = snap->play;
    sd_bus_emit_properties_changed(s->bus, MPRIS_PATH, MPRIS_PLAYER, "PlaybackStatus", "Metadata", NULL);
}

static void np_poll(void* state) {
    np_state* s = state;
    int r;
    do {
        r = sd_bus_process(s->bus, NULL);
    } while (r > 0);
}

static void np_close(void* state) {
    np_state* s = state;
    sd_bus_slot_unref(s->slot_player);
    sd_bus_slot_unref(s->slot_root);
    sd_bus_flush_close_unref(s->bus);
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
    set(&g_hooks[0], SEL_INT_NAME, (uintptr_t) "MPRIS");
    set(&g_hooks[1], SEL_INT_OPEN, SEL_TAG_FN(np_open));
    set(&g_hooks[2], SEL_INT_ON_EVENT, SEL_TAG_FN(np_on_event));
    set(&g_hooks[3], SEL_INT_POLL, SEL_TAG_FN(np_poll));
    set(&g_hooks[4], SEL_INT_CLOSE, SEL_TAG_FN(np_close));
    set(&g_hooks[5], SEL_TAG_END, 0);

    set(&g_desc[0], SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_INTEGRATION);
    set(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "mpris");
    set(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    set(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    set(&g_desc[4], SEL_TAG_END, 0);
    return g_desc;
}
