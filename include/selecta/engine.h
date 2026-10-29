/* The engine: one state machine behind two single-producer single-consumer
 * queues (design doc section 2). Commands go in with sel_engine_send,
 * sel_engine_pump dispatches them and hands the resulting events to a
 * sink. All memory is taken in sel_engine_create; send, pump and render
 * never allocate. */
#ifndef SELECTA_ENGINE_H
#define SELECTA_ENGINE_H

#include <selecta/host.h>
#include <selecta/protocol.h>
#include <selecta/replayer.h>
#include <selecta/tags.h>
#include <selecta/viz.h>
#include <selecta/voices.h>

SEL_EXTERN_C_BEGIN

typedef enum sel_play_state { SEL_STOPPED = 0, SEL_PLAYING, SEL_PAUSED } sel_play_state;

/* Option names from design section 3, in table order. */
#define SEL_OPTIONS(X)                                                                        \
    X(CONSUME, "consume")                                                                     \
    X(SINGLE, "single")                                                                       \
    X(RANDOM, "random")                                                                       \
    X(REPEAT, "repeat")                                                                       \
    X(CROSSFADE, "crossfade")                                                                 \
    X(STOP_AFTER_CURRENT, "stop-after-current")                                               \
    X(STOP_AFTER_QUEUE, "stop-after-queue")                                                   \
    X(FOLLOW, "follow")                                                                       \
    X(HOOK_COMMAND, "hook_command")                                                           \
    X(SCOPE, "scope")                                                                         \
    X(LOOPS, "loops")                                                                         \
    X(FADE_MS, "fade_ms")                                                                     \
    X(MAX_SONG_MS, "max_song_ms")                                                             \
    X(SILENCE_MS, "silence_ms")                                                               \
    X(RECORD_TO, "record_to")

typedef enum sel_option_id {
#define SEL_OPTION_ENUM(id, name) SEL_OPT_##id,
    SEL_OPTIONS(SEL_OPTION_ENUM)
#undef SEL_OPTION_ENUM
        SEL_OPTION_COUNT
} sel_option_id;

#define SEL_OPTION_VALUE_MAX 256
#define SEL_OUTPUT_NAME_MAX 32

/* One play-queue entry. The path is `queue_text + path_offset` of the snapshot. */
typedef struct sel_queue_entry {
    uint32_t id;
    uint32_t subsong_plus1; /* 0: the replayer's default subsong */
    uint32_t path_offset;   /* into the text arena */
    uint32_t flags;         /* SEL_QUEUE_FLAG_* */
} sel_queue_entry;
/* A divider row shows its name (the path text) and is never played. A hidden
 * row plays but the regular style does not show it (Hippo stealth). */
#define SEL_QUEUE_FLAG_DIVIDER 1u
#define SEL_QUEUE_FLAG_HIDDEN 2u

/* The view model (design doc section 9b), regular style: one flat list over
 * the queue or the browse listing, a cursor and the visible window. */
typedef enum sel_view_source { SEL_VIEW_QUEUE = 0, SEL_VIEW_BROWSE = 1, SEL_VIEW_QUERY = 2, SEL_VIEW_FOLDERS = 3, SEL_VIEW_HOME = 4, SEL_VIEW_SEARCH = 5 } sel_view_source;
/* SEL_ROW_HEADING and SEL_ROW_HOME are the section headings and entries of the home source (I191).
 * SEL_ROW_FOLDER and SEL_ROW_PLAYLIST are the rows of the folders source (I179): id is the database id,
 * depth the nesting level, text the name. */
typedef enum sel_row_kind { SEL_ROW_DIR = 0, SEL_ROW_FILE, SEL_ROW_QUEUE, SEL_ROW_NODE, SEL_ROW_FOLDER, SEL_ROW_PLAYLIST,
               SEL_ROW_HEADING, SEL_ROW_HOME } sel_row_kind;
/* Styles over the queue: regular (flat), tree (group nodes that expand to
 * their songs) and drilldown (columns: pick a value per level, the next
 * column lists what matches). Grid, cover flow and compact (DUI-P4-4) list the same flat rows as regular and
 * fill sel_view_row.art_id; how they are drawn is up to the UI, and a UI offers only the styles its
 * SEL_UI_VIEW_STYLES mask names (sel_engine_set_ui_view_styles). The browse source is always regular. */
typedef enum sel_view_style {
    SEL_VIEW_STYLE_REGULAR = 0,
    SEL_VIEW_STYLE_TREE,
    SEL_VIEW_STYLE_DRILLDOWN,
    SEL_VIEW_STYLE_GRID,
    SEL_VIEW_STYLE_COVERFLOW,
    SEL_VIEW_STYLE_COMPACT,
    SEL_VIEW_STYLE_COUNT
} sel_view_style;
/* Bit of a style in a SEL_UI_VIEW_STYLES mask. The classic mask is what a UI without the tag offers. */
#define SEL_VIEW_STYLE_BIT(s) (1u << (s))
#define SEL_VIEW_STYLES_CLASSIC \
    (SEL_VIEW_STYLE_BIT(SEL_VIEW_STYLE_REGULAR) | SEL_VIEW_STYLE_BIT(SEL_VIEW_STYLE_TREE) | SEL_VIEW_STYLE_BIT(SEL_VIEW_STYLE_DRILLDOWN))
#define SEL_VIEW_STYLES_ALL ((1u << SEL_VIEW_STYLE_COUNT) - 1u)

