/* UI model: see include/selecta/uimodel.h. */
#include <selecta/uimodel.h>

#include <stdio.h>
#include <string.h>

static void put_clock(char* out, size_t cap, uint32_t ms, const char* sign) {
    uint32_t s = ms / 1000u;
    snprintf(out, cap, "%s%u:%02u", sign, (unsigned)(s / 60u), (unsigned)(s % 60u));
}

static const char* basename_of(const char* path) {
    const char* b = path;
    const char* p;
    for (p = path; *p != '\0'; p++) {
        if (*p == '/' || *p == ':') {
            b = p + 1;
        }
    }
    return b;
}

static const char* state_label(sel_play_state s) {
    return s == SEL_PLAYING ? "Playing" : s == SEL_PAUSED ? "Paused" : "Stopped";
}

void sel_uim_init(sel_uim* uim) {
    memset(uim, 0, sizeof *uim);
    uim->dirty = SEL_UIM_DIRTY_ALL;
}

uint32_t sel_uim_update(sel_uim* uim, const sel_state* st) {
    sel_uim_frame n;
    const sel_uim_frame* o = &uim->frame;
    const sel_tag* meta = st->song.path != NULL ? st->song.meta : NULL;
    const char* title = meta != NULL ? (const char*)sel_tag_find(meta, SEL_META_TITLE, 0) : NULL;
    const char* artist = meta != NULL ? (const char*)sel_tag_find(meta, SEL_META_AUTHOR, 0) : NULL;
    const char* format = meta != NULL ? (const char*)sel_tag_find(meta, SEL_META_FORMAT, 0) : NULL;
    uint32_t dur = st->song.duration_ms;
    uint32_t pos = st->song.position_ms;
    uint32_t changed = 0;
    uint32_t i;

    memset(&n, 0, sizeof n);
    if ((title == NULL || title[0] == '\0') && st->song.path != NULL) {
        title = basename_of(st->song.path);
    }
    snprintf(n.title, sizeof n.title, "%s", title != NULL ? title : "");
    snprintf(n.artist, sizeof n.artist, "%s", artist != NULL ? artist : "");
    snprintf(n.format, sizeof n.format, "%s", format != NULL ? format : "");
    put_clock(n.clock, sizeof n.clock, pos, "");
    if (dur != 0u) {
        put_clock(n.remaining, sizeof n.remaining, dur > pos ? dur - pos : 0u, "-");
        n.progress_q16 = pos >= dur ? 65536u : (uint32_t)(((uint64_t)pos << 16) / dur);
    }
    n.play = st->play;
    n.queue_count = st->queue_count;
    n.queue_playing = st->queue_playing;
    for (i = 0; i < st->queue_count; i++) {
        if (st->queue[i].id == st->queue_playing) {
            n.can_prev = i > 0u;
            n.can_next = i + 1u < st->queue_count;
        }
    }
    n.volume = st->volume;
    /* The status line carries the play state alone: the clock and the title have their own regions, so a song change
     * redraws the song and queue widgets and nothing else. */
    snprintf(n.status, sizeof n.status, "%s", state_label(n.play));

    if (strcmp(n.title, o->title) != 0 || strcmp(n.artist, o->artist) != 0 || strcmp(n.format, o->format) != 0) {
        changed |= SEL_UIM_DIRTY_SONG;
    }
    if (n.queue_count != o->queue_count || n.queue_playing != o->queue_playing) {
        changed |= SEL_UIM_DIRTY_QUEUE;
    }
    if (n.play != o->play || n.can_prev != o->can_prev || n.can_next != o->can_next) {
        changed |= SEL_UIM_DIRTY_TRANSPORT;
    }
    if (strcmp(n.clock, o->clock) != 0 || strcmp(n.remaining, o->remaining) != 0 || n.progress_q16 != o->progress_q16) {
        changed |= SEL_UIM_DIRTY_PROGRESS;
    }
    if (n.volume != o->volume) {
        changed |= SEL_UIM_DIRTY_VOLUME;
    }
    if (strcmp(n.status, o->status) != 0) {
        changed |= SEL_UIM_DIRTY_STATUS;
    }
    uim->frame = n;
    uim->updates++;
    uim->dirty |= changed;
    return changed;
}

int sel_uim_set_layout(sel_uim* uim, const char* mode) {
    uint32_t layout;
    if (strcmp(mode, "full") == 0) {
        layout = SEL_UIM_LAYOUT_FULL;
    } else if (strcmp(mode, "window") == 0) {
        layout = SEL_UIM_LAYOUT_WINDOW;
    } else if (strcmp(mode, "mini") == 0) {
        layout = SEL_UIM_LAYOUT_MINI;
    } else {
        return 0;
    }
    if (layout != uim->layout) {
        uim->layout = layout;
        uim->dirty = SEL_UIM_DIRTY_ALL;
    }
    return 1;
}

uint32_t sel_uim_take_dirty(sel_uim* uim) {
    uint32_t d = uim->dirty;
    uim->dirty = 0u;
    if (d != 0u) {
        uim->last_taken = d;
    }
    return d;
}
