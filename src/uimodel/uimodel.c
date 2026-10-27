/* UI model: see include/selecta/uimodel.h. */
#include <selecta/uimodel.h>

#include <stdio.h>
#include <string.h>

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
    uint32_t pos = st->play == SEL_STOPPED ? 0u : st->song.position_ms; /* I548: STOP keeps the slot loaded and its frozen position; a stopped transport reads 00:00 */
    uint32_t changed = 0;
    uint32_t i;

    memset(&n, 0, sizeof n);
    if ((title == NULL || title[0] == '\0') && st->song.path != NULL) {
        title = basename_of(st->song.path);
    }
    snprintf(n.title, sizeof n.title, "%s", title != NULL ? title : "");
    snprintf(n.artist, sizeof n.artist, "%s", artist != NULL ? artist : "");
    snprintf(n.format, sizeof n.format, "%s", format != NULL ? format : "");
    sel_uim_clock(n.clock, sizeof n.clock, pos, st->song.path != NULL);
    if (dur != 0u) {
        n.remaining[0] = '-';
        sel_uim_clock(n.remaining + 1, sizeof n.remaining - 1u, dur > pos ? dur - pos : 0u, 1);
        sel_uim_clock(n.length, sizeof n.length, dur, 1);
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
    n.subsong = st->song.subsong + 1u;
    n.subsong_count = st->song.subsong_count;
    n.repeat_mode = strcmp(st->options[SEL_OPT_REPEAT], "all") == 0 ? 1u : strcmp(st->options[SEL_OPT_REPEAT], "one") == 0 ? 2u : 0u;
    n.random_on = strcmp(st->options[SEL_OPT_RANDOM], "on") == 0;
    n.lyric_slot = st->lyric_lines > 0u ? st->lyric_line + 1 : 0;
    n.panel_serial = st->panel_serial;
    /* The status line carries the play state and the mixer stage settings: the clock and the title have their own regions, so a song change
     * redraws the song and queue widgets and nothing else. */
    snprintf(n.status, sizeof n.status, "%s", sel_uim_play_word(n.play));
    sel_uim_stage_status(n.status, sizeof n.status, st);

    if (n.lyric_slot != o->lyric_slot || n.panel_serial != o->panel_serial || strcmp(n.title, o->title) != 0 || strcmp(n.artist, o->artist) != 0 || strcmp(n.format, o->format) != 0) {
        changed |= SEL_UIM_DIRTY_SONG;
    }
    if (n.queue_count != o->queue_count || n.queue_playing != o->queue_playing) {
        changed |= SEL_UIM_DIRTY_QUEUE;
    }
    if (n.play != o->play || n.can_prev != o->can_prev || n.can_next != o->can_next ||
        n.subsong != o->subsong || n.subsong_count != o->subsong_count || n.repeat_mode != o->repeat_mode ||
        n.random_on != o->random_on) {
        changed |= SEL_UIM_DIRTY_TRANSPORT;
    }
    if (strcmp(n.clock, o->clock) != 0 || strcmp(n.remaining, o->remaining) != 0 || strcmp(n.length, o->length) != 0 || n.progress_q16 != o->progress_q16) {
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
    } else if (strcmp(mode, "visual") == 0) {
        layout = SEL_UIM_LAYOUT_VISUAL;
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