typedef struct sel_view_row {
    uint32_t kind;  /* sel_row_kind */
    uint32_t id;    /* queue entry id (SEL_ROW_QUEUE), node id (SEL_ROW_NODE: 1.. in order of first
                       appearance, stable while the group has songs), else the index in the listing */
    uint32_t depth; /* 0 in the regular style; songs under a tree node are 1 */
    const char* text;
    uint32_t count;    /* SEL_ROW_NODE: songs the node holds */
    uint32_t expanded; /* SEL_ROW_NODE: 1 when its songs are listed below it */
    const char* key;   /* SEL_ROW_HOME: the song key to play or, when album is 1, the album name to open; NULL else (I191a) */
    uint32_t album;    /* SEL_ROW_HOME: 1 when key names an album, 0 when it names a song */
    uint32_t quick;    /* I478: SEL_ROW_HEADING: 1 for the Quick picks heading; the Home rows after it are the quick-pick grid, not a shelf */
    uint32_t composer; /* I479: SEL_ROW_HOME: 1 when key is a composer canon (the Your composers shelf); activating it opens that composer's songs */
    const char* art_ref; /* SEL_ROW_HOME: what the artwork provider resolves: key, or "<album>\x1f<artist>" for an album with a known artist (I471); NULL else */
    const char* artist; /* I476: the album artist the tile draws under its title (Home album tiles, grid and cover flow rows); NULL when unknown */
    const char* format; /* I691c: SEL_ROW_HOME album and composer tiles: the majority format of their songs (representative extension, resolve with sel_gui_platform_of_name); NULL else */
    uint32_t art_id;   /* grid, cover flow and compact queue rows: albums.id of the song's album (the artwork id), 0 none; a Home album tile: albums.id of that album (I678h2) */
} sel_view_row;

/* rows[0..row_n) are the visible window: listing indices window_start and up.
 * Row and path pointers are valid until the next pump. */
typedef struct sel_view_state {
    uint32_t source;       /* sel_view_source */
    uint32_t style;        /* sel_view_style */
    uint32_t row_count;    /* rows in the whole list */
    uint32_t cursor;       /* index into the whole list */
    uint32_t cursor_id;    /* id of the cursor row (see sel_view_row.id); tree and
                              drilldown keep the song's id while its node row shows */
    uint32_t window_start; /* index of rows[0] */
    uint32_t window_rows;  /* requested window height */
    uint32_t row_n;        /* rows[] entries filled */
    const sel_view_row* rows;
    const char* browse_path; /* directory the browse listing shows */
    const char* browse_leaf; /* last component of browse_path, split on "/" and AmigaDOS ":";
                                empty at a root ("/", "Work:") */
    const char* group;       /* grouping key: "folder" or "format" */
    uint32_t drill_n;        /* drilldown: values picked so far */
    const char* drill[2];    /* the picked values, column order */
    uint32_t version;        /* moves whenever the listing's rows change identity or order (relist, sort, filter, search, drill,
                                queue edit): the SONGS_* verbs refuse a selection made against another version (I632b) */
} sel_view_state;

typedef struct sel_song_state {
    uint32_t queue_id; /* 0 = opened outside the queue */
    uint32_t subsong, subsong_count;
    uint32_t duration_ms; /* 0 = unknown */
    uint32_t position_ms;
    sel_rp_position pattern_pos; /* zeros when not reported */
    const sel_tag* meta;         /* SEL_META_* list, valid until next pump */
    const char* path;            /* NULL when no song is open; valid until next pump */
} sel_song_state;

/* Pattern window of the playing tracker song (plan M1.3 I101). A front end asks
 * with sel_engine_pattern_request; the audio thread, which owns the replayer,
 * fills it between render calls and the next pump publishes it in
 * sel_state.pattern. */
#define SEL_PATTERN_ROWS 48u     /* rows a window holds */
#define SEL_PATTERN_CHANNELS 16u /* channels a window holds */
typedef struct sel_pattern_view {
    uint32_t seq;      /* the request this answers; 0: none answered yet */
    uint32_t valid;    /* 1: the song has pattern data and the window below is filled */
    uint32_t order, row_lo, row_n; /* the window: rows row_lo .. row_lo + row_n - 1 of the pattern at `order` */
    uint32_t channels; /* columns filled (the song's channels, at most SEL_PATTERN_CHANNELS) */
    sel_rp_tracker_info info; /* the pattern's geometry: orders, patterns, rows, channels */
    sel_rp_cell cell[SEL_PATTERN_ROWS][SEL_PATTERN_CHANNELS]; /* [row - row_lo][channel] */
} sel_pattern_view;

/* The snapshot front ends render. Fields are only appended. Everything below
 * `options` is refreshed at the end of every sel_engine_pump. */
/* sel_state.lyric_source (F19): precedence is a .lrc next to the song, then the tracker's song message, then DEViLBOX. */
enum { SEL_LYRIC_NONE = 0, SEL_LYRIC_LRC = 1, SEL_LYRIC_MESSAGE = 2, SEL_LYRIC_DEVILBOX = 3 };

/* I678f: one row of a collection shelf: an album (id, name, artist, year, songs) or a composer spelling (name, songs). */
typedef struct sel_engine_collection_row {
    uint32_t id; /* albums.id of the cover, 0 none */
    char name[96];
    char artist[96];
    uint32_t year;
    uint32_t songs;
    char format[16]; /* I691c: majority format of the album's songs (shelf album rows), "" none */
} sel_engine_collection_row;

enum { SEL_COLLECTION_SHELVES = 4, SEL_COLLECTION_SHELF_MAX = 24 };

