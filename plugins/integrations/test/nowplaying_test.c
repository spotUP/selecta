/* nowplaying-test: the fake integration backend for test T38 (plan M1.2 phase
 * 6). It mirrors the player the way the OS plugins do (publish a title,
 * duration and play state whenever one changes) and prints each publish, so a
 * script can assert what a real Now Playing backend would have been given. On
 * its first poll it issues one fake remote command, `next`, as NEXT. On close
 * it prints `np_publish=<n> np_command=<c>`. */
#include <selecta/engine.h>
#include <selecta/host.h>
#include <selecta/integration.h>
#include <selecta/plugin.h>
#include <selecta/tags.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct np_state {
    sel_int_host ih;
    unsigned publishes;
    unsigned commands;
    int polled;
    int have_last;
    char title[128];
    uint32_t duration_ms;
    sel_play_state play;
} np_state;

static const char* play_name(sel_play_state p) {
    return p == SEL_PLAYING ? "playing" : p == SEL_PAUSED ? "paused" : "stopped";
}

static void* np_open(const sel_host* host, const sel_int_host* ih) {
    np_state* s = calloc(1, sizeof *s);
    (void)host;
    if (s != NULL) {
        s->ih = *ih;
    }
    return s;
}

static void np_on_event(void* state, const sel_message* evt, const sel_state* snap) {
    np_state* s = state;
    const char* title = NULL;
    if (evt->id != SEL_EVT_STATE && evt->id != SEL_EVT_SONG_OPENED) {
        return;
    }
    if (snap->song.meta != NULL) {
        title = (const char*)sel_tag_find(snap->song.meta, SEL_META_TITLE, 0);
    }
    if (title == NULL) {
        title = "";
    }
    if (s->have_last && strcmp(title, s->title) == 0 && snap->song.duration_ms == s->duration_ms &&
        snap->play == s->play) {
        return;
    }
    s->have_last = 1;
    snprintf(s->title, sizeof s->title, "%s", title);
    s->duration_ms = snap->song.duration_ms;
    s->play = snap->play;
    s->publishes++;
    printf("np_record title=%s duration_ms=%u state=%s\n", s->title, (unsigned)s->duration_ms, play_name(s->play));
}

static void np_poll(void* state) {
    np_state* s = state;
    if (!s->polled) {
        s->polled = 1;
        s->commands++;
        s->ih.send_command(s->ih.ctx, "NEXT");
    }
}

static void np_close(void* state) {
    np_state* s = state;
    printf("np_publish=%u np_command=%u\n", s->publishes, s->commands);
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
    set(&g_hooks[0], SEL_INT_NAME, (uintptr_t) "Now Playing test backend");
    set(&g_hooks[1], SEL_INT_OPEN, SEL_TAG_FN(np_open));
    set(&g_hooks[2], SEL_INT_ON_EVENT, SEL_TAG_FN(np_on_event));
    set(&g_hooks[3], SEL_INT_POLL, SEL_TAG_FN(np_poll));
    set(&g_hooks[4], SEL_INT_CLOSE, SEL_TAG_FN(np_close));
    set(&g_hooks[5], SEL_TAG_END, 0);

    set(&g_desc[0], SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_INTEGRATION);
    set(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "nowplaying-test");
    set(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    set(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    set(&g_desc[4], SEL_TAG_END, 0);
    return g_desc;
}