/* I678f (design doc 2026-10-29-desktop-library-architecture.md section 7/8): the page COLLECTION_OPEN built for an album, artist,
 * composer, source, Liked Songs, Recently Played or Local Files (I678g), published in sel_state.collection. The UI only reads it. `serial` counts builds; `rows` is "key\0plays\0" per song in page
 * order (an album in scan order, an artist most played first, a composer in library order with plays 0), then "\0". Shelves, index =
 * SEL_COLL_* - 1: Albums, Appears On, More by, Spellings; shelf_total counts the whole shelf, shelf the first SEL_COLLECTION_SHELF_MAX. */
typedef struct sel_collection_page {
    uint32_t serial;
    uint32_t kind; /* SEL_LIB_ITEM_* */
    char key[128];
    char title[96];  /* album name, artist name, composer display name */
    char sub[96];    /* album: its artist */
    uint32_t year;   /* album release year, 0 none */
    uint32_t songs;  /* songs in rows */
    const char* rows;
    uint32_t shelf_n[SEL_COLLECTION_SHELVES], shelf_total[SEL_COLLECTION_SHELVES];
    sel_engine_collection_row shelf[SEL_COLLECTION_SHELVES][SEL_COLLECTION_SHELF_MAX];
} sel_collection_page;

typedef struct sel_state {
    sel_play_state play;
    int32_t volume; /* 0..100 */
    char output[SEL_OUTPUT_NAME_MAX];
    char options[SEL_OPTION_COUNT][SEL_OPTION_VALUE_MAX];
    sel_song_state song;
    const sel_queue_entry* queue;
    const char* queue_text; /* path arena the entries' path_offset points into */
    uint32_t queue_count, queue_playing, queue_version; /* queue_playing: id, 0 = none */
    const sel_view_state* view; /* the view model, valid until the next pump */
    const sel_viz_frame* viz;   /* newest visualisation frame (seq 0: none yet), valid until the next pump */
    int32_t pitch_cents;        /* PITCH: -1200..1200 */
    int32_t tempo_percent;      /* TEMPO: -50..100 */
    uint32_t pitch_q16;         /* 2^(pitch_cents / 1200), Q16 */
    uint32_t mute_mask;         /* bit k set: voice k + 1 is muted (MUTE, SOLO) */
    uint32_t mixed_path;        /* 1: the engine mixes (DSP, crossfade); 0: the output plays voices itself */
    const sel_pattern_view* pattern; /* newest answered pattern window, valid until the next pump */
    uint32_t led_on;            /* FILTER_LED: 1 when the LED filter stage is on */
    uint32_t stereo_width;      /* MIX_STEREO_WIDTH: 0 (mono) to 100 (unchanged) */
    const char* panel;          /* I185: catalogue rows of the open song, "label\0value\0" pairs (library data: STIL note,
                                 * composer, release, year, genre, rating, play count; absent data has no row), NULL: none */
    uint32_t panel_rows;        /* rows in `panel` */
    uint32_t panel_serial;      /* I288: counts panel loads; a change means `panel` was read again (a rating or sticker write) */
    uint32_t library_serial;    /* I348: counts library changes (a scan, sync or fetch finished); a change means library listings are stale */
    uint32_t library_songs;     /* DUI-P4-18: songs in the catalogue, re-counted when library_serial changes (the Clear Library dialog names it) */
    const char* stats;          /* I192: the stats screen is open: "label\0value\0" pairs read from the stats SQL views, then "\0"; NULL: closed */
    const char* composers;      /* DUI-P4-3: the Library page is open: "canon\0display\0songs\0" per composer (decimal count), then "\0"; NULL: closed */
    const char* formats;        /* I615c: the song formats of the catalogue after FORMATS_SHOW: "format\0songs\0" per format (decimal count, most songs first), then "\0"; NULL before the first FORMATS_SHOW */
    const char* composer_page;  /* the composer drilled into: "canon\0display\0songs\0", then "key\0name\0" per song, then "\0"; NULL: none */
    const sel_collection_page* collection; /* I678f: the page the last COLLECTION_OPEN of an album, artist or composer built; NULL before the first */
    uint32_t lyric_source;      /* I193: SEL_LYRIC_*: where the lyrics of the open song come from */
    const char* lyric_text;     /* plain lyrics (song message, or unsynced text), NULL: none or synced */
    uint32_t lyric_lines;       /* timed lines of synced lyrics (LRC), 0: none */
    int32_t lyric_line;         /* index of the line at the play position, -1: before the first line or no synced lyrics */
    const char* lyric_current;  /* text of that line, NULL when lyric_line is -1 */
    const char* lyric_prev;     /* DUI-P4-7: text of the line before the current one, NULL when there is none */
    const char* lyric_next;     /* DUI-P4-7: text of the line after the current one (the first line while lyric_line is -1), NULL when none */
} sel_state;

struct sel_library;
/* Counters of the last LIBRARY_SCAN (test sentinels; probed > 0 proves the scan code ran). */
typedef struct sel_scan_info {
    uint32_t scanned_files, skipped_files, scan_batches, probed, dirs;
    uint32_t archives;        /* archive files opened */
    uint32_t archive_members; /* members written as songs */
    uint32_t archive_lzx;     /* LZX archives opened */
    uint32_t depack_pp;       /* PowerPacker files depacked and scanned */
    uint32_t bytes_read;      /* bytes read from song files; a streamed MP3 or Ogg reads less than its size (I184d) */
} sel_scan_info;

typedef struct sel_engine_config {
    uint32_t cmd_queue_depth; /* 0: default 64 */
    uint32_t evt_queue_depth; /* 0: default 256 */
    /* Plugin enumeration (sel_registry_count / sel_registry_get). The engine
     * core does not link the registry; the front end hands these in. NULL:
     * OPEN answers ERROR no_replayer. */
    uint32_t (*registry_count)(void);
    const sel_tag* (*registry_get)(uint32_t index);
    /* Queue sizes, allocated once in sel_engine_create. 0: scaled from the
     * SEL_MACHINE_FAST_RAM tag of `machine` (the list sel_machine_probe
     * returns; the front end probes, the engine does not), or the smallest
     * defaults when `machine` is NULL. */
    uint32_t queue_capacity;
    uint32_t queue_text_bytes;
    /* Browse buffer (LIBRARY_BROWSE), allocated once; 0: same as the queue. */
    uint32_t browse_entries;
    uint32_t browse_text_bytes;
    const sel_tag* machine;
    /* Directory of prefs.conf (see sel_config_dir). NULL: prefs are not read
     * or written, they stay at their defaults for this run. */
    const char* config_dir;
    /* Library database (src/library/db); the front end opens and closes it. NULL:
     * LIBRARY_SEARCH, LIBRARY_SCAN, LIBRARY_FETCH, PLAY_QUERY, QUEUE_ADD_QUERY and VIEW_SAVE answer ERROR no_library. */
    struct sel_library* library;
} sel_engine_config;

typedef struct sel_engine sel_engine;

void sel_engine_scan_info(const sel_engine* e, sel_scan_info* out);

/* I315: live progress of a LIBRARY_SCAN, readable from any thread while the scan runs. `file` is the path being
 * scanned, `done` the files started so far, `total` the files the folder holds (counted first). `active` is 1 while
 * a scan runs; the last values stay after it ends. */
#define SEL_SCAN_FILE_MAX 160u

/* I488: longest composer name or canonical key a library row carries, NUL included. Public so a UI plugin sizes a
 * COMPOSERS_SHOW request from it; src/library/composer.h includes this header and adds no second value. */
#define SEL_COMPOSER_MAX 160u
typedef struct sel_scan_progress {
    uint32_t active, done, total;
    char file[SEL_SCAN_FILE_MAX];
} sel_scan_progress;
void sel_engine_scan_progress(const sel_engine* e, sel_scan_progress* out);

/* Counters of the last LIBRARY_SYNC (I161; test sentinels: listing_rows > 0 proves the sync code ran). */
typedef struct sel_sync_info {
    uint32_t listing_rows, skipped_rows, changelogs;
    uint32_t cl_added, cl_removed, cl_moved, cl_updated;
    uint32_t new_this_week;
    uint32_t songlengths, stil_entries, subtunes; /* hvsc sync (I162) */
    uint32_t hippo_lists, prg_files, prg_entries, prg_dividers, prg_stealth, prg_subsongs; /* hippo sync (I163) */
    uint32_t net_zip_bytes, net_listing_bytes, net_changelogs_fetched, net_changelogs_missing, net_requests,
        net_unpack_peak_heap; /* modland URL sync (I161a, I161c) */
    uint32_t net_songlengths_bytes, net_stil_bytes, net_listing_derived; /* hvsc URL sync (I162a); net_requests too */
    /* The source's own stats list (SEL_SYNC_* tags, END-terminated), what its `sync` reported (I517). The named
     * fields above are filled from it for the readers that predate the list. */
    sel_tag stats[SEL_SYNC_STATS_MAX];
} sel_sync_info;
void sel_engine_sync_info(const sel_engine* e, sel_sync_info* out);

/* The config directory: $XDG_CONFIG_HOME/selecta, else ~/.config/selecta;
 * ENVARC:Selecta on the Amiga. Writes the path to `out`; 0 when it cannot be
 * determined or does not fit. The directory is not created. */
int sel_config_dir(char* out, size_t cap);

/* The default library DB: PROGDIR:library.db on the Amiga, else library.db in the config directory (created when
 * missing). NULL when no location is known. The shell and selecta-cli library scan share it. */
const char* sel_default_db_path(char* out, size_t cap);

/* Creates `dir` and missing parents (src/core/<os>/config_mkdir.c). 1 when it exists afterwards. */
int sel_config_mkdir(const char* dir);

/* Called by sel_engine_pump once per delivered event. */
typedef void (*sel_event_sink)(void* ctx, const sel_message* evt);
/* I336: LIBRARY_SCAN runs on the library job thread and answers on a later pump. A caller with no loop of its own (the
 * CLI, --exec) pumps through this until the running scan has answered; 0 when no scan runs. */
int sel_engine_scan_busy(const sel_engine* e);
void sel_engine_scan_wait(sel_engine* e, sel_event_sink sink, void* ctx);

sel_engine* sel_engine_create(const sel_host* host, const sel_engine_config* cfg);
void sel_engine_destroy(sel_engine* e);

/* Enqueues a command. String and tag arguments are not copied: they must
 * stay valid until the next sel_engine_pump. Returns 0 when the queue is full. */
int sel_engine_send(sel_engine* e, const sel_message* cmd);

/* Enqueues an ERROR event on behalf of the protocol layer (for a line the
 * codec could not decode). `detail` is copied. */
void sel_engine_report_error(sel_engine* e, const char* code, const char* detail);

/* The shared protocol line parser (src/protocol/parse.c): decodes one command
 * line into `pool` (the strings stay in it, so it must live until the next
 * sel_engine_pump) and sends it, or raises the ERROR event. Returns
 * SEL_CODEC_OK or the codec error. */
int sel_engine_send_line(sel_engine* e, const char* line, char* pool, size_t poolcap);

/* Drains the command queue, dispatches, delivers events. Returns the
 * number of events delivered. */
uint32_t sel_engine_pump(sel_engine* e, sel_event_sink sink, void* ctx);

const sel_state* sel_engine_state(const sel_engine* e);
/* The host to hand to plugins outside playback (integrations, I177b): a copy of the engine's host whose
 * config_get answers a non-empty engine pref (include/selecta/prefs.def) first, then the real host. Valid until
 * sel_engine_destroy. */
const sel_host* sel_engine_plugin_host(const sel_engine* e);
const char* sel_play_state_name(sel_play_state s);
const char* sel_option_name(sel_option_id id);

/* Sample rate and resampler (a SEL_MIX_RESAMPLE_* value) used by the next
 * OPEN. Rate 0 keeps 44100. Defaults: 44100 Hz, linear. */
void sel_engine_set_mix(sel_engine* e, uint32_t rate, uint32_t resampler);

/* Tells the engine what the chosen output is (its hook list): an output with
 * SEL_OUT_VOICES plays voices itself, so no DSP chain runs; SEL_OUT_HEADPHONES
 * turns the crossfeed on unless the pref dsp.crossfeed is "off". Call after
 * sel_engine_set_mix and the registry hook-up, before the song starts. */
void sel_engine_set_output(sel_engine* e, const sel_tag* out_hooks);
/* An output that plays voices itself (SEL_OUT_VOICE_BACKEND) hands its backend here before the song starts;
 * the replayer's voice calls then reach it instead of the mixer. NULL restores the mixer. */
void sel_engine_set_voice_sink(sel_engine* e, const sel_voices* sink);

/* Routing table (I121). The sink is used only when the replayer is VOICE kind (voice_count > 0) and
 * voice_count <= the output's SEL_OUT_VOICE_MAX (0: no limit; the Paula outputs say 4); else the mixer. */
/* HW kind (I219) gets SEL_ROUTE_HANDOVER: the replayer owns the sound hardware, the output is handed over to it. */
enum { SEL_ROUTE_MIXER = 0, SEL_ROUTE_DIRECT = 1, SEL_ROUTE_HANDOVER = 2 };
uint32_t sel_engine_route(uint32_t kind, uint32_t voice_count, int has_sink, uint32_t voice_max);
/* sel_engine_route for a VOICE or RENDER replayer (kind from voice_count). */
uint32_t sel_engine_voice_route(uint32_t voice_count, int has_sink, uint32_t voice_max);
/* Output handover (I219). Before a HW-kind song starts the engine calls fn(ctx, 1): the app closes its output so
 * the replayer can take the hardware. Before the next non-HW song starts it calls fn(ctx, 0): the app reopens the
 * output and sets its source again; fn returns 1 on success. While the output is handed over, sel_engine_pump
 * runs sel_engine_render itself by the wall clock, so position, pause and end of song keep working. */
typedef int (*sel_engine_handover_fn)(void* ctx, uint32_t release);
void sel_engine_set_handover(sel_engine* e, sel_engine_handover_fn fn, void* ctx);
/* Output swap (I532, design section 5). A song routed to the mixer needs a PCM sink; when the open output only
 * plays voices (SEL_OUT_VOICES, Paula) it would discard the PCM. Before such a song starts the engine picks a PCM
 * output (ahi when the machine lists it, else paula14) and calls fn(ctx, id): the app closes its output, opens `id`
 * at that output's rate (sel_engine_set_mix), sets the source, and returns 1. A song routed to the voice sink
 * directly swaps back to the original output the same way. Never called without a callback. */
typedef int (*sel_engine_output_switch_fn)(void* ctx, const char* id);
void sel_engine_set_output_switch(sel_engine* e, sel_engine_output_switch_fn fn, void* ctx);
/* 1 while the output is handed over to a HW-kind replayer. */
uint32_t sel_engine_handed_over(const sel_engine* e);
/* Where the song started last went: 1 = the output's voice sink, 0 = the mixer. */
uint32_t sel_engine_routed_direct(const sel_engine* e);

/* LED filter on/off and stereo width percent (0 mono, 100 unchanged), applied
 * to the mixed output before the DSP chain (I127, I128). Call before the
 * audio thread renders. Both stages are bypassed while the output is real
 * Paula: the Paula output calls sel_engine_set_paula_output(e, 1). */
void sel_engine_set_stage(sel_engine* e, uint32_t led_on, uint32_t width_percent);
void sel_engine_set_paula_output(sel_engine* e, uint32_t on);

/* Per-song replayer options, "plugin.key=value,..." (D-M2-8): beats the global pref rp.config. They live in
 * the library DB (sticker rp.config of the song key, sel_library_song_config_set) and are read at OPEN (I170). */

/* Why the last PLAYLIST_IMPORT / PLAYLIST_ADD_FOLDER failed (sel_engine_stats.pl_fail_reason, I652e). */
enum { SEL_PLFAIL_NONE = 0, SEL_PLFAIL_UNREADABLE = 1, SEL_PLFAIL_BUSY = 2, SEL_PLFAIL_OTHER = 3 };

typedef struct sel_engine_stats {
    uint32_t ticks;        /* replayer ticks run */
    uint32_t voice_ops;    /* calls through the voices API */
    uint32_t mixed_frames; /* frames mixed */
    const char* replayer;            /* name of the open song's replayer, NULL: none */
    uint32_t render_kind;            /* 1: the replayer renders itself (no ticks, no voices) */
    uint32_t replayer_render_calls;  /* RP_RENDER calls (RENDER kind) */
    uint32_t rendered_frames;        /* frames RP_RENDER produced (RENDER kind) */
    uint32_t seek_path;           /* last SEEK: 0 none, 1 native (RP_SEEK), 2 restart-and-skip */
    uint32_t seek_skipped_frames; /* frames the last restart-and-skip ran through */
    uint32_t songs_opened;        /* songs started by OPEN or the queue (T19) */
    uint32_t dup_redirects;       /* OPENs that played another source of the same md5 than the one asked for (I184 sentinel) */
    const char* dup_played;       /* key of the file the last redirected OPEN read, "" when none (I184) */
    uint32_t hash_files;          /* local files the idle md5 pass finished (I184b, sel_hash_stats) */
    uint32_t hash_chunks;         /* file reads of the pass: without threads one chunk per idle tick */
    uint32_t hash_tick_max;       /* most bytes one idle tick read (without threads <= SEL_HASH_CHUNK) */
    uint32_t scrobble_due;        /* SCROBBLE_DUE events the engine emitted (T95 sentinel) */
    uint32_t sample_plays;        /* SAMPLE_PLAY commands taken (DUI-P4-12 sentinel) */
    uint32_t sample_frames;       /* frames of audition samples the audio thread mixed (DUI-P4-12) */
    uint32_t history_writes;      /* history rows the engine wrote on song end or skip (T78 sentinel) */
    uint32_t skip_excluded;       /* skip-flagged rows the random order left out (T78 flags sentinel) */
    uint32_t analyse_jobs;        /* ANALYSE jobs that stored a gain (I187 sentinel, T89) */
    uint32_t analyse_speed;       /* last ANALYSE job: audio seconds rendered per CPU second (x real time) */
    int32_t analyse_lufs_cdb;     /* last ANALYSE job: integrated loudness in centi-LUFS (the RMS fallback when ungated) */
    int32_t analyse_gain_cdb;     /* last ANALYSE job: gain stored in the library, centi-dB */
    int32_t analyse_peak_cdb;     /* last ANALYSE job: sample peak in centi-dBFS */
    uint32_t analyse_gated;       /* last ANALYSE job: 1 = R128 gated result, 0 = RMS+peak fallback */
    uint32_t cue_sets;            /* CUE_SET rows stored (I175 sentinel) */
    uint32_t cue_gotos;           /* CUE_GOTO seeks made (I175 sentinel) */
    uint32_t home_rows;           /* entries the home view source built: plays + albums to resume + new this week (I191 sentinel) */
    uint32_t folder_view_rows;    /* rows the folders view source built from the library tree (I179 sentinel) */
    uint32_t stats_queries;       /* STATS_SHOW queries run against the stats views (I192 sentinel) */
    uint32_t panel_loads;         /* module info panel lookups made in the library (I185 sentinel) */
    uint32_t lyric_fetches;       /* DEViLBOX lyrics requests queued, none for a song with a .lrc or a message (I193 sentinel) */
    uint32_t folder_ops;          /* FOLDER_ADD/MOVE/DEL and PLAYLIST_MOVE run against the library (I178 sentinel) */
    uint32_t import_runs;         /* PLAYLIST_IMPORT files parsed and appended (I652c sentinel) */
    uint32_t import_added;        /* songs the last PLAYLIST_IMPORT appended */
    uint32_t addf_runs;           /* PLAYLIST_ADD_FOLDER scans finished and appended (I652d sentinel) */
    uint32_t addf_added;          /* songs the last PLAYLIST_ADD_FOLDER appended */
    uint32_t pl_fail_runs;        /* PLAYLIST_IMPORT / PLAYLIST_ADD_FOLDER runs that failed (I652e) */
    uint32_t pl_fail_reason;      /* SEL_PLFAIL_* of the last failed run */
    uint32_t songs_runs;          /* SONGS_* bulk verbs that ran (I632b sentinel) */
    uint32_t songs_done;          /* songs the last SONGS_* verb handled (the count the toast shows) */
    uint32_t songs_skipped;       /* rows the last SONGS_* verb skipped because they name no song (group, folder, heading, divider)
                                   * (I632b2) */
    uint32_t songs_stale;         /* SONGS_* verbs refused because the list version moved */
    uint32_t import_missing;      /* entries of the last PLAYLIST_IMPORT whose file does not exist */
    uint32_t sticker_ops;         /* STICKER_* commands run against the library (I176 sentinel) */
    uint32_t links_parsed;        /* share links OPEN parsed (I180 sentinel) */
    uint32_t links_made;          /* share links SHARE wrote (I180 sentinel) */
    uint32_t sources_listed;      /* SOURCES rows the engine queued for the UI (I546 sentinel) */
    uint32_t lib_touch_calls;   /* LIBRARY_TOUCH commands applied (I678c sentinel: Recents order has a written opened time) */
    uint32_t collection_opens;  /* COLLECTION_OPEN commands applied (I678e sentinel: a collection page was opened through the engine) */
    uint32_t collection_queries; /* I678f sentinel: collection page lists (songs or shelf) that reached the library */
    uint32_t library_items_calls; /* sel_engine_library_items calls that reached the library (I678a sentinel) */
    uint32_t evt_dropped;         /* events dropped because the event queue was full (I546 sentinel) */
    uint32_t radio_appends;       /* songs RADIO appended to the queue (I186 sentinel) */
    uint32_t radio_min_ahead;     /* fewest songs queued after the playing one after a radio top-up, 0xFFFFFFFF before RADIO (I186) */
    uint32_t net_cancels;         /* URL fetches abandoned because a newer STOP or OPEN arrived (I195a sentinel) */
    uint32_t net_cancel_lost;     /* cancels lost to the send/pump race; must be 0 (I195c sentinel) */
    uint32_t rating_pushes;       /* ratings DEViLBOX accepted (PUT or DELETE) since the process started (I173 sentinel) */
    uint32_t rating_batches;      /* rating batch read requests sent (I173 sentinel) */
    uint32_t vote_posts;          /* votes the HippoPlayer service accepted since the process started (I174, T80 sentinel) */
    uint32_t rating_pending;      /* ratings in the library not yet on DEViLBOX (I173) */
    uint32_t library_searches;    /* queries sent to the library worker (T64) */
    uint32_t library_fts_queries; /* of those, queries the FTS5 index served (T64) */
    uint32_t result_count;         /* keys held by the last query result (T64, T65) */
    uint32_t rp_config_applied;   /* replayer option values answered by the rp.config pref or the song override (T61) */
    uint32_t undo_depth, redo_depth; /* queue edits that UNDO / REDO can step over (T28) */
    uint32_t dsp_blocks;             /* render blocks that ran at least one DSP plugin (T47) */
    uint32_t xfade_frames;           /* frames mixed with two songs at once (T44) */
    uint32_t slots_mixed;            /* most songs mixed at once so far */
    uint32_t mixed_path;             /* 1: the engine mixes, 0: the output owns voices */
    uint32_t mutes;                  /* mute mask changes (MUTE, SOLO) (T39) */
    uint32_t fades;                  /* end fades started (loop count, max length) (T39) */
    uint32_t crossfeed_on;           /* 1: the crossfeed DSP is in the chain (T39) */
    uint32_t sleep_armed;            /* 1: a sleep timer is counting down (T39) */
    uint32_t led_on, led_frames, led_cutoff_hz;       /* LED filter state, frames filtered, designed -3 dB point (T53) */
    uint32_t width_percent, width_frames, width_mono; /* stereo width, frames processed, 1: width 0 (T53) */
    const char* loudness_source;  /* "off", "none" (no gain known) or "stored" or "provisional" (library gain applied), "leveler" (auto mode, adaptive) (T90, I189, I190) */
    uint32_t loudness_gain_q16;   /* the 16.16 gain in force; 65536 when none */
    uint32_t limiter_hits;        /* frames the -1 dBFS limiter reduced */
    uint32_t loudness_frames;     /* frames that ran through the gain stage (sentinel) */
    uint32_t leveler_frames;      /* frames the adaptive leveler measured and scaled (sentinel of I190) */
    uint32_t leveler_writes;      /* provisional gains the leveler wrote back to the library (I190) */
    uint32_t hw_handovers;        /* times the output was handed over to a HW-kind replayer (I219) */
    uint32_t hw_starts;           /* HW-kind RP_START calls that succeeded (sentinel of I219) */
} sel_engine_stats;
void sel_engine_stats_get(const sel_engine* e, sel_engine_stats* out);
/* Key `i` of the last LIBRARY_SEARCH, PLAY_QUERY or QUEUE_ADD_QUERY result (byte order of the key), NULL past the end. */
const char* sel_engine_result_key(const sel_engine* e, uint32_t i);

/* The name of a sel_view_style value ("regular", "tree", "drilldown", "grid", "coverflow", "compact"); "unknown" otherwise. */
const char* sel_view_style_label(uint32_t style);
/* "queue", "browse" or "query" for a sel_view_source value; "unknown" otherwise. */
const char* sel_view_source_label(uint32_t source);
/* Key of the song under the cursor in the queue or query list, or the path of the file row under the cursor in
 * the browse listing (joined with the platform's path rules); NULL on a directory or node row or an empty list.
 * The browse path stays valid until the next call. */
const char* sel_engine_view_cursor_key(sel_engine* e);
/* Path of the song on visible view row `row` (index into view.rows) written to out; 0 on a non-song row or a path that does not fit. */
int sel_engine_view_row_path(sel_engine* e, uint32_t row, char* out, size_t cap);
/* DUI-P4-18: playlists in the library now (the Clear Library dialog names the count it deletes); 0 without a library. */
uint32_t sel_engine_library_playlists(sel_engine* e);
/* I608: the sidebar Your Library list. One saved playlist: its id and display name. */
typedef struct sel_engine_library_item {
    uint32_t id;
    char name[96];
    uint32_t added; /* I611: creation order (ids grow with each new playlist), larger is newer */
    char creator[96]; /* I611: who made it, per kind (I678c): playlist the owner, album its artist, artist/composer/source the name */
    /* I678a: the fields of the mixed Your Library list (sel_engine_library_items); the playlist-only call leaves them zero. */
    uint32_t kind;        /* SEL_LIB_ITEM_* (include/selecta/library_item.h) */
    char key[96];         /* stable id a click routes on: playlist id, album id, artist name, composer canon, source id, pinned key */
    char sub[96];         /* raw subtitle data (the album artist); the text is built by sel_uim_library_item_subtitle */
    uint32_t songs;       /* songs inside */
    uint32_t last_played; /* epoch seconds of the newest play of anything inside, 0 never */
    uint32_t opened;      /* I678c: epoch seconds the item page was last opened, 0 never; Recents orders by the newer of this and last_played */
    uint32_t art_id;      /* albums.id of the cover, 0 none */
    int pinned;           /* 1: Liked Songs, Recently Played, Local Files */
    char format[16];      /* I691c: majority format of the songs inside (representative extension), "" none */
} sel_engine_library_item;
/* I678a: the whole Your Library list of the kinds in `kind_mask` (SEL_LIB_ITEM_BIT): pinned rows, playlists, albums, artists,
 * composers, sources. Fills out[0..max) from row `offset`, returns the rows written (0 without a library); *total (when not NULL)
 * is the row count of the whole list for paging. Source rows carry the registry display name. */
uint32_t sel_engine_library_items(sel_engine* e, uint32_t kind_mask, uint32_t offset, sel_engine_library_item* out, uint32_t max,
                                  uint32_t* total);
/* I633: the song keys of saved playlist `id` in position order into out as "key\0key\0...\0\0"; returns how many. */
uint32_t sel_engine_library_playlist_keys(sel_engine* e, uint32_t id, char* out, uint32_t cap);
/* DUI-P4-18: the catalogue title of the song at `ref` (path or key) into out; 1 when it has one, else 0 (callers fall back to the file name). */
int sel_engine_song_title(sel_engine* e, const char* ref, char* out, size_t cap);
/* I258: the Queue page group of the song at `ref`: its album, else its source, into out; 1 when the song is catalogued, else 0 and out empty. */
int sel_engine_song_group(sel_engine* e, const char* ref, char* out, size_t cap);
/* I678h3: the artist, album, album id (decimal) and first composer canon of the song at `ref` into out as four NUL terminated strings (empty when absent); 1 when catalogued. A track row links to those pages with them. */
int sel_engine_song_meta(sel_engine* e, const char* ref, char* out, size_t cap);

/* Fills `frames` stereo frames: the open song's mix while PLAYING, else
 * silence. Counts calls. Never allocates. */
/* Marks the calling thread as the audio thread (call it from the output's
 * pull callback on its first run). Debug builds then assert that
 * sel_engine_render runs only there and send/pump/destroy never do. */
void sel_engine_mark_audio_thread(sel_engine* e);
uint32_t sel_engine_render(sel_engine* e, int16_t* stereo, uint32_t frames);
uint32_t sel_engine_render_calls(const sel_engine* e);
/* The running end fade of the playing song: 1 and its position and length in frames, else 0. */
int sel_engine_fade(const sel_engine* e, uint32_t* pos, uint32_t* total);

/* Tracker pattern data of the open song (RP_TRACKER_INFO / RP_TRACKER_CELLS).
 * Return 0 when no song is open or the replayer has no pattern data. They
 * call the replayer directly: use them only from the thread that owns it
 * (the script driver); there is no audio-thread request path yet. */
int sel_engine_tracker_info(const sel_engine* e, uint32_t order, sel_rp_tracker_info* out);
uint32_t sel_engine_tracker_cells(const sel_engine* e, uint32_t order, uint32_t channel, uint32_t row_lo,
                                  uint32_t row_hi, sel_rp_cell* out, uint32_t cap);

/* Asks for a pattern window: rows row_lo .. row_lo + rows - 1 (at most
 * SEL_PATTERN_ROWS) of the pattern at `order`. Pump thread only. The audio
 * thread answers after its next render call and the following pump publishes
 * the answer in sel_state.pattern (same `seq` as the return value). Returns the
 * request number, or 0 when the previous request is still unanswered (ask
 * again after the next pump). */
/* Sample slot n (1-based) of the open song, for a front end that lists samples (DUI-P4-12). Pump thread only, like
 * the SAMPLE_LIST command. Returns 1 and fills `out`, or 0 when the song has no sample table or the slot is empty;
 * out->count is the number of slots when the song has a table. */
int sel_engine_sample_get(const sel_engine* e, uint32_t n, sel_rp_sample* out);

uint32_t sel_engine_pattern_request(sel_engine* e, uint32_t order, uint32_t row_lo, uint32_t rows);

/* 1 when the engine answers this verb with real behaviour, 0 when it
 * replies ERROR not_implemented. The ratchet (T8) counts the zeros. */
int sel_engine_cmd_implemented(uint32_t cmd_id);

/* The id a UI_SWITCH command asked for (UI or socket line): copied to id and cleared; 0 when none is pending. The
 * engine owns no UI, so the shell takes the request after a pump and switches (apps/selecta ui_switch). */
/* The view styles the active UI offers (its SEL_UI_VIEW_STYLES mask); VIEW_STYLE names outside it are refused with
 * ERROR unsupported_style. Default SEL_VIEW_STYLES_CLASSIC; the shell sets it when a UI opens or replaces the primary. */
void sel_engine_set_ui_view_styles(sel_engine* e, uint32_t mask);
int sel_engine_take_ui_switch(sel_engine* e, char* id, size_t cap);
/* 1 once after a QUIT command (inside BEGIN: after END); the shell then ends its run as if the primary UI returned -1. */
int sel_engine_take_quit(sel_engine* e);
/* 1 while a BEGIN is open (the ARexx port ends it at close, plan M4-AX section 5). */
int sel_engine_in_batch(const sel_engine* e);

SEL_EXTERN_C_END

#endif /* SELECTA_ENGINE_H */
