/* Desktop UI plugin (M3d, DUI-P1-4). It renders the engine snapshot through the shared UI model
 * (include/selecta/uimodel.h): one widget per region, redrawn only when the region's dirty bit is set. Drawing goes
 * to a backend: `mem` (headless, no SDL, DUI-P2-6) or `sdl` (a window, DUI-P2-7), selected with `--backend`. */
#include <ctype.h>
#include <selecta/plugin.h>
#include <selecta/engine.h>
#include <selecta/library_item.h>
#include <selecta/ui.h>
#include <selecta/uimodel.h>

#include "art.h"
#include <selecta/gui/gui_genart.h>
#include "atlas_build.h"
#include "font_roles.h"
#include <selecta/gui/gui_anim.h>
#include <selecta/gui/gui_draw.h>
#include <selecta/gui/gui_text.h>
#include <selecta/gui/gui_widget.h>
#include <selecta/gui/gui_input.h>
#include <selecta/uimodel/browse.h>
#include <selecta/uimodel/select.h>
#include <selecta/uimodel/viewcmds.h>
#include <selecta/uimodel/sources.h>
#include <selecta/uimodel/cmdline.h>
#include <selecta/uimodel/doors.h>
#include <selecta/uimodel/route.h>
#include <selecta/uimodel/keybind.h>
#include <selecta/uimodel/keys.h>
#include <selecta/uimodel/palette.h>
#include <selecta/uimodel/theme.h>

#if defined(SELECTA_DESKTOP_AGA) && !defined(SELECTA_DESKTOP_SDL)
#define SELECTA_DESKTOP_SDL 1
#include "amiga/backend_aga.h"
#include "amiga/input.h"
#define DESK_BACKEND_NAME "aga"
#define desk_backend_depth(b) desk_aga_depth(b)
#define DESK_BACKEND_INDEX8 1 /* the aga screen is always 8 bitplanes, 256 colours (design doc 2.1) */
#define desk_backend_open(w, h, i8) desk_aga_open((w), (h), (i8))
#elif defined(SELECTA_DESKTOP_RTG) && !defined(SELECTA_DESKTOP_SDL)
#define SELECTA_DESKTOP_SDL 1
#include "amiga/backend_rtg.h"
#define DESK_BACKEND_NAME "rtg"
#define desk_backend_depth(b) desk_rtg_depth(b)
#define DESK_BACKEND_INDEX8 1 /* --format index8 opens an 8-bit CLUT screen (I369) */
#define desk_backend_open(w, h, idx) desk_rtg_open((w), (h), (idx))
#elif defined(SELECTA_DESKTOP_SDL)
#include "backend_sdl.h"
#define DESK_BACKEND_NAME "sdl"
#define desk_backend_depth(b) ((void)(b), 32)
#define DESK_BACKEND_INDEX8 0
#define desk_backend_open(w, h, idx) ((void)(idx), desk_sdl_open((w), (h)))
#endif

#include <stdarg.h>
#include <time.h>
#include <stdio.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>

/* I524b: --stats launch trace. One stderr line per launch phase with the host clock, so the log (--log) shows where the
 * minutes before the first present go. Absolute host clock ms: subtract the first line (selecta: trace start). */
static int g_desk_trace;
static void desk_trace(const sel_host* h, const char* phase) {
    if (g_desk_trace) {
        fprintf(stderr, "trace t_ms=%u %s\n", (unsigned)h->clock_ms(h), phase);
    }
}

/* The text size tokens (plan 2.6) as pixel sizes; each gets one atlas per weight. */
#define DESK_SIZES DESK_FONT_SIZE_N
/* The type scale tokens (Main.dc.html): caption 11/12, body 13/14, nav 15, subtitle 16, brand 18, heading 22, 24, 32. */
static const int g_sizes[DESK_SIZES] = {DESK_FONT_SIZES};
/* The logical icon sizes (DUI-P3-1); at scale k the icon atlas holds each at k times the size (DUI-P2-8). */
#define DESK_ICON_SIZES 4
static const int g_icon_sizes[DESK_ICON_SIZES] = {16, 20, 24, 32};

/* The pages the sidebar doors and --view reach (I262). One list: --view names, door clicks, keys and the stats sentinel
 * all use it. */
typedef enum { PG_HOME, PG_SEARCH, PG_FOLDERS, PG_PLAYLISTS, PG_QUEUE, PG_NOWPLAYING, PG_SONGS, PG_SONGINFO, PG_VIZ, PG_TRACKER, PG_SHELF, PG_BROWSE, PG_COLLECTION, PG_COUNT } desk_page;
static const char* const g_page_name[PG_COUNT] = {"home", "search", "folders", "playlists", "queue", "nowplaying", "songs", "songinfo", "viz", "tracker", "shelf", "browse", "collection"};
#define SHOW_ALL_LABEL "Show all" /* I617: the link at the right end of every Home shelf header */
#define SHOW_ALL_PX 14
/* The sidebar doors in order: Spotify's Home, Search and Your Library, then Playlists, Queue and Now Playing. */
#define DOOR_COUNT SEL_UIM_DOOR_COUNT
/* Capacity of the hover hit-test list (I345). Worst case on one page: the Songs rows at SONG_ROW_H 32 px in a 4K tall main
 * region (2160 / 32 = 67.5 -> 68 rows, plus one row half scrolled out = 69), 7 doors, 8 transport buttons and 16 chips,
 * tabs and page buttons: 69 + 7 + 8 + 16 = 100, rounded up to 128. Library rows at 48 px need 46, queue and playlist rows
 * at 40 px need 54, so Songs is the maximum. */
#define HBOX_MAX 128

/* One song the shared song menu (I305, DUI-P3-16) acts on: its path, the queue entry id when it is a queue row, and
 * whether the menu offers Remove. */
typedef struct song_ref {
    char path[512];
    uint32_t qid;
    int removable;
    int home;   /* 1 = a Home shelf tile: Play Next also counts as a tile_queued */
    uint32_t pl_id;    /* I633: the saved playlist whose page lists this row (0 = not a playlist row): the menu offers Remove from Playlist */
    uint32_t pl_index; /* I633: the row's position in that playlist */
    uint32_t list_kind;    /* I632d: the list this row belongs to (sel_uim_list_id.kind; 0 = not a selectable list row) */
    uint32_t list_id;      /* I632d: which list of that kind (page, playlist id, drilldown) */
    uint32_t list_version; /* I632d: the list's version: a sort, filter, search or page change makes a new one */
    uint32_t list_index;   /* I632d: the row's position in the whole list (not in the window), the selection model's row number */
} song_ref;

/* Rows a frame registers (song_row_reg): the Queue page lists the rows that fit (at most ~30 at 1080p), the Library lists hold
 * the view window (12 rows, LIB_WINDOW_ROWS; 64 on Search), the Library grid at most that window of tiles, the Home tile grid
 * the tiles that fit under the header before the registry is full, and the Now Playing title and the Song info header one each. */
enum { SM_ROWS = 96 };

typedef struct song_row {
    sel_gui_rect r, more;
    song_ref ref;
} song_row;

/* I389: the painters timed in desk_present; the slot names are the --stats suffixes. */
enum { PT_CANVAS = 0, PT_SONG, PT_SIDEBAR, PT_MAIN, PT_TRANSPORT, PT_PROGRESS, PT_VOLUME, PT_STATUS, PT_FOCUS, PT_TIP,
       PT_OVERLAY, PT_VISUAL, PT_COUNT };

enum { SIDE_CHIPS = 6 }; /* I610, I678b: the kind chips of the Your Library sidebar */
#define SIDE_LIB_MAX 64u /* I608: playlists the sidebar list holds */

/* I678h3: the artist, album and album id of one collection page song, read from the db row once per reload (sel_engine_song_meta). */
typedef struct pls_meta_row {
    char artist[96], album[96];
    uint32_t album_id; /* the albums row of the song's album; 0 when the album is not in the table */
} pls_meta_row;

typedef struct desk {
    sel_engine* engine;
    const sel_host* host;
    sel_uim uim;
    uint32_t frames;
    uint32_t widget_draws;
    uint32_t hash;
    const char* resize; /* --resize WxH[,WxH...]: resize events the mem backend replays at open */
    sel_uim_layout layout;
    int actions; /* --actions: the close hook prints the action table and the help rows */
    int report; /* --stats: the close hook prints its sentinel line */
    txt_theme theme;       /* design tokens (DUI-P1-10): defaults, then --theme <file> over them */
    int tokens;            /* --tokens: the close hook prints what the mem backend painted with the tokens */
    sel_uim_budget budget; /* --format: the effects the canvas format affords */
    uint32_t tweens;       /* tweens started (title cross-fade on a song change) */
    uint32_t effect_prims; /* blur and shadow primitives run */
    sel_gui_canvas* canvas; /* the drawing surface (DUI-P2-1), --canvas WxH, else the backend's size (1280x800) */
    sel_gui_shadow shadow;  /* baked once when the governor allows shadows */
    int shadow_ready;
    int testcard;           /* --view testcard: draw the primitive test card once, no widgets */
    uint32_t dirty_pixels;  /* pixels the latest present repainted (DUI-P2-2) */
    uint32_t dirty_total;   /* pixels repainted over all presents */
    uint32_t hover_dirty_max; /* most pixels one hover-move present repainted (DUI-P3-8, stats line hover_dirty) */
    int hover_frame;          /* a hover move painted since the last present: the next present counts toward hover_dirty_max */
    int tween_ended;        /* a tween ended between live turns: the next live turn draws its end state once (I341) */
    int ticks_given;        /* --ticks was passed: only such mem runs drive visualizer frames (DUI-P4-11) */
    uint32_t ticks;         /* --ticks N: clock seconds the mem backend replays after the quiet turn */
    int textsheet;          /* --view text: specimen lines and the queue names, once (DUI-P2-3) */
    int widgets;            /* --view widgets: the widget gallery, once (DUI-P3-1) */
    sel_gui_widget_stats wstats; /* per-kind draw counts of the gallery */
    int home;               /* --view home: the tile grid of the queue entries in the main region (DUI-P2-6) */
    sel_gui_text_stats text; /* glyphs blitted and code points the atlas lacked (DUI-P2-3) */
    const sel_gui_atlas* atlas[2][DESK_SIZES]; /* [semibold][size]: baked (I463) or built on first use */
    uint32_t view_opens;    /* tiles opened by input (DUI-P2-6) */
    int32_t open_tile;      /* 1-based queue index of the opened tile, 0 = home */
    const char* frame_dump; /* --frame-dump <file>: the canvas as PPM at close */
    char* input;            /* --input <file>: scripted clicks, replayed once on the first quiet turn */
    int input_done;
    int* input_consumed;    /* the shell's once-per-process flag for the --input script */
    desk_art* art;          /* tile artwork from the library, NULL without one (DUI-P2-4) */
    uint64_t art_tile_hash; /* pixels of the first home tile, STAT_ART_TILE_HASH (DUI-P2-5) */
    char canvas_hash_hex[17]; /* the 64-bit hashes as 16 hex digits: the stats carry strings, a tag value may be 32 bits */
    char art_tile_hash_hex[17];
    char present_hash_hex[17];
    char zoom_str[8];       /* the zoom setting for the stats line: fit, 75, 100, 125 or 150 */
#ifdef SELECTA_DESKTOP_SDL
    desk_sdl* sdl;          /* --backend sdl: the window the canvas is presented in (DUI-P2-7) */
#endif
    sel_gui_timeline tl;    /* the animation timeline (DUI-P3-5): at most 16 tweens, Q16 values, ms */
    uint32_t now_ms;        /* the UI clock: virtual in the mem backend (--input `wait N`), SDL ticks when live */
    int32_t hover_tile;     /* 1-based tile under the pointer, 0 = none */
    int live;               /* sdl on a real display without --input: the window stays until it is closed */
    int repaint;            /* the next present paints the whole canvas (first frame, resize) */
    int32_t seek_fill;      /* pixel width of the seek bar fill last drawn; -1 = not drawn since the last full repaint */
    int32_t vol_fill;       /* the same for the volume bar */
    char seek_length[16];   /* the length text last drawn, so a clock tick marks it dirty only when it changed */
    int32_t scale;          /* the backend pixel density 1, 2 or 3 (DUI-P2-8) */
    int32_t scale_q16;      /* density times zoom, Q16.16 (I458): layout is in logical units, drawn times this */
    int32_t zoom_pct;       /* UI Zoom percent 75, 100, 125, 150; 0 = fit */
    int32_t zoom_default;   /* the zoom the backend starts at: 0 (fit) on rtg and aga, 100 otherwise */
    uint32_t atlas_builds;   /* text atlases built at run time */
    uint32_t atlas_baked_hits; /* text atlases taken from the baked tables (I463) */
    uint32_t bitmap_hits[3];   /* I526: text lookups served by mini4, mini6, mini8 */
    uint32_t headline_hits[2]; /* I526: text lookups served by the Inter headline sizes 18 and 24 */
    uint32_t atlas_ms;       /* host clock ms spent in desk_atlas_build (I463): the soft-float cost on the 68020 tier */
    uint32_t atlas_rebuilds; /* scale changes that dropped the built text atlases */
    uint32_t icon_rebuilds;  /* scale changes that rebuilt the icon atlas view */
    sel_gui_icons icons;     /* the icon atlas at the current scale: the baked masks of size k * logical */
    int32_t icon_px[DESK_ICON_SIZES];
    const uint8_t* icon_alpha[DESK_ICON_SIZES];
    uint8_t* icon_buf[DESK_ICON_SIZES]; /* resampled masks (I460) for sizes that are not baked; NULL when baked */
    uint32_t icon_resampled; /* sizes of the current icon atlas that were resampled */
    txt_bindings binds;      /* the keys of the action table (DUI-P3-3) */
    uint32_t keys_in, actions_fired, actions_distinct, focus_moves, searches, arrows;
    uint32_t fired_seen[4];  /* one bit per action row that fired */
    int32_t focus;           /* focus stop: layout regions in order, then the search field; -1 = none */
    char query[64];          /* the type-to-search text */
    uint32_t search_since;   /* UI clock of the last typed character (DUI-P4-6) */
    int search_pending;      /* 1 when the query changed since the last search sent: the debounce is running */
    int search_on;           /* 1 when the main region shows the search results */
    int queue_page;          /* --view queue: the main region shows the Queue page (DUI-P4-8) */
    int pl_page;             /* --view playlists: the main region shows the Playlists page (DUI-P4-9) */
    char empty_text[64];     /* the one-line empty state the last page frame drew; empty when it drew none */
    uint32_t pl_draws, pl_rows, pl_cmds, folder_opens; /* frames drawn (sentinel), rows in the last one, verbs sent, folders source opens */
    uint32_t pl_sel_kind, pl_sel_id; /* the selected row: SEL_ROW_FOLDER or SEL_ROW_PLAYLIST and its database id (0 = none) */
    uint32_t col_kind;       /* I678f: 0 = a playlist, else SEL_LIB_ITEM_ALBUM / ARTIST / COMPOSER / RECENT / LOCAL / SOURCE, or COL_LIKED for Liked Songs (whose engine kind is 0): the kind the collection page shows (pls_id is COL_KIND_ID then) */
    char col_sub[96];        /* the album's artist (the side item's sub) */
    uint32_t col_year;       /* the album's year (0 unknown) */
    uint32_t* col_av;        /* the artist / composer avatar: generated art of the name, rendered once and drawn through a circle mask (D6) */
    int32_t col_av_w;
    uint32_t col_av_hash;
    uint32_t col_open_serial; /* serial of the published collection page when this page was opened: only a newer page is this page's */
    char col_key[256];       /* the item key of a kind page (album id, artist name, composer canon) */
    uint32_t* col_plays;     /* plays of each loaded song of a kind page */
    uint32_t col_popular;    /* artist page: Popular rows shown (5, or 10 after Show more) */
    sel_engine_collection_row col_shelf[4][24]; /* the shelves of a kind page, index = shelf - 1: Albums, Appears On, More by, Spellings */
    uint32_t col_shelf_n[4], col_shelf_total[4];
    uint32_t pls_id;         /* I633: the saved playlist the Playlists page shows as a page of songs (0 = the tree) */
    char pls_name[96];
    char* pls_buf;           /* its song keys, NUL separated, in playlist order */
    uint32_t* pls_off;       /* offset of each key in pls_buf */
    char (*pls_title)[96];   /* the catalogue title of each song, read once per reload */
    pls_meta_row* pls_meta;  /* I678h3: the artist and album of each song, read once per reload */
    sel_gui_rect si_composer; /* I678h3: the Song Information composer link, zero width when the page has no composer row */
    char si_composer_key[SEL_COMPOSER_MAX]; /* its composer canon, from the song's db row */
    char si_composer_ref[512]; /* the song path si_composer_key was read for */
    int32_t si_composer_row; /* the info row index of the Author tag, -1 when the song has none */
    int32_t pls_playing; /* I690: the position of the playing song in the drawn collection page, -1 when none */
    uint32_t pls_want, pls_n, pls_stale, pls_scroll, pls_draws, pls_rows; /* songs, reload wanted, first row shown, frames drawn, rows in the last one */
    uint32_t pls_family;     /* I691d: the majority family of the songs on the open page (an album, a person, a playlist), the header cover and the avatar */
    int pl_edit;             /* 1 while the name field has the keyboard */
    char pl_name[64];
    uint32_t* pls_ord;       /* I678e: the shown songs as indices into pls_off: the search filter, then the sort */
    uint32_t pls_vn, col_sort, col_color, col_top; /* shown songs, sort (0 custom order, 1 title), header cover colour and the band's top colour */
    int col_cover;           /* 1 when col_color came from the first song's cover */
    int col_search;          /* 1 while the collection's search field has the keyboard */
    char col_q[64];
    size_t col_qlen;
    uint32_t pls_min;        /* I652a: the page must list at least this many songs (an Add is still on its way to the engine) */
    uint32_t add_id;         /* I652b: the playlist the Library page adds to (0 = the Library page is not in add mode) */
    char add_name[96];
    uint32_t add_sent;       /* I652b: PLAYLIST_ADD_SONG lines sent from the Library page in add mode */
    uint32_t add_drawn;      /* I652b: Add controls drawn on the Library rows this frame */
    int imp_wait;            /* I652c: a playlist dialog is open, its answer arrives as a drop: 1 playlist file, 2 folder (I652d) */
    uint32_t addf_sent, addf_seen; /* I652d sentinels: PLAYLIST_ADD_FOLDER lines sent / engine folder adds the UI has shown */
    uint32_t pl_fail_seen; /* I652e: engine import / add-folder failures the UI has shown */
    uint32_t imp_sent, imp_seen; /* I652c sentinels: PLAYLIST_IMPORT lines sent / engine import runs the UI has shown */
    uint32_t pls_adds;       /* I652a: PLAYLIST_ADD_SONG lines sent from the empty-playlist search */
    uint32_t pls_find_sig;   /* I652a: signature of the search result last drawn under the empty playlist's field */
    int pls_find_shown;      /* I652a: result rows drawn under the empty playlist's search field */
    size_t pl_nlen;
    int tr_page;             /* --view tracker: the main region shows the Tracker page (DUI-P4-12) */
    uint32_t tr_tab, tr_draws, tr_rows, tr_cmds, tr_sig; /* tab, frames drawn (sentinel), rows in the last one, verbs sent, redraw hash */
    uint32_t tr_bar;                                     /* the slot the playing-row bar drew in, TR_NO_BAR when none (I283) */
    uint32_t tr_pos, tr_moves; /* order << 16 | row of the last pattern frame, and how many frames moved it: the roll sentinel (I282) */
    uint32_t tr_asked, tr_order, tr_row, tr_qid;         /* the last pattern window asked for */
    int si_page;             /* --view songinfo: the main region shows the Song Information page (DUI-P4-10) */
    int si_edit;             /* 1 while the note field has the keyboard */
    uint32_t si_sig, si_acc; /* hash of the info rows: a change redraws the page */
    int32_t si_y_q16, si_cy; /* I298: the info list scroll offset (Q16 logical pixels) and the walk's running content y (device px) */
    uint32_t si_draws, si_rows, si_cmds; /* frames drawn (sentinel), info rows in the last one, verbs sent */
    char si_note[96];
    size_t si_nlen;
    int32_t songs_drag;      /* I448: the songs row held by the primary button, -1 when none; the drop targets show only then */
    int32_t qdrag_from, qdrag_gap, qdrag_x, qdrag_y; /* I258e: a held queue row drag; from -1 when none, gap = the index it would land at */
    uint32_t qdrag_paints;                           /* frames that drew the lifted row and the insertion line */
    uint32_t queue_draws, queue_rows, queue_cmds; /* Queue page frames drawn (sentinel), rows in the last one, verbs it sent */
    struct {                 /* I258: the Queue page album groups, rebuilt when the queue changes */
        uint32_t hash, count, headers, valid;
        uint32_t* before;    /* count + 1 slots: group headers above entry i, so entry i sits on row i + before[i] */
        uint32_t* name;      /* count slots: 1 + pool offset of the header text drawn above entry i, 0 for none */
        char* pool;
        size_t pool_n, pool_cap;
    } qg;
    uint32_t drops;          /* files and folders dropped on the window or chosen in Add Folder: STAT_DROPS (DUI-P5-11) */
    uint32_t lib_serial_seen; /* I348: sel_state.library_serial the page was last loaded for (UINT32_MAX: not seen yet) */
    uint32_t lib_reloads;    /* I348: page reloads the library serial caused (stats line library_reloads) */
    uint32_t scans;          /* LIBRARY_SCAN answers seen: STAT_SCANS */
    const char* dialog_answer; /* --dialog-answer <path>: what the folder dialog answers in a test (DUI-P5-11) */
    uint32_t scan_done, scan_total; /* I315: scan progress last drawn on the Library page (files started, files in the folder) */
    char scan_file[SEL_SCAN_FILE_MAX]; /* I315: path of the file the progress shows */
    uint32_t scan_mid;       /* I336: progress snapshots seen while the scan was still running (done < total), proof the shell kept turning */
    uint32_t scan_draws;     /* I315: partial redraws of the Add Folder area while a scan ran (sentinel) */
    int scan_pending;        /* LIBRARY_SCANs sent and not yet answered (the engine queues them, I506): searches answer with the same event */
    int drop_playing;        /* 1 after a dropped file was sent as OPEN: an open_failed error then toasts */
    int32_t np_key_end;      /* right edge of the Now Playing label column; the values start np_val_x (I447) */
    int32_t np_val_x;
    uint32_t np_rows;        /* metadata pairs the Now Playing panel drew: STAT_META_PANEL_ROWS (DUI-P4-7) */
    uint32_t np_page_draws;  /* Now Playing page draws (sentinel) */
    uint32_t font_misses;    /* text draws whose size is not in g_sizes, so nothing was drawn (I361) */
    char np_text_title[96]; /* the title line the Now Playing page last drew (I361) */
    char np_text_artist[96];
    uint32_t lib_bar_draws;  /* the Library scrollbar painter ran (I541 sentinel) */
    int lib_held_frame;      /* I549: a held-button scrollbar move painted since the last present */
    uint32_t lib_held_n, lib_held_px_max; /* I549 sentinels: presents of held-button scrollbar moves, and the most pixels one repainted */
    uint32_t lib_off_page;   /* Library page draws while another page is current (sentinel, must stay 0: I274) */
    uint32_t np_draws;       /* Now Playing panel draws (sentinel) */
    uint32_t np_genart;      /* platform cover draws by the Now Playing panel: one per draw (DUI-P4-7, I691b) */
    uint32_t np_fade;        /* fade strips over the Now Playing artwork last draw: 0 on a platform cover, whose word sits in the faded third (I693b) */
    uint32_t pc_np, pc_bar, pc_page, pc_tile0, pc_draws, pc_shelf, pc_grid, pc_flow, pc_side, pc_header; /* I691d: shelf, grid, flow, sidebar and page header */ /* I691b: family + 1 of the last platform cover per song site, 0 = real art won; draws = all fallback draws */
    uint32_t cover_np, cover_bar, cover_side; /* I692: real covers (art_tile) drawn by the Now Playing panel and page, the player bar, the sidebar rows */
    sel_gui_rect np_title;      /* the Now Playing title and artist lines, w 0 = not drawn: a song row of the song menu (I372) */
    sel_gui_rect np_like, np_rate[5]; /* click targets of the like button and the five rating marks */
    int32_t np_inset_l, np_inset_r; /* gap between the Now Playing panel edge and its content, left and right: one spacing token (I404) */
    sel_gui_rect np_art, np_panel; /* the Now Playing cover and the panel it spans, for the dump (I443) */
    uint32_t np_msg_lines;            /* lines of the song message the last Now Playing draw laid out (I290) */
    int32_t np_liked;                 /* 1 when the last Now Playing draw showed the heart as liked (I306) */
    int32_t np_rating;                /* filled rating marks of the last Now Playing draw (I288) */
    uint32_t lib_searches;   /* LIBRARY_SEARCH lines sent for typed queries: STAT_LIBRARY_SEARCHES */
    uint32_t search_draws, search_rows;  /* search view frames drawn, result rows in the last one */
    int browse_filter;       /* I615c: the Search page shows a Browse tile's filter: its results draw as the tile grid whatever the view style */
    uint32_t lib_tabs_drawn, library_remaps, browse_folders_opens; /* I678i: tab-row tabs drawn, --view library remaps, Browse Folders opened from its tile */
    uint32_t browse_draws, browse_opens, browse_n; /* I615c: Browse page frames drawn, tiles opened, tiles in the last frame */
    sel_gui_rect browse_rect[48];                  /* I615c: the tile rects of the last frame (hit test and the "browse" input verb) */
    uint32_t search_grid_draws, search_flow_draws; /* I615b: search frames drawn as the tile grid / the cover flow */
    uint32_t search_facet;   /* the facet chip in force: 0 All, 1 Songs, 2 Albums, 3 Artists */
    uint32_t search_groups[3], search_drawn;  /* group counts read from the drawn headings, rows drawn in the last search frame */
    size_t qlen;
    desk_page page;          /* the page in the main region: the sidebar highlight and the nav sentinel (I262) */
    uint32_t nav_clicks, nav_keys; /* sidebar door clicks and page keys handled (I262) */
    int songs;               /* --view songs: the queue as a sortable list with a header row (DUI-P3-2) */
    uint16_t order[512];     /* display order of the list: queue indices */
    uint32_t order_n;
    int32_t col_focus;       /* I678f2: keyboard focus slot (index into pls_ord) of a collection page's song rows, -1 until a list key is pressed */
    int32_t row_focus;       /* keyboard focus row of the songs list (display index), -1 until a list key is pressed (DUI-P3-3, I423) */
    int kb_list;             /* 1 once a list key moved the Library view cursor: its row then draws as the focus row */
    uint32_t kb_activates;   /* Enter presses that activated a list row */
    int32_t q_focus;         /* keyboard focus row of the Queue page (queue index), -1 until a list key is pressed (I498) */
    int32_t lib_focus;       /* keyboard focus row of the Library composer list or drilldown, -1 until a list key is pressed (I498) */
    uint32_t sort_col;       /* 0 title, 1 format */
    uint32_t sort_dir;       /* 0 queue order, 1 ascending, 2 descending */
    int sm_open;             /* the shared song menu is open (I305) */
    int stats_open;          /* the Library Statistics panel (More) is expanded over the grid (I455) */
    uint32_t stats_panel_draws, stats_panel_rows; /* panels painted, and the statistics rows in the last one */
    int dlg_open;            /* DUI-P4-18: the confirm dialog is up: 1 = Remove from Library, 2 = Clear Library, 3 = Quit Selecta */
    char dlg_path[512];      /* the song a Remove from Library dialog names */
    char dlg_text[640];      /* the question the dialog shows */
    uint32_t dlg_playlists;  /* playlists the Clear Library dialog names */
    int quit_req;            /* I623: the Quit Selecta dialog was confirmed; the turn ends the UI loop like a closed window */
    uint32_t dlg_opens, dlg_confirms, dlg_cancels; /* sentinels: dialogs opened / confirmed / cancelled */
    int sm_step;             /* 0 the items, 1 the playlist chooser, 2 the Queue scope choices (I258), 3 the playlist or folder menu (I647) */
    uint32_t sm_pl_id;       /* I647: the playlist or folder the step 3 menu acts on */
    uint32_t sm_pl_folder;   /* 1 = that row is a folder (the menu offers Delete only) */
    char sm_pl_name[64];
    uint32_t dlg_pl_id, dlg_pl_folder; /* the playlist or folder a Delete dialog names (I647) */
    song_ref sm_ref;         /* the song it was opened on */
    song_row sm_rows[SM_ROWS]; /* the song rows drawn in the main region this frame: the one registry every page feeds */
    sel_engine_library_item sm_pls[8]; /* the song menu's Add to Playlist step: all playlists, not the sidebar's filtered list */
    uint32_t sm_pl_n;
    uint32_t sm_n, sm_opens, sm_picks, sm_pl_adds;
    sel_uim_selset sel; /* I632d: the one selection of the list on screen (src/uimodel/select.c) */
    uint32_t sel_clicks, sel_keys, pls_ver, sel_cur;
    int32_t menu_x, menu_y;  /* canvas pixels, top left of the menu */
    uint32_t hovers, clicks, double_clicks, drags, sorts, menus, row_plays;
    int32_t tip_btn;         /* 1-based transport button under the pointer, 0 = none (DUI-P3-6) */
    int32_t tip_shown;       /* 1-based button whose tooltip is visible, 0 = none */
    uint32_t tip_since;      /* UI clock when the pointer came to tip_btn */
    uint32_t tooltips;       /* tooltips shown over the run */
    char tip_text[SEL_UIM_TIP_MAX];
    uint32_t gestures;       /* pinch and swipe events handled (DUI-P3-4) */
    uint32_t tile_step;      /* index into g_tile_steps: the tile size token step, pinch moves it */
    int32_t lib_q16;         /* Library page scroll offset in logical pixels, Q16: the composer list, a drilldown, or the engine view's top row (I303) */
    int32_t queue_q16;       /* Queue page scroll offset in logical pixels, Q16 (I258), as the songs list's scroll_q16 */
    int32_t scroll_q16;      /* songs list scroll offset in logical pixels, Q16: precise wheel deltas are fractions of a pixel */
    int32_t hist[16];        /* view history (DUI-P3-4): the open_tile of each visited view, 0 = home */
    uint32_t hist_n, hist_pos;
    sel_route_hist rh;       /* app-wide history (I266, I678h): the pages and collection pages visited, with their scroll; Back and Forward move */
    sel_gui_rect col_artist; /* I678h: the artist name on an album page's meta line (a click opens the artist page); empty elsewhere */
    int baked_zoom;          /* I587: the Amiga tier (aga, rtg; --baked-zoom on the host): zoom and fit take only the baked scales */
    int copper_opt;          /* --copper N: copper pens on an INDEX8 canvas of any backend (the mem backend tests the AGA copper, I628); 0 = by backend */
    int ydiv_opt;            /* --ydiv N: forces the pixel aspect (the mem backend tests the AGA 640x256 layout); 0 = by backend */
    int is_aga;              /* the aga backend: canvases up to 300 rows get ydiv 2 (I468) */
    int pg_walk;             /* 1 while Back or Forward re-enters a page: desk_page_goto must not append it again */
    uint32_t nav_arrow_clicks; /* history arrow clicks */
    uint32_t vol_clicks, seek_clicks; /* footer bar clicks and drags sent as VOLUME and SEEK (I312) */
    int search_keep;         /* 1 while the Home field click enters Search: the typed query stays */
    sel_gui_rect ring, field, caret; /* the last focus ring, the search field it hugs and the caret bar (stats line) */
    uint32_t caret_draws;
    struct { sel_gui_rect box; int32_t ink_x, ink_w; char text[24]; } labels[48]; /* the centred labels as last drawn, by box and text (stats line) */
    uint32_t label_n;
    sel_gui_rect hbox[HBOX_MAX]; /* the hoverable controls and list rows as last drawn (buttons, chips, tabs, doors, rows): the pointer hit-test list (I272, I345) */
    uint32_t hbox_n;
    sel_gui_rect hover_box;  /* the control under the pointer, w = 0 for none; it is lit at once (surface-hover fill or full ink) */
    uint32_t hover_fill;     /* the fill the hovered control was last painted with (stats line) */
    uint32_t hover_paints;   /* paints of a hovered control (sentinel) */
    sel_gui_rect hover_prev; /* the box the pointer left, w = 0 for none: kept only so its region repaints once and the old box loses its ink on that frame (I356: no fade) */
    int32_t ink_in;          /* the weight hover_ink_of last used for the hovered box (stats line hover_ink) */
    uint32_t doors_drawn;    /* I615a: bit i set: sidebar door i got a row in the last frame (hidden doors stay clear) */
    uint32_t lit_doors;      /* bit i set: sidebar door i was drawn raised in the last frame (nav sentinel) */
    sel_engine_library_item side_items[SIDE_LIB_MAX]; /* I608: the saved playlists listed under the doors */
    uint32_t side_n, side_stale, side_draws, side_rows, side_opens, side_empty_draws, side_create_clicks; /* items held, reload wanted, frames drawn, rows drawn in the last one, rows opened */
    uint32_t side_touch_seen;   /* I678c: lib_touch_calls last seen; a change reloads the list */
    uint32_t side_item_calls; /* I678b: mixed-list reads */
    uint32_t side_open_n, side_open_tries; /* I646: the sidebar count a just created playlist brings the list to (0 = none pending), frames waited */
    uint32_t shelf_open;                                   /* I617: the 1-based shelf the Shelf page shows in full, 0 when none */
    int32_t shelf_back_y_q16;                              /* I617: the Home scroll kept while the Shelf page is open */
    uint32_t show_all_draws, show_all_clicks, shelf_page_draws, shelf_page_opens, shelf_page_tiles; /* I617: Show all links drawn, clicked, and Shelf page frames drawn */
    sel_gui_rect show_all_rect[8];                 /* I617: the Show all link of each shelf as last drawn, w 0 when not drawn */
    uint32_t detail_hidden, np_toggle_clicks, np_add_clicks; /* I619: the Now Playing column is hidden; clicks on its top bar toggle and on the add button */
    sel_gui_rect np_add;                                      /* I619: the round add-to-library button beside the track title */
    char np_ctx_shown[96];                                     /* I619: the heading the last Now Playing draw showed */
    char np_ctx[96];                                          /* I619: the playlist the playing song was started from, "" when none */
    uint32_t topbar_draws, topbar_home_clicks, topbar_browse_clicks; /* I615: top bar frames drawn, Home and Browse button clicks */
    uint32_t side_wide, side_plus_clicks, side_wide_clicks; /* I609: the sidebar is widened; header Create Playlist and width toggle clicks */
    uint32_t side_anim, side_from, side_to, side_tween_frames; /* I668: the width tween runs, its ends in logical px, frames relaid out by it */
    uint32_t side_sub, side_sub_clicks; /* I678d: the second-level chip in force (1 By You), clicks on it */
    sel_gui_rect side_sub_btn;
    sel_gui_rect side_empty_btn; /* I627: the empty-state Create playlist button, zero size when not drawn */
    int32_t side_y_q16; /* the list's scroll offset in Q16 logical pixels */
    uint32_t side_chip, side_filters, side_chip_scrolls; /* I610: the library chip in force (0 none, else 1 + index), filter passes run, chevron scrolls */
    uint32_t side_sort, side_view; /* I611: the Your Library sort (SEL_UIM_LSORT_*) and view size (SEL_UIM_LVIEW_*) */
    uint32_t side_prefs_read, side_pref_sets; /* I611: the sort/view prefs were read once; PREF_SET lines sent for them */
    uint32_t lib_view_sends; /* I611: COMPOSERS_VIEW lines sent for the Library page rows */
    uint32_t lib_tools_draws, lib_tools_clicks, lib_sort_keys; /* I611: Library page tools row draws and clicks, Sort key presses */
    uint32_t lf_open, lf_edit, lf_len; /* I611: the filter field is shown, has the keyboard, and holds this many characters */
    char lf_text[64];
    uint32_t side_sorts, side_sort_picks, side_view_picks, side_menu_opens, side_filter_keys; /* I611: sort passes run, menu picks, menu opens, typed filter keys */
    int np_page;             /* the main region shows the Now Playing page */
    int view_pending;        /* --view named a page: desk_open enters it through desk_page_apply, before the --exec lines */
    desk_page view_pg;
    sel_gui_rect nav_back, nav_fwd; /* the history arrows at the top of the main pane */
    uint32_t nav_icon_draws; /* I599: arrow chevrons drawn through the icon atlas (sentinel) */
    uint32_t transport_hits; /* transport button clicks sent to the engine (DUI-P4-1) */
    uint32_t home_parts;     /* bit set of the Home frame parts drawn (HP_*), the I230 proof that each is on screen */
    uint32_t tile_cards;     /* tile card surfaces drawn in the Home view */
    sel_gui_rect art_probe; /* the last shelf tile art rect drawn, w == 0 when none (home dump line tile_art, I477) */
    uint32_t art_probe_fill; /* the card fill behind that art */
    uint32_t quick_hover_fill; /* the card fill the hovered quick-pick tile was last painted with, 0 when none is hovered (home dump line quick_hover, I490) */
    uint32_t frame_parts;    /* frame parts drawn: sidebar entries, Now Playing panel and transport buttons */
    int32_t shelf_q16[8];    /* horizontal scroll of each Home shelf in logical pixels, Q16 */
    uint32_t hover_shelf1;   /* I607: 1-based Home shelf under the pointer, 0 = none: only that shelf shows its previous/next buttons */
    uint8_t shelf_nav_drawn[8]; /* I607: what the last shelf paint drew per shelf: bit 0 previous button, bit 1 next button */
    uint8_t shelf_nav_alpha[8]; /* I630: the alpha byte the last shelf paint gave each shelf's buttons */
    uint32_t shelf_nav_clicks;  /* I607: previous/next button clicks that moved a shelf */
    uint32_t nav_shelf1;        /* I630: 1-based shelf whose buttons are shown or fading out (the last hovered one) */
    int32_t nav_fade_from, nav_fade_to;       /* I630: button opacity Q16 the fade tween runs between */
    int32_t nav_scale_from[2], nav_scale_to[2]; /* I630: button scale Q16 (65536 = 1.0) per direction, the hover grow tween's ends */
    uint32_t nav_scroll_shelf;  /* I630: the shelf whose click scroll tween runs; shelf_q16 holds the scroll's target */
    int32_t nav_scroll_from;    /* I630: where that scroll started, Q16 */
    int32_t ptr_x, ptr_y;    /* the last pointer position: the wheel scrolls the shelf under it */
    uint32_t shelf_plays;    /* shelf tiles activated (song or album sent to the engine) */
    uint32_t shelf_album_art_draws; /* I469: shelf ALBUM tiles drawn with a library cover in the last shelf paint */
    int32_t home_y_q16;      /* vertical scroll of the whole Home content in logical pixels, Q16 (I247) */
    uint32_t tile_queued;    /* tiles sent to the engine as queue next through the tile menu */
    uint32_t home_sig;       /* source and row count of the last HOME window drawn: a change redraws */
    char home_dump[3072];    /* shelves as text for the stats */
    uint32_t art_ms_max;     /* I377: longest single art_load on the UI thread, host clock ms */
    uint32_t turn_ms_max;    /* I377: longest turn of desk_wait without its poll wait, host clock ms */
    uint32_t poll_ms;        /* I377: poll wait accumulated inside the running turn */
    uint32_t turn_why[5];    /* I525: live turns by where they returned: idle poll, present, scan, input, visualizer */
    uint32_t turn_bits[6];   /* I525: live turns that presented, by the dirty region that asked for it */
    uint32_t frame_ms_min, frame_ms_max; /* I368: ms per paint (one desk_flush), host clock; min is valid once frame_ms_n > 0 */
    uint32_t frame_ms_sum, frame_ms_n;   /* I368: the avg is sum / n */
    uint32_t paint_ms_min, paint_ms_max, paint_ms_sum;     /* I387: the paint share of a frame (frame start to desk_flush), host clock ms */
    uint32_t present_ms_min, present_ms_max, present_ms_sum; /* I387: the present share (backend blit plus sel_gui_present); paint + present = frame */
    uint32_t frame_t0;       /* I385: host clock ms when the frame's paint began; valid while frame_open */
    int frame_open;          /* I385: a paint has begun and its desk_flush has not yet closed the frame */
    uint32_t page_seen;      /* I368: the page the last paint showed, +1 (0 = nothing painted yet) */
    uint32_t page_dirty;     /* I368: pixels the first paint after the latest page change repainted */
    uint32_t pt_t0;           /* I389: clock at the last painter lap */
    uint32_t pt_ms[PT_COUNT]; /* I389: ms per painter (host clock deltas); the primitive classes live in canvas->prof_ms */
    /* I395: cover flow frames by kind (0 rest, 1 slide): frame count, paint ms sum and max, and the primitive-class ms inside them. */
    uint32_t fl_n[2], fl_paint[2], fl_max[2], fl_prof[2][SEL_GUI_PROF_COUNT];
    uint32_t frame_prof0[SEL_GUI_PROF_COUNT]; /* canvas->prof_ms when the open frame began */
    int frame_flow;                           /* 0 not a cover flow frame, 1 rest, 2 slide */
    sel_tag stats[100 + 2 * (SEL_GUI_PROF_COUNT + PT_COUNT) + 2 * SEL_GUI_PROF_COUNT + 10];
    char* input_at;         /* where the scripted input resumes after a turn that sent an engine line */
    uint32_t sends;         /* engine lines sent so far: the replay yields a turn so the shell pumps them */
    char library_dump[16384]; /* the Library page as text for the stats (DUI-P4-3) */
    uint32_t library_sig;    /* hash of the Library page last drawn: a change redraws */
    sel_gui_rect grid_box;                       /* bounding box of the tiles of the last grid frame (I379) */
    sel_gui_rect flow_cover, flow_area;          /* the centre cover and the free area of the last cover flow frame (I391) */
    uint32_t grid_draws, grid_tiles, grid_cols;   /* grid view frames drawn, tiles in the last one, columns of the last one (DUI-P4-5) */
    uint32_t flow_draws, flow_centre;             /* cover flow frames drawn and the listing index of the centred album in the last one */
    int64_t flow_from;                            /* the fractional centre (16.16, listing index) the glide started from (I400) */
    struct { int32_t g, x, w, a; } flow_rec[2 * 3 + 4]; /* the covers of the last cover flow frame in draw order: index, x, size, scrim alpha */
    uint32_t flow_rec_n;
    char flow_snaps[2048];                        /* script `flowsnap` lines: the covers of the frame drawn so far, one line per snap (I400 test) */
    uint32_t lib_mode;       /* Library page mode: 0 composers, 1 tree, 2 column browser (DUI-P4-3 slices B, C; I678i retired files, history, sources) */
    struct { char key[24], kind[12], name[40], path[96]; int enabled; uint32_t count; int64_t synced; int64_t snooze; int due; } src[16]; /* the SOURCES rows of the Sources tab (I265) */
    uint32_t src_n;          /* rows held in src */
    uint32_t nag_off;        /* bit i set: the sync offer (I527) has source row i unchecked; all start checked */
    uint32_t sync_pending;   /* bit i set: a LIBRARY_SYNC for source row i is queued or running; cleared by its outcome (I582) */
    uint32_t sync_failed;    /* bit i set: the last sync of source row i ended in ERROR sync_failed or sync_busy (I582) */
    uint32_t nag_syncs;      /* sources the offer's Sync button sent LIBRARY_SYNC for (the test's sentinel) */
    int nag_asked;           /* the launch SOURCES request was sent (I527) */
    int nag_done;            /* the sync offer dialog was shown this launch: it opens once, on the first Library visit with a due source (I602) */
    uint32_t nagdlg_n;       /* rows the sync offer dialog lists, and which src[] rows they are */
    uint32_t nagdlg_idx[16];
    uint32_t nagdlg_opens;   /* sentinel: sync offer dialogs opened */
    int src_wait;            /* a SOURCES request is out: its first row replaces the held rows */
    char src_syncing[40];    /* name of the source a Sync was sent for; the SOURCES reply that follows it toasts */
    int overlay;             /* the open overlay: OV_NONE, OV_PALETTE, OV_HELP or OV_CMDLINE (DUI-P4-13) */
    txt_palette pal;         /* the palette and help model of the text UI, over the same verbs and keys */
    txt_cmdline cmd;         /* the command line model of the text UI: Tab completes the verb */
    uint32_t quant_calls;    /* median-cut runs over the tile thumbnails (INDEX8) */
    uint32_t palette_opens, help_opens, palette_runs, cmdline_opens, cmdline_runs, overlay_sends_ok;
    uint32_t menu_picks[4];  /* context menu items picked: Play, Play Next, Add to Queue, Remove */
    uint32_t toasts;         /* toasts shown */
    int toast_on;            /* 1 while a toast is on screen */
    int toast_out;           /* 1 while the toast fades out (toast_until is then the end of the fade) */
    uint32_t toast_until;    /* UI clock when the toast goes away */
    char toast_text[160];
    char last_verb[64];      /* the line the palette or the command line sent last */
    const sel_tag* (*find_plugin)(uint32_t kind, const char* id); /* registry lookup the app hands over (DUI-P4-11) */
    sel_viz_render_fn viz_fn[3]; /* the viz plugins found at open: scope, levels, spectrum */
    const char* viz_id[3];
    const char* viz_name[3];
    uint32_t viz_n, viz_sel;     /* plugins found, index of the one the Now Playing strip and the visual layout draw */
    uint32_t* viz_px;            /* RGBA32 target the plugins render into */
    uint32_t viz_cap;            /* pixels viz_px holds */
    uint32_t viz_seq;            /* sequence number of the newest frame drawn */
    uint32_t viz_frames;         /* frames drawn to screen, one per new engine frame (sentinel) */
    uint32_t viz_prims;          /* primitives the last render reported */
    uint32_t viz_panes;          /* panes the last Visualizer page drew */
    uint32_t viz_sigpath;        /* signal path texts drawn */
    uint32_t viz_visual_draws;   /* visual layout frames drawn */
    uint32_t viz_song_ms;        /* song position at the newest frame drawn */
    int viz_page;                /* --view viz: the main region shows the Visualizer page */
    sel_gui_rect np_viz;         /* the strip Now Playing gave the visualizer, w 0 = none */
    sel_gui_rect viz_pane[3];    /* the panes of the Visualizer page */
    sel_gui_rect viz_btn[2][3];  /* the visualizer switch buttons: [0] under the Now Playing strip, [1] on the Visualizer page; w 0 = not drawn */
    uint32_t viz_clicks;         /* switch button clicks that fired an action (sentinel) */
} desk;

static int desk_resize(desk* d, int32_t w, int32_t h);
static int desk_set_scale(desk* d, int32_t scale_q16);
static void desk_relayout(desk* d);
static void side_wide_toggle(desk* d);
enum { SIDE_HDR_PLUS = 1, SIDE_HDR_WIDE = 2, SIDE_HDR_COUNT = 2 }; /* I609: Your Library header controls */
static sel_gui_rect side_hdr_rect(const desk* d, uint32_t which);
static sel_gui_rect side_hdr_title_rect(const desk* d);
static int desk_set_density(desk* d, int32_t density);
static int32_t desk_scale_for(const desk* d, int32_t w, int32_t h);
static int desk_present(desk* d);
static int desk_event_frame(desk* d, int changed);
static void desk_draw_focus(desk* d);
static void desk_draw_tip(desk* d);
static void desk_draw_overlay(desk* d);
static song_row* song_row_reg(desk* d, sel_gui_rect r, const char* path, uint32_t qid, int removable, int home);
static uint32_t desk_toast_wait(const desk* d);
static void desk_toast(desk* d, const char* text);
static int desk_add_folder(desk* d);
static sel_gui_rect toast_rect(desk* d);
static int desk_toast_tick(desk* d);
static int overlay_refresh(desk* d);

enum { ANIM_KEY_VIEW = 1, ANIM_KEY_TIP = 3, ANIM_KEY_FLOW = 4, ANIM_KEY_TOAST = 5, ANIM_KEY_NAV_FADE = 6, ANIM_KEY_NAV_SCROLL = 7, ANIM_KEY_NAV_SCALE = 8 /* +1 per direction */, ANIM_KEY_SIDE = 10 /* I668: sidebar width */ };
enum { HR_NONE, HR_SIDEBAR, HR_MAIN, HR_DETAIL, HR_PLAYER };
static int hover_region_of(const desk* d, sel_gui_rect box);
enum { SLOT_SUB_PREV, SLOT_SHUFFLE, SLOT_PREV, SLOT_PLAY, SLOT_NEXT, SLOT_REPEAT, SLOT_SUB_NEXT, TRANSPORT_COUNT };
static sel_gui_rect desk_button_rect(const desk* d, uint32_t i);
static const char* desk_slot_action(const desk* d, uint32_t i);

/* I385: marks the start of a frame's paint. The first call after a flush wins, so one frame has one start. */
static void desk_frame_begin(desk* d) {
    if (!d->frame_open) {
        const sel_host* ch = sel_engine_plugin_host(d->engine);
        d->frame_t0 = ch->clock_ms(ch);
        d->frame_open = 1;
        d->frame_flow = 0;
        if (d->report && d->canvas != NULL) {
            memcpy(d->frame_prof0, d->canvas->prof_ms, sizeof d->frame_prof0);
        }
    }
}

/* I389: the paint profile (per primitive class in the canvas, per painter in pt_ms) runs only under --stats. */
static uint32_t desk_prof_clock(void* ctx) {
    desk* d = (desk*)ctx;
    const sel_host* ch = sel_engine_plugin_host(d->engine);
    return ch->clock_ms(ch);
}

static void desk_prof_attach(desk* d) {
    if (d->report && d->canvas != NULL) {
        d->canvas->clock = desk_prof_clock;
        d->canvas->clock_ctx = d;
    }
}

/* Adds the ms since pt_t0 to painter slot `pt` and restarts pt_t0, so consecutive painters chain with one clock read each. */
static void desk_pt_lap(desk* d, uint32_t pt) {
    if (d->report) {
        uint32_t now = desk_prof_clock(d);
        d->pt_ms[pt] += now - d->pt_t0;
        d->pt_t0 = now;
    }
}

/* Hands the dirty rectangles to the backend, then empties the list; returns the pixels repainted. The sdl backend
 * must see the list before sel_gui_present clears it. frame_ms covers the whole frame: paint start (desk_frame_begin)
 * through the backend blit and sel_gui_present. */
static uint32_t desk_flush(desk* d) {
    const sel_host* ch = sel_engine_plugin_host(d->engine);
    uint32_t t_flush;
    if (!d->frame_open) { /* a flush with no marked paint still times its blit and present */
        desk_frame_begin(d);
    }
    t_flush = ch->clock_ms(ch);
#ifdef SELECTA_DESKTOP_SDL
    if (d->sdl != NULL) {
        desk_sdl_present(d->sdl, d->canvas);
    }
#endif
    {
        uint32_t px = sel_gui_present(d->canvas);
        uint32_t t_end = ch->clock_ms(ch);
        uint32_t dt = t_end - d->frame_t0;
        if (d->report) { /* I525: when each repaint finished and how long it took */
            uint32_t k;
            fprintf(stderr, "frame end=%u ms=%u px=%u paint=%u present=%u cube_ops=%u", (unsigned)t_end, (unsigned)dt,
                    (unsigned)px, (unsigned)(t_flush - d->frame_t0), (unsigned)(t_end - t_flush),
                    (unsigned)d->canvas->cube_ops);
            for (k = 0; k < SEL_GUI_PROF_COUNT; k++) { /* I574: the primitive classes that took this frame's time */
                uint32_t pk = d->canvas->prof_ms[k] - d->frame_prof0[k];
                if (pk >= 20u) {
                    fprintf(stderr, " prof%u=%u", (unsigned)k, (unsigned)pk);
                }
            }
            fputc('\n', stderr);
        }
        uint32_t paint = t_flush - d->frame_t0, present = t_end - t_flush;
        d->frame_open = 0;
        if (d->frame_flow != 0 && d->report) { /* I395: a cover flow frame, rest or slide */
            uint32_t kind = (uint32_t)d->frame_flow - 1u, k;
            d->fl_n[kind]++;
            d->fl_paint[kind] += paint;
            if (paint > d->fl_max[kind]) {
                d->fl_max[kind] = paint;
            }
            for (k = 0; k < SEL_GUI_PROF_COUNT; k++) {
                d->fl_prof[kind][k] += d->canvas->prof_ms[k] - d->frame_prof0[k];
            }
        }
        if (d->frame_ms_n == 0u || paint < d->paint_ms_min) {
            d->paint_ms_min = paint;
        }
        if (d->frame_ms_n == 0u || present < d->present_ms_min) {
            d->present_ms_min = present;
        }
        if (paint > d->paint_ms_max) {
            d->paint_ms_max = paint;
        }
        if (present > d->present_ms_max) {
            d->present_ms_max = present;
        }
        d->paint_ms_sum += paint;
        d->present_ms_sum += present;
        if (d->frame_ms_n == 0u || dt < d->frame_ms_min) {
            d->frame_ms_min = dt;
        }
        if (dt > d->frame_ms_max) {
            d->frame_ms_max = dt;
        }
        d->frame_ms_sum += dt;
        d->frame_ms_n++;
        if (d->page_seen != (uint32_t)d->page + 1u) { /* the first paint after a page change: its dirty pixels are the page's cost */
            d->page_seen = (uint32_t)d->page + 1u;
            d->page_dirty = px;
        }
        if (d->hover_frame && px > d->hover_dirty_max) {
            d->hover_dirty_max = px;
        }
        d->hover_frame = 0;
        if (d->lib_held_frame) {
            d->lib_held_n++;
            if (px > d->lib_held_px_max) {
                d->lib_held_px_max = px;
            }
            d->lib_held_frame = 0;
        }
        return px;
    }
}

static const char* desk_opt(const sel_ui_open_args* a, const char* name) {
    int i;
    for (i = 0; i + 1 < a->argc; i++) {
        if (strcmp(a->argv[i], name) == 0) {
            return a->argv[i + 1];
        }
    }
    return NULL;
}

static void desk_hash(desk* d, const char* s) {
    for (; *s != '\0'; s++) {
        d->hash = (d->hash ^ (uint8_t)*s) * 16777619u;
    }
}

#define ARGB(tok) (0xFF000000u | (tok))

static sel_gui_rect rect_of(int32_t x, int32_t y, int32_t w, int32_t h) {
    sel_gui_rect r;
    r.x = x;
    r.y = y;
    r.w = w;
    r.h = h;
    return r;
}

/* A logical length in canvas pixels. */
static int32_t lpx(const desk* d, int32_t logical) {
    return (int32_t)(((int64_t)logical * d->scale_q16 + 32768) >> 16);
}

/* A logical Q16 length (a scroll offset) in whole canvas pixels, rounded down: the one helper for the q16 scroll
 * sites (I458). For an integer scale it is (q16 * scale) >> 16, as before. */
static int32_t lpx_q16(const desk* d, int32_t q16) {
    return (int32_t)(((int64_t)q16 * d->scale_q16) >> 32);
}

/* A canvas pixel length in logical units (truncating, as the integer division by the scale did), and rounding up. */
static int32_t px_to_logical(const desk* d, int32_t px) {
    return (int32_t)(((int64_t)px * 65536) / d->scale_q16);
}

static int32_t px_to_logical_ceil(const desk* d, int32_t px) {
    return (int32_t)(((int64_t)px * 65536 + d->scale_q16 - 1) / d->scale_q16);
}

/* I468: the layout height in square pixels: the canvas rows times the pixel aspect (ydiv 2 on the AGA 640x256 screen). */
static int32_t cvh(const desk* d) {
    return d->canvas->h * (d->canvas->ydiv > 1 ? d->canvas->ydiv : 1);
}

static sel_gui_rect region_rect(const desk* d, const char* name) {
    uint32_t i;
    for (i = 0; i < d->layout.count; i++) {
        const sel_uim_region* r = &d->layout.regions[i];
        if (strcmp(r->name, name) == 0) {
            return rect_of(lpx(d, r->x), lpx(d, r->y), lpx(d, (int32_t)r->w), lpx(d, (int32_t)r->h));
        }
    }
    return rect_of(0, 0, 0, 0);
}

/* The atlas for a weight and a size token (pixel size), built from the embedded Inter on first use; NULL when
 * the size is not a token or memory ran out. */
/* I526: the bitmap tier is the native AGA 640x256 screen (ydiv 2): fit zoom 1.0, Amiga bitmap fonts and two Inter
 * headline sizes through the role table of font_roles.c. */
static int desk_bitmap_text(const desk* d) { return d->canvas != NULL && d->canvas->ydiv > 1; }
/* The page title box: 40 layout units for the 32 px title; on the bitmap tier (I545) the 12-row headline (px 24, line
 * 29 units) needs 32, which returns 4 screen rows to the page below it. One rule for every page. */
enum { PAGE_TITLE_H = 40, PAGE_TITLE_H_BITMAP = 32 };
static int32_t page_title_h(const desk* d) { return desk_bitmap_text(d) ? PAGE_TITLE_H_BITMAP : PAGE_TITLE_H; }
/* A layout y that sits below a page title (designed for the full box), pulled up by what the title gave back. */
static int32_t page_y(const desk* d, int32_t y) { return y - (PAGE_TITLE_H - page_title_h(d)); }

/* The role atlas of a token on the bitmap tier for a box of box_h layout units (0 = no fit), counted for the stats. */
static const sel_gui_atlas* desk_role_atlas(desk* d, int semibold, int size, int32_t box_h) {
    int family = 0, px = 0;
    const sel_gui_atlas* at = desk_font_role_atlas(semibold, size, box_h, &family, &px);
    if (at != NULL) {
        if (family >= DESK_FAMILY_MINI4) {
            d->bitmap_hits[family - DESK_FAMILY_MINI4]++;
        } else {
            d->headline_hits[px >= 24 ? 1 : 0]++;
        }
    }
    return at;
}

static const sel_gui_atlas* desk_font(desk* d, int semibold, int size) {
    int i;
    for (i = 0; i < DESK_SIZES; i++) {
        if (g_sizes[i] == size) {
            if (desk_bitmap_text(d)) {
                const sel_gui_atlas* role = desk_role_atlas(d, semibold, size, 0);
                if (role != NULL) {
                    return role;
                }
            }
            if (d->atlas[semibold][i] == NULL) {
                d->atlas[semibold][i] = desk_atlas_baked(semibold, lpx(d, size), d->canvas->ydiv > 1 ? d->canvas->ydiv : 1);
                if (d->atlas[semibold][i] != NULL) {
                    d->atlas_baked_hits++;
                }
#ifndef SEL_DESKTOP_NO_TTF
                else {
                    const sel_host* ch = sel_engine_plugin_host(d->engine);
                    uint32_t t0 = ch->clock_ms(ch);
                    d->atlas[semibold][i] =
                        desk_atlas_build_tall(semibold ? sel_font_inter_semibold : sel_font_inter_regular, lpx(d, size),
                                              d->canvas->ydiv > 1 ? d->canvas->ydiv : 1);
                    d->atlas_ms += ch->clock_ms(ch) - t0;
                    d->atlas_builds++;
                }
#endif
            }
            return d->atlas[semibold][i];
        }
    }
    d->font_misses++;
    return NULL;
}

/* Blend of two 0xAARRGGBB colours, q16 = 0 gives a, 65536 gives b. */
static uint32_t desk_mix(uint32_t a, uint32_t b, int32_t q16) {
    uint32_t out = 0xFF000000u;
    int sh;
    for (sh = 0; sh < 24; sh += 8) {
        int64_t ca = (a >> sh) & 0xFFu, cb = (b >> sh) & 0xFFu;
        out |= (uint32_t)(ca + (((cb - ca) * q16) >> 16)) << sh;
    }
    return out;
}

/* One line of text at the top left of the box, ellipsized to the box width. */
static const sel_gui_atlas* desk_text_atlas(desk* d, int semibold, int px, int32_t box_h) {
    const sel_gui_atlas* at = desk_font(d, semibold, px);
    if (at != NULL && desk_bitmap_text(d) && at->line_height > box_h) { /* I526: a box shorter than the line takes a 6-row font */
        const sel_gui_atlas* fit = desk_role_atlas(d, semibold, px, box_h);
        at = fit != NULL ? fit : at;
    }
    return at;
}

static void draw_text(desk* d, sel_gui_rect box, const char* s, int semibold, int px, uint32_t argb) {
    const sel_gui_atlas* at = desk_text_atlas(d, semibold, px, box.h);
    if (at != NULL) {
        sel_gui_clip_push(d->canvas, box);
        sel_gui_text_draw(d->canvas, at, box.x, box.y + (box.h - at->line_height) / 2, s, argb, box.w, &d->text);
        sel_gui_clip_pop(d->canvas);
    }
}

/* One line of text centred in a button or chip box, ellipsized to the box width minus a pad on each side. Every button
 * and chip label goes through here (I270). The ink span is recorded per box for the stats line. */
static void draw_text_centered_pad(desk* d, sel_gui_rect box, const char* s, int semibold, int px, uint32_t argb, int32_t pad) {
    const sel_gui_atlas* at = desk_font(d, semibold, px);
    int32_t avail = box.w - 2 * pad, tw, ink, x;
    uint32_t i;
    if (at == NULL || avail <= 0) {
        return;
    }
    tw = sel_gui_text_width(at, s, strlen(s));
    ink = tw < avail ? tw : avail;
    x = box.x + (box.w - ink) / 2;
    draw_text(d, rect_of(x, box.y, ink, box.h), s, semibold, px, argb);
    for (i = 0; i < d->label_n && (d->labels[i].box.x != box.x || d->labels[i].box.y != box.y || strncmp(d->labels[i].text, s, sizeof d->labels[i].text - 1u) != 0); i++) {
    }
    if (i < sizeof d->labels / sizeof d->labels[0]) {
        d->labels[i].box = box;
        d->labels[i].ink_x = x;
        d->labels[i].ink_w = ink;
        snprintf(d->labels[i].text, sizeof d->labels[i].text, "%s", s);
        if (i == d->label_n) {
            d->label_n++;
        }
    }
}


static void draw_text_centered(desk* d, sel_gui_rect box, const char* s, int semibold, int px, uint32_t argb) {
    draw_text_centered_pad(d, box, s, semibold, px, argb, lpx(d, 8));
}

static int rect_inside(sel_gui_rect b, sel_gui_rect area) {
    return b.x >= area.x && b.y >= area.y && b.x + b.w <= area.x + area.w && b.y + b.h <= area.y + area.h;
}

static int rect_same(sel_gui_rect a, sel_gui_rect b) {
    return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
}

/* Registers a hoverable control box for the pointer hit-test (a box is listed once). */
static void hover_reg(desk* d, sel_gui_rect box) {
    uint32_t i;
    for (i = 0; i < d->hbox_n; i++) {
        if (rect_same(d->hbox[i], box)) {
            return;
        }
    }
    if (d->hbox_n < sizeof d->hbox / sizeof d->hbox[0]) {
        d->hbox[d->hbox_n++] = box;
    }
}

/* Weight (Q16) of the hover blend for a control: full (65536) the instant the pointer is on it, else 0. Hover switches at once
 * in both directions, as macOS and Spotify do: no fade in (I351) and none out (I356). */
static int32_t hover_weight(const desk* d, sel_gui_rect box) {
    return d->hover_box.w > 0 && rect_same(d->hover_box, box) ? 65536 : 0;
}

/* The ink of a hoverable row or door (I345, I346, I351, I356): `muted` at rest, `full` while the pointer is on it. The one
 * mechanism for the sidebar doors and every list row; they take no background for hover. */
static uint32_t hover_ink_of(desk* d, sel_gui_rect box, uint32_t muted, uint32_t full) {
    int32_t w = hover_weight(d, box);
    hover_reg(d, box);
    if (d->hover_box.w > 0 && rect_same(d->hover_box, box)) {
        d->ink_in = w;
    }
    return desk_mix(muted, full, w);
}

/* I690: a song list marks the row whose song the engine plays. One test for every list: the playing song is the engine's
 * state (song.path), never the row the user clicked, so the mark follows Next, Previous and the auto-advance. */
static int song_playing(desk* d, const char* path) {
    const char* cur = sel_engine_state(d->engine)->song.path;
    return cur != NULL && path != NULL && strcmp(cur, path) == 0;
}

/* The ink of a list row (I345): a muted row warms to the text ink, a text row to the accent, an accent row (the selected or
 * playing one) stays. Only the ink moves; no list row takes a hover background. */
static uint32_t row_ink(desk* d, sel_gui_rect row, uint32_t rest) {
    uint32_t muted = ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT_MUTED]), accent = ARGB(d->theme.token[SEL_GUI_TOKEN_ACCENT]);
    return hover_ink_of(d, row, rest, rest == muted ? ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT]) : accent);
}

/* The fill of a control under the pointer: a plain surface fill fades to the surface-hover token; an accent fill brightens
 * toward white (the surface-hover token would dull it). */
static uint32_t hover_fill_of(desk* d, sel_gui_rect box, uint32_t base) {
    int32_t w = hover_weight(d, box);
    uint32_t out = base;
    if (w > 0) {
        out = base == ARGB(d->theme.token[SEL_GUI_TOKEN_ACCENT])
                  ? desk_mix(base, ARGB(d->theme.token[SEL_GUI_TOKEN_HOVER_LIGHT]), w / 6)
                  : desk_mix(base, ARGB(d->theme.token[SEL_GUI_TOKEN_SURFACE_HOVER]), w);
        d->hover_fill = out;
        d->hover_paints++;
    }
    return out;
}

/* Drops the listed controls and their recorded labels inside a region before it is painted again. The labels are "as last
 * drawn": left over from the page that was replaced they would describe controls that are no longer on screen. */
static void hover_prune(desk* d, sel_gui_rect area) {
    uint32_t i, k = 0;
    for (i = 0; i < d->hbox_n; i++) {
        sel_gui_rect b = d->hbox[i];
        if (!rect_inside(b, area)) {
            d->hbox[k++] = b;
        }
    }
    d->hbox_n = k;
    for (i = 0, k = 0; i < d->label_n; i++) {
        if (!rect_inside(d->labels[i].box, area)) {
            d->labels[k++] = d->labels[i];
        }
    }
    d->label_n = k;
}

/* The one button primitive (I272): a rounded fill and a centred label, with the hover state from the pointer. Every
 * button, chip and tab goes through here. */
static void draw_button_pad(desk* d, sel_gui_rect box, uint32_t base, const char* label, int px, uint32_t ink, int32_t pad) {
    hover_reg(d, box);
    sel_gui_round_rect(d->canvas, box, lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_RADIUS]), hover_fill_of(d, box, base));
    draw_text_centered_pad(d, box, label, 1, px, ink, pad);
}

static void draw_button(desk* d, sel_gui_rect box, uint32_t base, const char* label, int px, uint32_t ink) {
    draw_button_pad(d, box, base, label, px, ink, lpx(d, 8));
}

/* The one chip drawer (Home header chips, Search facets, the sidebar library chips of I610): the selected chip is filled with
 * the accent, an unselected one with the given token. */
static void draw_chip(desk* d, sel_gui_rect box, const char* label, int selected, int off_token) {
    draw_button(d, box, ARGB(d->theme.token[selected ? SEL_GUI_TOKEN_ACCENT : off_token]), label, 13,
                ARGB(d->theme.token[selected ? SEL_GUI_TOKEN_ACCENT_TEXT : SEL_GUI_TOKEN_TEXT]));
}

/* Fill the box with the surface colour, then draw the text (regular 13 pixel Inter, ellipsized), and mark the box
 * dirty. */
static void draw_label(desk* d, sel_gui_rect box, const char* s) {
    sel_gui_fill_rect(d->canvas, box, ARGB(d->theme.token[SEL_GUI_TOKEN_SURFACE]));
    draw_text(d, box, s, 0, 13, ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT]));
    sel_gui_dirty_add(d->canvas, box);
}

/* Same as draw_label but on the frame colour with a type size and an ink: the player bar texts. */
static void draw_frame_text(desk* d, sel_gui_rect box, const char* s, int px, int mark) {
    sel_gui_fill_rect(d->canvas, box, ARGB(d->theme.token[SEL_GUI_TOKEN_FRAME]));
    draw_text(d, box, s, 0, px, ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT_MUTED]));
    if (mark) {
        sel_gui_dirty_add(d->canvas, box);
    }
}

/* A slim rounded bar (seek, volume): the track token behind, the text colour in front, as in the mockup player. The
 * whole bar is repainted into the canvas (the result is the same pixels wherever it did not change), but only the span
 * between the previous and the new fill, plus one bar height of rounded cap on each side, is marked dirty (plan 2.11).
 * *prev is the fill drawn last, or -1 when the bar was not drawn since the last full repaint. */
static void draw_bar(desk* d, sel_gui_rect box, uint32_t q16, int32_t* prev) {
    int32_t fill = (int32_t)(((uint64_t)q16 * (uint32_t)box.w) >> 16);
    int32_t m = lpx(d, 2), lo = 0, hi = box.w;
    sel_gui_rect wide = rect_of(box.x, box.y - m, box.w, box.h + 2 * m);
    sel_gui_fill_rect(d->canvas, wide, ARGB(d->theme.token[SEL_GUI_TOKEN_FRAME]));
    sel_gui_round_rect(d->canvas, box, box.h / 2, ARGB(d->theme.token[SEL_GUI_TOKEN_TRACK]));
    if (fill >= box.h) {
        sel_gui_round_rect(d->canvas, rect_of(box.x, box.y, fill, box.h), box.h / 2,
                           ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT]));
    }
    if (*prev >= 0) {
        lo = (*prev < fill ? *prev : fill) - box.h;
        hi = (*prev < fill ? fill : *prev) + box.h;
        lo = lo < 0 ? 0 : lo;
        hi = hi > box.w ? box.w : hi;
    }
    *prev = fill;
    if (hi > lo) {
        sel_gui_dirty_add(d->canvas, rect_of(box.x + lo, wide.y, hi - lo, wide.h));
    }
}

/* Bits of desk.home_parts. */
enum {
    HP_NAV = 1, HP_BRAND = 2, HP_TILE_CARD = 4, HP_PLAY_HOVER = 8, HP_PLAYER_ART = 16, HP_PLAYER_TITLE = 32,
    HP_SEEK = 64, HP_GREETING = 128, HP_SECTION = 256, HP_ARTIST_LINE = 512, HP_SEARCH_ICON = 1024,
    HP_HEADER_TINT = 2048, HP_PANELS = 4096, HP_CHIPS = 8192
};

/* A region as a raised panel (Main.dc.html: panels on the frame colour, large radius): half the spacing token off each
 * side leaves one spacing token of frame colour between neighbours. */
/* The panel of a region: the region inset by half the spacing. Drawing, hit tests and focus rings all start here. */
static sel_gui_rect panel_rect(const desk* d, const char* name) {
    sel_gui_rect r = region_rect(d, name);
    int32_t g = lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_SPACING]) / 2;
    return rect_of(r.x + g, r.y + g, r.w - 2 * g, r.h - 2 * g);
}

static sel_gui_rect draw_panel(desk* d, const char* name) {
    sel_gui_rect r = region_rect(d, name);
    sel_gui_rect pr = panel_rect(d, name);
    if (pr.w > 0 && pr.h > 0) {
        /* The frame colour first: the rounded corners blend with what is under them, so painting a panel again over
         * its own earlier corners would change them. The region is repainted whole each time (idempotent). */
        sel_gui_fill_rect(d->canvas, r, ARGB(d->theme.token[SEL_GUI_TOKEN_FRAME]));
        sel_gui_round_rect(d->canvas, pr, lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_RADIUS_LARGE]),
                           ARGB(d->theme.token[SEL_GUI_TOKEN_PANEL]));
        sel_gui_dirty_add(d->canvas, r);
        d->home_parts |= HP_PANELS;
    }
    return pr;
}

/* One widget in its rectangle inside its layout region. Only the dirty widgets run, so a clock tick touches the
 * clock, the remaining time and the progress bar and nothing else. */
static void queue_title(const sel_state* st, uint32_t i, char* out, size_t cap);
static void draw_home(desk* d);
static void draw_topbar(desk* d);
static void select_sync(desk* d);
static int select_key(desk* d, const sel_gui_input* ev);
static void tile_palette(desk* d, const char* const* keys, uint32_t n);
static uint32_t art_keys(const sel_state* st, const char** out, uint32_t cap);
static void draw_transport(desk* d);
static void draw_sidebar(desk* d);
static int32_t side_lib_at(const desk* d, int32_t x, int32_t y);
static int32_t side_lib_max_q16(const desk* d);
static void side_lib_reload(desk* d);
static uint32_t lib_touch_calls(desk* d);
static int32_t side_chip_at(desk* d, int32_t x, int32_t y);
static int32_t side_tools_at(const desk* d, int32_t x, int32_t y);
static sel_gui_rect side_mag_rect(const desk* d);
static sel_gui_rect side_sort_rect(const desk* d);
static sel_gui_rect side_field_rect(const desk* d);
static uint32_t side_lib_cols(const desk* d);
static void tl_click(desk* d, sel_gui_rect t, int32_t k);
static void tl_draw(desk* d, sel_gui_rect t);
static sel_gui_rect lib_tools_rect(desk* d, sel_gui_rect mn);
static sel_gui_rect tl_mag_rect(const desk* d, sel_gui_rect t);
static sel_gui_rect tl_sort_rect(const desk* d, sel_gui_rect t);
static int32_t tl_at(const desk* d, sel_gui_rect t, int32_t x, int32_t y);
static void lsort_prefs_load(desk* d);
static sel_gui_rect lmenu_strip_rect(const desk* d, uint32_t seg);
static void side_sort_pick(desk* d, uint32_t sort);
static void side_view_pick(desk* d, uint32_t view);
static void side_chip_click(desk* d, int32_t c);
static int side_chip_single(desk* d);
static int side_chip_has_x(desk* d);
static int side_lib_open_item(const desk* d, uint32_t i);
static sel_gui_rect side_chip_rect(desk* d, uint32_t k);
static int32_t side_chips_h(const desk* d);
static int side_table(const desk* d);
static int32_t side_table_head_h(const desk* d);
static int32_t side_lib_row_h(const desk* d);
static uint32_t side_pinned_n(const desk* d);
static int32_t side_card_h(const desk* d);
static sel_gui_rect side_chip_chevron(desk* d, int right);
static sel_gui_rect side_lib_row_rect(const desk* d, uint32_t i);
static void draw_nowplaying(desk* d);
static void draw_nowplaypage(desk* d);
static void draw_nav_arrows(desk* d, sel_gui_rect mn);
static sel_gui_rect nav_arrow_rect(const desk* d, sel_gui_rect mn, int32_t k);
static int nav_arrows_shown(const desk* d, sel_gui_rect mn);
static sel_gui_rect sidebar_door_rect(const desk* d, uint32_t i);
static void draw_player_left(desk* d);

/* The footer bars (I312). The seek row spans tw pixels from x0, centred over the transport buttons, with the times at
 * both ends; the volume bar sits at the right end under the status text, its speaker icon to its left. Drawing and
 * the pointer hit test both take their boxes from here. */
/* DUI-P4-20: on a short canvas (layout height at or under SEL_UIM_FOOTER_ONE_ROW_MAX_H) the player bar is one row:
 * transport buttons, elapsed time, the seek bar over the free width, total time, speaker and volume bar, all centred
 * on the row. footer_y0 is the top of the 16 px text row in both footers; the bars sit 6 px under it. */
static int footer_one_row(const desk* d) {
    return d->layout.height <= SEL_UIM_FOOTER_ONE_ROW_MAX_H;
}

static int32_t footer_y0(const desk* d) {
    sel_gui_rect pl = region_rect(d, "player");
    if (footer_one_row(d)) {
        return pl.y + (pl.h - lpx(d, 16)) / 2;
    }
    return pl.y + lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_SPACING]);
}

static void seek_span(const desk* d, int32_t* x0, int32_t* tw) {
    sel_gui_rect pl = region_rect(d, "player");
    int32_t lw = lpx(d, 40), w = lpx(d, 560);
    if (footer_one_row(d)) { /* from the last transport button to the speaker icon */
        int32_t sp = lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_SPACING]);
        sel_gui_rect last = desk_button_rect(d, desk_slot_action(d, SLOT_SUB_NEXT) != NULL ? SLOT_SUB_NEXT : SLOT_REPEAT);
        sel_gui_rect vb = pl;
        int32_t right;
        vb.x = pl.x + pl.w - sp - lpx(d, 96) - sp - lpx(d, 16);
        right = vb.x - sp;
        *x0 = last.x + last.w + 2 * sp;
        *tw = right - *x0;
        return;
    }
    if (w > pl.w - 2 * lpx(d, 300)) {
        w = pl.w - 2 * lpx(d, 300);
    }
    if (w < 4 * lw) {
        w = 4 * lw;
    }
    *x0 = pl.x + pl.w / 2 - w / 2;
    *tw = w;
}

static sel_gui_rect seek_bar_rect(const desk* d) {
    int32_t sp = lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_SPACING]), lw = lpx(d, 40), x0, tw;
    seek_span(d, &x0, &tw);
    return rect_of(x0 + lw + sp, footer_y0(d) + lpx(d, 6), tw - 2 * lw - 2 * sp, lpx(d, 4));
}

static sel_gui_rect vol_bar_rect(const desk* d) {
    sel_gui_rect pl = region_rect(d, "player");
    int32_t sp = lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_SPACING]);
    return rect_of(pl.x + pl.w - sp - lpx(d, 96), footer_one_row(d) ? footer_y0(d) + lpx(d, 6) : pl.y + sp + lpx(d, 40), lpx(d, 96),
                   lpx(d, 4));
}

/* The window-level overlays (the song menu and the dialog) float above every region, so a repaint of ANY region (the
 * sidebar hover fade, the queue widget) must end with this pass or it paints over the part of them it overlaps. */
static void song_menu_draw(desk* d);
static void dlg_draw(desk* d);
static void desk_draw_window_overlays(desk* d) {
    song_menu_draw(d);
    dlg_draw(d);
}

static void draw_widget(desk* d, uint32_t bit) {
    const sel_uim_frame* f = &d->uim.frame;
    int32_t sp = lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_SPACING]);
    sel_gui_rect pl = region_rect(d, "player"), mn = region_rect(d, "main");
    int32_t y0 = footer_y0(d);
    if (bit == SEL_UIM_DIRTY_SONG) {
        draw_player_left(d);
        if (d->home && !d->songs && !d->queue_page && !d->pl_page && d->open_tile == 0) { /* the Home header replaces the song card */
            if (d->np_page) { /* the Now Playing page follows the song */
                draw_home(d);
            }
            draw_nowplaying(d);
            return;
        }
        sel_gui_rect card = rect_of(mn.x + sp, mn.y + sp, mn.w - 2 * sp < lpx(d, 400) ? mn.w - 2 * sp : lpx(d, 400), lpx(d, 48));
        if (d->shadow_ready) {
            sel_gui_shadow_draw(d->canvas, &d->shadow, card, lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_RADIUS]));
            sel_gui_dirty_add(d->canvas, rect_of(card.x - lpx(d, 8), card.y - lpx(d, 8), card.w + lpx(d, 16), card.h + lpx(d, 16)));
        }
        sel_gui_round_rect(d->canvas, card, lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_RADIUS]),
                           ARGB(d->theme.token[SEL_GUI_TOKEN_SURFACE]));
        draw_label(d, rect_of(card.x + sp, card.y + sp, card.w - 2 * sp, lpx(d, 16)), f->title);
        draw_label(d, rect_of(card.x + sp, card.y + sp + lpx(d, 20), card.w - 2 * sp, lpx(d, 16)), f->artist);
        sel_gui_dirty_add(d->canvas, card);
        draw_nowplaying(d);
    } else if (bit == SEL_UIM_DIRTY_QUEUE) {
        draw_topbar(d);
        draw_sidebar(d);
        desk_pt_lap(d, PT_SIDEBAR);
        if (d->home) {
            draw_home(d);
        } else {
            desk_draw_window_overlays(d);
        }
    } else if (bit == SEL_UIM_DIRTY_TRANSPORT) {
        draw_transport(d);
    } else if (bit == SEL_UIM_DIRTY_PROGRESS) {
        /* The seek row (mockup): times at both ends of a slim bar, centred over the transport buttons. */
        int32_t lw = lpx(d, 40), tw, x0;
        seek_span(d, &x0, &tw);
        int len_changed = strncmp(d->seek_length, f->length, sizeof d->seek_length) != 0;
        draw_frame_text(d, rect_of(x0, y0, lw, lpx(d, 16)), f->clock, 12, 1);
        draw_bar(d, seek_bar_rect(d), f->progress_q16, &d->seek_fill);
        draw_frame_text(d, rect_of(x0 + tw - lw, y0, lw, lpx(d, 16)), f->length, 12, len_changed);
        snprintf(d->seek_length, sizeof d->seek_length, "%s", f->length);
        d->home_parts |= HP_SEEK;
    } else if (bit == SEL_UIM_DIRTY_VOLUME) {
        int32_t v = f->volume < 0 ? 0 : f->volume > 100 ? 100 : f->volume;
        sel_gui_rect vb = vol_bar_rect(d);
        int32_t ic = lpx(d, 16);
        sel_gui_rect ir = rect_of(vb.x - sp - ic, vb.y + vb.h / 2 - ic / 2, ic, ic);
        sel_gui_fill_rect(d->canvas, ir, ARGB(d->theme.token[SEL_GUI_TOKEN_FRAME]));
        sel_gui_icon_draw(d->canvas, &d->icons, SEL_GUI_ICON_VOLUME_2, ir, ic, ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT_MUTED]));
        sel_gui_dirty_add(d->canvas, ir);
        draw_bar(d, vb, (uint32_t)v * 65536u / 100u, &d->vol_fill);
    } else if (bit == SEL_UIM_DIRTY_STATUS) {
        if (footer_one_row(d)) { /* the play or pause button already shows the state */
            return;
        }
        draw_frame_text(d, rect_of(pl.x + pl.w - sp - lpx(d, 200), y0, lpx(d, 200), lpx(d, 16)), f->status, 12, 1);
    }
}

/* The display title of queue entry i: the file name without directory and extension, UTF-8 as stored. */
static void queue_title(const sel_state* st, uint32_t i, char* out, size_t cap) {
    const char* path = st->queue_text + st->queue[i].path_offset;
    const char* base = path;
    const char* p;
    size_t n;
    for (p = path; *p != '\0'; p++) {
        if (*p == '/' || *p == ':') {
            base = p + 1;
        }
    }
    n = strlen(base);
    for (p = base + n; p > base; p--) {
        if (p[-1] == '.') {
            n = (size_t)(p - 1 - base);
            break;
        }
    }
    if (n >= cap) {
        n = cap - 1u;
        while (n > 0u && (((unsigned char)base[n]) & 0xC0u) == 0x80u) { /* never cut a UTF-8 sequence */
            n--;
        }
    }
    memcpy(out, base, n);
    out[n] = '\0';
}

/* The text sheet (DUI-P2-3): every size in both weights, then each queue name once ellipsized to one narrow line and
 * once wrapped into three lines, so the golden hash covers glyphs, kerning, ellipsis and wrap. */
static void draw_textsheet(desk* d) {
    sel_gui_canvas* c = d->canvas;
    const sel_state* st = sel_engine_state(d->engine);
    uint32_t text = ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT]);
    int32_t y = 8, row;
    uint32_t i;
    int k, w;
    char name[256];
    sel_gui_fill_rect(c, rect_of(0, 0, c->w, c->h), ARGB(d->theme.token[SEL_GUI_TOKEN_SURFACE]));
    for (k = 0; k < DESK_SIZES; k++) {
        for (w = 0; w < 2; w++) {
            const sel_gui_atlas* at = desk_font(d, w, g_sizes[k]);
            if (at != NULL) {
                sel_gui_text_draw(c, at, 8, y, "Selecta: the quick brown fox jumps over 0123456789", text, 500, &d->text);
                y += at->line_height + 2;
            }
        }
    }
    for (i = 0, row = 0; i < st->queue_count && i < 12u; i++, row++) {
        const sel_gui_atlas* semi = desk_font(d, 1, 13);
        const sel_gui_atlas* reg = desk_font(d, 0, 13);
        queue_title(st, i, name, sizeof name);
        if (semi != NULL && reg != NULL) {
            sel_gui_text_draw(c, semi, 520, 8 + row * 60, name, text, 200, &d->text);
            sel_gui_text_wrap(c, reg, rect_of(740, 8 + row * 60, 140, 3 * reg->line_height), name, text, 3, &d->text);
        }
    }
    sel_gui_dirty_add(c, rect_of(0, 0, c->w, c->h));
}

/* The widget gallery (DUI-P3-1): every widget kind drawn once on the background token, in a grid of surface cards.
 * The style comes from the theme tokens, so the golden hash covers a dark and a light theme. */
static void gallery_put_ex(desk* d, const sel_gui_style* st, uint32_t kind, int32_t x, int32_t y, int32_t w, int32_t h,
                           uint32_t state, const char* label, int32_t icon, int32_t value, const char* const* items,
                           uint32_t item_n, int32_t sel, const int32_t* icons) {
    sel_gui_widget wd;
    memset(&wd, 0, sizeof wd);
    wd.kind = kind;
    wd.r = rect_of(x, y, w, h);
    wd.state = state;
    wd.label = label;
    wd.icon = icon;
    wd.value = value;
    wd.extent = SEL_GUI_Q16_ONE / 4;
    wd.items = items;
    wd.item_n = item_n;
    wd.sel = sel;
    wd.item_icons = icons;
    wd.top = 1;
    sel_gui_widget_draw(d->canvas, st, &wd, &d->wstats);
    d->widget_draws++;
}

static void gallery_put(desk* d, const sel_gui_style* st, uint32_t kind, int32_t x, int32_t y, int32_t w, int32_t h,
                        uint32_t state, const char* label, int32_t icon, int32_t value, const char* const* items,
                        uint32_t item_n, int32_t sel) {
    gallery_put_ex(d, st, kind, x, y, w, h, state, label, icon, value, items, item_n, sel, NULL);
}

static void draw_widgets(desk* d) {
    static const char* const songs[] = {"Axel F", "Cannon Fodder", "Stardust Memories", "Hybris", "Elysium"};
    static const char* const tabs[] = {"Songs", "Albums", "Artists"};
    static const char* const head[] = {"Title", "Artist", "Time"};
    static const char* const side[] = {"Home", "Search", "Library", "Favourites"};
    static const int32_t side_icons[] = {SEL_GUI_ICON_HOUSE, SEL_GUI_ICON_SEARCH, SEL_GUI_ICON_LIST, SEL_GUI_ICON_HEART};
    static const char* const panes[] = {"Library", "Details"};
    static const char* const ask[] = {"This removes the song from your library. The file stays on disk.", "Cancel", "Remove"};
    static const char* const menu[] = {"Play next", "Add to queue", "-", "Show in library"};
    static const char* const seg[] = {"Day", "Week", "Month"};
    static const char* const tiles[] = {"Amiga Classics", "Chip Hits", "Tracker Night", "Deep Focus"};
    sel_gui_canvas* c = d->canvas;
    sel_gui_style st;
    const int32_t half = SEL_GUI_Q16_ONE / 2, c1 = 20, c2 = 330, c3 = 640, c4 = 950, cw = 300;
    const uint32_t hov = SEL_GUI_WS_HOVER, sl = SEL_GUI_WS_SELECTED;
    int32_t cardr = (int32_t)d->theme.token[SEL_GUI_TOKEN_RADIUS_LARGE];
    int32_t i;
    memset(&st, 0, sizeof st);
    st.background = ARGB(d->theme.token[SEL_GUI_TOKEN_BACKGROUND]);
    st.surface = ARGB(d->theme.token[SEL_GUI_TOKEN_SURFACE]);
    st.raised = ARGB(d->theme.token[SEL_GUI_TOKEN_SURFACE_RAISED]);
    st.hover = ARGB(d->theme.token[SEL_GUI_TOKEN_SURFACE_HOVER]);
    st.pressed = ARGB(d->theme.token[SEL_GUI_TOKEN_SURFACE_PRESSED]);
    st.text = ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT]);
    st.muted = ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT_MUTED]);
    st.disabled = ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT_DISABLED]);
    st.accent = ARGB(d->theme.token[SEL_GUI_TOKEN_ACCENT]);
    st.accent_text = ARGB(d->theme.token[SEL_GUI_TOKEN_ACCENT_TEXT]);
    st.divider = ARGB(d->theme.token[SEL_GUI_TOKEN_DIVIDER]);
    st.focus = ARGB(d->theme.token[SEL_GUI_TOKEN_FOCUS_RING]);
    st.error = ARGB(d->theme.token[SEL_GUI_TOKEN_ERROR]);
    st.warning = ARGB(d->theme.token[SEL_GUI_TOKEN_WARNING]);
    st.success = ARGB(d->theme.token[SEL_GUI_TOKEN_SUCCESS]);
    st.scrim = ARGB(d->theme.token[SEL_GUI_TOKEN_SCRIM]);
    st.radius_small = (int32_t)d->theme.token[SEL_GUI_TOKEN_RADIUS_SMALL];
    st.radius = (int32_t)d->theme.token[SEL_GUI_TOKEN_RADIUS];
    st.radius_large = cardr;
    st.spacing = (int32_t)d->theme.token[SEL_GUI_TOKEN_SPACING];
    st.body = desk_font(d, 0, 13);
    st.semibold = desk_font(d, 1, 13);
    st.caption = desk_font(d, 0, 11);
    st.icons = &d->icons;
    st.shadow = d->shadow_ready ? &d->shadow : NULL;
    if (st.body == NULL || st.semibold == NULL || st.caption == NULL) {
        return;
    }
    sel_gui_fill_rect(c, rect_of(0, 0, c->w, c->h), st.background);
    for (i = 0; i < 4; i++) { /* the four column cards */
        sel_gui_round_rect(c, rect_of(10 + i * 310, 10, cw + 20, c->h - 20), cardr, st.surface);
    }
    /* column 1: controls */
    gallery_put(d, &st, SEL_GUI_WIDGET_LABEL, c1, 24, cw, 22, 0, "Now playing", -1, 0, NULL, 0, 0);
    gallery_put(d, &st, SEL_GUI_WIDGET_ICON, c1, 60, 24, 24, 0, NULL, SEL_GUI_ICON_MUSIC, 0, NULL, 0, 0);
    gallery_put(d, &st, SEL_GUI_WIDGET_ICON_BUTTON, c1 + 40, 52, 40, 40, sl, NULL, SEL_GUI_ICON_PLAY, 0, NULL, 0, 0);
    gallery_put(d, &st, SEL_GUI_WIDGET_ICON_BUTTON, c1 + 90, 52, 40, 40, hov, NULL, SEL_GUI_ICON_SHUFFLE, 0, NULL, 0, 0);
    gallery_put(d, &st, SEL_GUI_WIDGET_ICON_BUTTON, c1 + 140, 52, 40, 40, SEL_GUI_WS_DISABLED, NULL, SEL_GUI_ICON_REPEAT, 0,
                NULL, 0, 0);
    gallery_put(d, &st, SEL_GUI_WIDGET_TEXT_BUTTON, c1, 108, 130, 36, sl, "Play", -1, 0, NULL, 0, 0);
    gallery_put(d, &st, SEL_GUI_WIDGET_TEXT_BUTTON, c1 + 140, 108, 130, 36, hov, "Follow", -1, 0, NULL, 0, 0);
    gallery_put(d, &st, SEL_GUI_WIDGET_TOGGLE, c1, 160, 44, 24, sl, NULL, -1, 0, NULL, 0, 0);
    gallery_put(d, &st, SEL_GUI_WIDGET_TOGGLE, c1 + 60, 160, 44, 24, 0, NULL, -1, 0, NULL, 0, 0);
    gallery_put(d, &st, SEL_GUI_WIDGET_SLIDER, c1, 200, cw, 20, hov, NULL, -1, SEL_GUI_Q16_ONE * 2 / 5, NULL, 0, 0);
    gallery_put(d, &st, SEL_GUI_WIDGET_PROGRESS, c1, 236, cw, 6, 0, NULL, -1, SEL_GUI_Q16_ONE * 13 / 20, NULL, 0, 0);
    gallery_put(d, &st, SEL_GUI_WIDGET_CHECKBOX, c1, 258, 140, 24, sl, "Shuffle", -1, 0, NULL, 0, 0);
    gallery_put(d, &st, SEL_GUI_WIDGET_RADIO, c1 + 150, 258, 140, 24, sl, "Repeat", -1, 0, NULL, 0, 0);
    gallery_put(d, &st, SEL_GUI_WIDGET_SEGMENTED, c1, 298, cw, 32, 0, NULL, -1, 0, seg, 3, 1);
    gallery_put(d, &st, SEL_GUI_WIDGET_DROPDOWN, c1, 346, cw, 36, 0, "Sort by recent", -1, 0, NULL, 0, 0);
    gallery_put(d, &st, SEL_GUI_WIDGET_TEXT_FIELD, c1, 398, cw, 36, SEL_GUI_WS_FOCUS, "Search", SEL_GUI_ICON_SEARCH, 0,
                NULL, 0, 0);
    gallery_put(d, &st, SEL_GUI_WIDGET_TABS, c1, 450, cw, 36, 0, NULL, -1, 0, tabs, 3, 0);
    gallery_put(d, &st, SEL_GUI_WIDGET_TABLE_HEADER, c1, 502, cw, 28, 0, NULL, -1, 0, head, 3, 0);
    gallery_put(d, &st, SEL_GUI_WIDGET_SPLIT_PANE, c1, 550, cw - 24, 200, 0, NULL, -1, half, panes, 2, 0);
    gallery_put(d, &st, SEL_GUI_WIDGET_SCROLL_BAR, c1 + cw - 12, 550, 12, 200, 0, NULL, -1, half, NULL, 0, 0);
    /* column 2: lists and menus */
    gallery_put(d, &st, SEL_GUI_WIDGET_VIRTUAL_LIST, c2, 24, cw, 200, 0, NULL, -1, 0, songs, 5, 1);
    gallery_put_ex(d, &st, SEL_GUI_WIDGET_SIDEBAR_LIST, c2, 244, cw, 200, 0, NULL, SEL_GUI_ICON_HOUSE, 0, side, 4, 0,
                   side_icons);
    gallery_put(d, &st, SEL_GUI_WIDGET_MENU, c2, 464, cw, 140, 0, NULL, -1, 0, menu, 4, 1);
    gallery_put(d, &st, SEL_GUI_WIDGET_TOOLTIP, c2, 630, 140, 28, 0, "Add to favourites", -1, 0, NULL, 0, 0);
    gallery_put(d, &st, SEL_GUI_WIDGET_TOAST, c2, 680, cw, 56, 0, "Added to queue", -1, 0, NULL, 0, 0);
    /* column 3: artwork */
    gallery_put(d, &st, SEL_GUI_WIDGET_TILE_GRID, c3, 24, cw, 270, 0, "Playlist", -1, 0, tiles, 2, 1);
    gallery_put(d, &st, SEL_GUI_WIDGET_COVER_FLOW, c3, 314, cw, 190, 0, NULL, -1, 0, NULL, 0, 3);
    gallery_put(d, &st, SEL_GUI_WIDGET_ARTWORK, c3, 524, 130, 130, 0, NULL, SEL_GUI_ICON_DISC, 0, NULL, 0, 0);
    gallery_put(d, &st, SEL_GUI_WIDGET_VIZ_SURFACE, c3 + 150, 524, 150, 130, 0, NULL, -1, 7 << 16, NULL, 0, 0);
    /* column 4: an overlay stack, a scrim and a dialog over a tile grid */
    gallery_put(d, &st, SEL_GUI_WIDGET_TILE_GRID, c4, 24, cw, 270, 0, "Playlist", -1, 0, tiles, 2, 0);
    gallery_put(d, &st, SEL_GUI_WIDGET_SCRIM, 0, 0, c->w, c->h, 0, NULL, -1, SEL_GUI_Q16_ONE * 3 / 5, NULL, 0, 0);
    gallery_put(d, &st, SEL_GUI_WIDGET_MODAL_DIALOG, (c->w - 280) / 2, (c->h - 190) / 2, 280, 190, 0, "Remove from library",
                -1, 0, ask, 3, 0);
    d->text.glyphs += d->wstats.text.glyphs;
    d->text.missing += d->wstats.text.missing;
    d->wstats.text.glyphs = 0;
    d->wstats.text.missing = 0;
    sel_gui_dirty_add(c, rect_of(0, 0, c->w, c->h));
}

#define TILE_ART 144 /* logical, the default step */
#define TILE_STEP_DEFAULT 2u
#define SCROLL_NOTCH 16 /* logical pixels one wheel notch scrolls */
#define TILE_STEPS SEL_UIM_TILE_STEPS /* the tile size steps are shared: uimodel/browse.h */
#define HOME_MAX_LINES 2

/* Home layout (Main.dc.html), logical pixels from the main panel's top: the header with the search field, the
 * greeting, the filter chips, the section heading, then the tile grid. */
enum { HOME_MARGIN = 24, HOME_SEARCH_MAX = 360, HOME_GREET_Y = 72,
       HOME_GREET_H = 40, HOME_CHIPS_Y = 124, HOME_CHIP_H = 32, HOME_SECTION_Y = 172, HOME_SECTION_H = 32,
       HOME_SHADOW_R = 8, HOME_COVER_SHADOW_LIFT = 4 /* I507: cover shadow sits this far inside the sides and lower */, HOME_HEAD_GAP = 8, /* tile shadow radius; clear space between a section heading and that shadow (I449) */
       HOME_TILES_Y = HOME_SECTION_Y + HOME_SECTION_H + HOME_SHADOW_R + HOME_HEAD_GAP, HOME_TILE_GAP = 4, HOME_TINT_H = 300, HOME_PLAY_SIZE = 40, HOME_PLAY_INSET = 8,
       HOME_TILE_TEXT_GAP = 8, HOME_TITLE_H = 18, HOME_META_H = 16, HOME_BRAND = 28 };

/* Where tile card i sits (DUI-P2-6, Main.dc.html): a grid of cards, each the art plus the tile padding token on every
 * side and the title and meta lines below, columns as many as fit. */
static sel_gui_rect tile_rect(desk* d, uint32_t i) {
    int32_t pad = lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_TILE_PAD]);
    sel_gui_rect mn = region_rect(d, "main");
    int32_t art = lpx(d, sel_uim_tile_step(d->tile_step));
    int32_t gap = lpx(d, HOME_TILE_GAP);
    int32_t cw = art + 2 * pad;
    int32_t cols = (mn.w - 2 * lpx(d, HOME_MARGIN) + 2 * pad + gap) / (cw + gap);
    int32_t h = pad + art + lpx(d, HOME_TILE_TEXT_GAP + HOME_TITLE_H + HOME_META_H) + pad;
    if (cols < 1) {
        cols = 1;
    }
    return rect_of(mn.x + lpx(d, HOME_MARGIN) - pad + (int32_t)(i % (uint32_t)cols) * (cw + gap),
                   mn.y + lpx(d, HOME_TILES_Y) - pad + (int32_t)(i / (uint32_t)cols) * (h + gap), cw, h);
}

/* The detail view of an opened tile: the main region cleared, the title large. */
static int32_t songs_top(const desk* d);
static void draw_detail(desk* d) {
    const sel_state* st = sel_engine_state(d->engine);
    int32_t sp = lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_SPACING]);
    sel_gui_rect mn = region_rect(d, "main");
    const sel_gui_atlas* big = desk_font(d, 1, 24);
    char name[256];
    /* Below the now-playing card strip, like the home and songs views: the card is a widget with its own dirty bit,
     * so a view that wipes it leaves a hole on the way back (I245). */
    sel_gui_rect body = rect_of(mn.x, songs_top(d), mn.w, mn.y + mn.h - songs_top(d));
    sel_gui_fill_rect(d->canvas, body, ARGB(d->theme.token[SEL_GUI_TOKEN_SURFACE]));
    if (big != NULL && d->open_tile >= 1 && (uint32_t)d->open_tile <= st->queue_count) {
        queue_title(st, (uint32_t)d->open_tile - 1u, name, sizeof name);
        sel_gui_clip_push(d->canvas, body);
        sel_gui_text_wrap(d->canvas, big, rect_of(mn.x + sp, body.y, mn.w - 2 * sp, 3 * big->line_height), name,
                          ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT]), 3, &d->text);
        sel_gui_clip_pop(d->canvas);
    }
    sel_gui_dirty_add(d->canvas, body);
}

/* The lower case family id of a pc_* slot (family + 1), "none" when real art won. */
static const char* pc_name(uint32_t slot) {
    return (slot & 0xFFu) == 0u ? "none" : sel_platform_id((slot & 0xFFu) - 1u);
}

/* The logical pixel size of the word a cover drew (0 = none), kept in the bits above the family byte of a pc_* slot. */
static uint32_t pc_word(uint32_t slot) {
    return slot >> 8;
}

/* The platform cover of `family` (I691b, I691d) on `cv`: the one drawing path of every fallback cover. `slot` receives family + 1
 * (plus the word px above the family byte) for the dump line `platform_covers`; draws counts every call, the sentinel of the
 * fallback branch. Covers under 64 logical px draw the field and mark only. */
static void family_platform_cover(desk* d, sel_gui_canvas* cv, sel_gui_rect ar, uint32_t family, const char* title, uint32_t* slot) {
    int32_t edge = px_to_logical(d, ar.w < ar.h ? ar.w : ar.h); /* logical: a 2x canvas shows a 56 px bar thumb at 112 px */
    int32_t want = edge / 7, px = 0, i;
    const sel_gui_atlas* word = NULL;

    if (edge >= 64) { /* design section 2: the word only on covers of 64 logical px and up, semibold, sized edge/7 */
        for (i = 0; i < DESK_SIZES; i++) { /* the largest type scale size not above edge/7, the smallest when none fits */
            if (g_sizes[i] <= want || px == 0) {
                px = g_sizes[i];
            }
        }
        word = desk_font(d, 1, px);
    }
    sel_gui_cover_draw(cv, ar, family, sel_gui_str_hash(title != NULL ? title : ""), d->theme.token, word);
    if (slot != NULL) {
        *slot = (family + 1u) | (word != NULL ? (uint32_t)px << 8 : 0u);
    }
    d->pc_draws++;
}

/* The song fallback (I691b): the platform cover of the family of the file name `path`. */
static void song_platform_cover(desk* d, sel_gui_rect ar, const char* path, const char* title, uint32_t* slot) {
    family_platform_cover(d, d->canvas, ar, sel_platform_of_name(path), title, slot);
}

/* The home view (DUI-P2-6): one tile per queue entry, its album artwork (DUI-P2-4) or else generated art of its
 * format (DUI-P2-5), and the title wrapped under it. The queue is the tile source. */
/* The format of queue entry i: its file extension, "" without one. */
static void queue_format(const sel_state* st, uint32_t i, char* out, size_t cap) {
    const char* path = st->queue_text + st->queue[i].path_offset;
    const char* ext = sel_platform_path_ext(path);
    if (ext == NULL || strlen(ext) >= cap) {
        out[0] = '\0';
        return;
    }
    strcpy(out, ext);
}

/* 64-bit FNV-1a over the pixels of r. */
static uint64_t rect_hash(const sel_gui_canvas* c, sel_gui_rect r) {
    uint64_t h = SEL_GUI_FNV64_BASIS;
    int32_t x, y;
    for (y = r.y; y < r.y + r.h; y++) {
        for (x = r.x; x < r.x + r.w; x++) {
            h = sel_gui_fnv64_word(h, sel_gui_pixel(c, x, y));
        }
    }
    return h;
}

/* Loads the artwork of every key art_keys names on the worker (art.h) before the tiles are drawn. */
static void load_tile_art(desk* d, const char* const* keys, uint32_t n) {
    if (d->art == NULL || n == 0u) {
        return;
    }
    {
        const sel_host* ch = sel_engine_plugin_host(d->engine);
        uint32_t t0 = ch->clock_ms(ch), dt;
        art_load(d->art, keys, n);
        dt = ch->clock_ms(ch) - t0;
        if (dt > d->art_ms_max) {
            d->art_ms_max = dt;
        }
    }
}

/* I692: the real cover of the song (queue path) or album ("<album>" or "<album>\x1f<artist>", art.h) that `key` names, drawn through the same
 * art_tile blit as the album grid and the queue cards. Looks it up on first ask (art_known). Returns 0 when it has no cover: the caller draws
 * the generated art then. */
static int cover_draw(desk* d, sel_gui_rect ar, const char* key, int32_t radius) {
    const uint32_t* art;
    if (d->art == NULL || key == NULL || key[0] == '\0') {
        return 0;
    }
    if (!art_known(d->art, key)) {
        load_tile_art(d, &key, 1u);
    }
    art = art_tile(d->art, key);
    if (art == NULL) {
        return 0;
    }
    sel_gui_set_region(d->canvas, SEL_GUI_RGN_TILE);
    sel_gui_blit_image_round(d->canvas, ar, art, DESK_ART_TILE_EDGE, DESK_ART_TILE_EDGE, DESK_ART_TILE_EDGE, radius);
    sel_gui_set_region(d->canvas, SEL_GUI_RGN_UI);
    return 1;
}

/* A grid or cover flow tile (I691d): the real art of the row first (precedence, design section 4), else the platform cover of its
 * family: the row's majority format when the engine gave one, else the file name of its song `path`. `slot` as in
 * family_platform_cover, 0 when real art won. */
static uint32_t row_family(const sel_view_row* r, const char* path) {
    const char* src = path != NULL ? path : r->key;
    if (r->format != NULL && r->format[0] != '\0') return sel_platform_of_ext(r->format); /* the engine hands an extension, not a name */
    return sel_platform_of_name(src != NULL ? src : "");
}
static void row_cover(desk* d, sel_gui_rect ar, const sel_view_row* r, const char* path, uint32_t* slot) {
    const char* art = r->art_ref != NULL ? r->art_ref : path != NULL ? path : r->key;
    if (r->art_id != 0u && cover_draw(d, ar, art, 0)) {
        *slot = 0u;
        return;
    }
    family_platform_cover(d, d->canvas, ar, row_family(r, path), r->text, slot);
}

/* Reads a pref. Prefs live in the engine, so they are answered by the engine's plugin host only; the host the UI
 * was opened with is the raw process host, whose config_get always returns the default (I566). */
static const char* desk_pref(const desk* d, const char* key, const char* def) {
    const sel_host* h = d->engine != NULL ? sel_engine_plugin_host(d->engine) : NULL;
    const char* v = h != NULL && h->config_get != NULL ? h->config_get(h, key, def) : def;
    return v != NULL ? v : def;
}

/* The pref ui.desktop.reduce_motion and the format governor together decide whether a tween may start. */
static int desk_motion(const desk* d) {
    const char* v = desk_pref(d, "ui.desktop.reduce_motion", "0");
    return sel_uim_motion_allowed(&d->budget, v != NULL && strcmp(v, "1") == 0);
}

/* Starts a tween when the governor allows it; returns 1 when it runs. */
static int desk_tween(desk* d, uint32_t key, uint32_t ms, uint32_t ease) {
    if (!desk_motion(d) || !sel_gui_tween_start(&d->tl, key, 0, 65536, ms, ease)) {
        return 0;
    }
    d->tweens++;
    return 1;
}


/* ---- Song list (DUI-P3-2): header row, one row per queue entry, drop targets in the sidebar. All geometry in canvas
 * pixels; a pointer position is mapped to a region of the layout tree first (sel_uim_hit), then to a part of it. ---- */
enum { SONG_HEADER_H = 24, SONG_ROW_H = 32, SONG_TARGET_H = 32, SONG_MENU_ITEM_H = 24, SONG_MENU_W = 140, SONG_MENU_ITEMS = 5, SONG_PLAYLIST_TARGETS = 4, SONG_TARGET_W = 160 };

static int desk_send(desk* d, const char* line);
static int desk_pls_import_pick(desk* d);
static int desk_pls_folder_pick(desk* d);
static int desk_send_path(desk* d, const char* verb, const char* path);
static int desk_play_path(desk* d, const char* path);
static const sel_uim_action* desk_action_by_id(const char* id);

static int32_t songs_top(const desk* d) {
    sel_gui_rect mn = region_rect(d, "main");
    int32_t sp = lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_SPACING]);
    return mn.y + sp + lpx(d, 48) + sp;
}

static sel_gui_rect songs_header_col(const desk* d, uint32_t col) {
    sel_gui_rect mn = region_rect(d, "main");
    int32_t split = mn.w * 3 / 4;
    return col == 0u ? rect_of(mn.x, songs_top(d), split, lpx(d, SONG_HEADER_H))
                     : rect_of(mn.x + split, songs_top(d), mn.w - split, lpx(d, SONG_HEADER_H));
}

static sel_gui_rect songs_row_rect(const desk* d, uint32_t row) {
    sel_gui_rect mn = region_rect(d, "main");
    return rect_of(mn.x, songs_top(d) + lpx(d, SONG_HEADER_H) + (int32_t)row * lpx(d, SONG_ROW_H) - lpx_q16(d, d->scroll_q16), mn.w, lpx(d, SONG_ROW_H));
}

/* Drop target 0 = Queue, then the saved playlists: stacked at the right edge of the main pane under the column header,
 * inside the songs content and never over the rail (I448). They are drawn only while a song row is held (songs_drag). */
static sel_gui_rect songs_target_rect(const desk* d, uint32_t t) {
    sel_gui_rect mn = region_rect(d, "main");
    int32_t sp = lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_SPACING]);
    int32_t w = lpx(d, SONG_TARGET_W) < mn.w - 2 * sp ? lpx(d, SONG_TARGET_W) : mn.w - 2 * sp;
    return rect_of(mn.x + mn.w - sp - w, songs_top(d) + lpx(d, SONG_HEADER_H) + sp + (int32_t)t * (lpx(d, SONG_TARGET_H) + sp), w, lpx(d, SONG_TARGET_H));
}

/* The k-th saved playlist of the folders source (the drop targets of the songs view, the rows of the Playlists page):
 * its id and name, or 0 when there is no such playlist. */
static uint32_t playlist_nth(const sel_state* st, uint32_t k, const char** name) {
    uint32_t i;
    if (st->view == NULL || st->view->source != (uint32_t)SEL_VIEW_FOLDERS) {
        return 0u;
    }
    for (i = 0; i < st->view->row_n; i++) {
        if (st->view->rows[i].kind == (uint32_t)SEL_ROW_PLAYLIST && k-- == 0u) {
            *name = st->view->rows[i].text;
            return st->view->rows[i].id;
        }
    }
    return 0u;
}

static int in_rect(sel_gui_rect r, int32_t x, int32_t y) {
    return x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h;
}

static void songs_key(const sel_state* st, uint32_t col, uint32_t i, char* out, size_t cap) {
    if (col == 0u) {
        queue_title(st, i, out, cap);
    } else {
        queue_format(st, i, out, cap);
    }
}

/* Rebuilds the display order from the queue: queue order, then a stable insertion sort by the sort column. */
static void songs_resort(desk* d) {
    const sel_state* st = sel_engine_state(d->engine);
    uint32_t n = st->queue_count < 512u ? st->queue_count : 512u, i, j;
    for (i = 0; i < n; i++) {
        d->order[i] = (uint16_t)i;
    }
    d->order_n = n;
    if (d->sort_dir == 0u) {
        return;
    }
    for (i = 1; i < n; i++) {
        uint16_t v = d->order[i];
        char kv[256], kj[256];
        songs_key(st, d->sort_col, v, kv, sizeof kv);
        for (j = i; j > 0u; j--) {
            int c;
            songs_key(st, d->sort_col, d->order[j - 1u], kj, sizeof kj);
            c = strcmp(kj, kv);
            if (d->sort_dir == 2u ? c >= 0 : c <= 0) {
                break;
            }
            d->order[j] = d->order[j - 1u];
        }
        d->order[j] = v;
    }
}

static void draw_empty_state(desk* d, sel_gui_rect mn, int32_t y, const char* text);

static void draw_songs(desk* d) {
    const sel_state* st = sel_engine_state(d->engine);
    sel_gui_rect mn = region_rect(d, "main");
    const sel_gui_atlas* semi = desk_font(d, 1, 13);
    uint32_t text = ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT]);
    uint32_t i, c;
    int32_t pad = lpx(d, 8);
    if (semi == NULL) {
        return;
    }
    if (d->order_n != st->queue_count) {
        songs_resort(d);
    }
    sel_gui_clip_push(d->canvas, mn);
    sel_gui_fill_rect(d->canvas, rect_of(mn.x, songs_top(d), mn.w, mn.h), ARGB(d->theme.token[SEL_GUI_TOKEN_SURFACE]));
    for (c = 0; c < 2u; c++) {
        static const char* const names[2] = {"Title", "Format"};
        char label[48];
        sel_gui_rect h = songs_header_col(d, c);
        snprintf(label, sizeof label, "%s%s", names[c], d->sort_dir != 0u && d->sort_col == c ? (d->sort_dir == 1u ? " (ascending)" : " (descending)") : "");
        sel_gui_text_wrap(d->canvas, semi, rect_of(h.x + pad, h.y + lpx(d, 4), h.w - 2 * pad, semi->line_height), label,
                          ARGB(d->theme.token[SEL_GUI_TOKEN_ACCENT]), 1, &d->text);
    }
    for (i = 0; i < d->order_n; i++) {
        sel_gui_rect r = songs_row_rect(d, i);
        char name[256], format[16];
        uint32_t q = d->order[i], rink;
        if (r.y >= mn.y + mn.h) {
            break;
        }
        if (r.y < songs_top(d) + lpx(d, SONG_HEADER_H)) { /* scrolled under the header */
            continue;
        }
        if ((int32_t)i == d->row_focus) { /* the keyboard focus row takes the selected-row fill */
            sel_gui_fill_rect(d->canvas, r, ARGB(d->theme.token[SEL_GUI_TOKEN_SURFACE_RAISED]));
        }
        rink = row_ink(d, r, text);
        song_row_reg(d, r, st->queue_text + st->queue[q].path_offset, st->queue[q].id, 1, 0);
        queue_title(st, q, name, sizeof name);
        queue_format(st, q, format, sizeof format);
        sel_gui_text_wrap(d->canvas, semi, rect_of(r.x + pad, r.y + lpx(d, 8), songs_header_col(d, 0).w - 2 * pad, semi->line_height),
                          name, rink, 1, &d->text);
        sel_gui_text_wrap(d->canvas, semi, rect_of(songs_header_col(d, 1).x + pad, r.y + lpx(d, 8), songs_header_col(d, 1).w - 2 * pad, semi->line_height),
                          format, rink, 1, &d->text);
    }
    if (d->order_n == 0u) { /* I552: no songs: the column headers alone read as a broken page, so name the way to fill it */
        draw_empty_state(d, mn, songs_top(d) + lpx(d, SONG_HEADER_H), "No songs to list yet. Add songs from the Library page.");
    }
    sel_gui_clip_pop(d->canvas);
    sel_gui_dirty_add(d->canvas, mn);
}

/* The songs drop targets, drawn over the content while a row is held (I448). */
static void draw_songs_targets(desk* d) {
    const sel_gui_atlas* semi = desk_font(d, 1, 13);
    uint32_t t;
    if (semi == NULL || d->songs_drag < 0) {
        return;
    }
    for (t = 0; t < 1u + SONG_PLAYLIST_TARGETS; t++) {
        sel_gui_rect r = songs_target_rect(d, t);
        const char* label = t == 0u ? "Queue" : "Playlist";
        if (t > 0u && playlist_nth(sel_engine_state(d->engine), t - 1u, &label) == 0u) { /* no such saved playlist */
            if (t > 1u) {
                break;
            }
            label = "Playlist";
        }
        sel_gui_fill_rect(d->canvas, r, ARGB(d->theme.token[SEL_GUI_TOKEN_SURFACE]));
        sel_gui_text_wrap(d->canvas, semi, rect_of(r.x + lpx(d, 8), r.y + lpx(d, 8), r.w - lpx(d, 16), semi->line_height),
                          label, ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT]), 1, &d->text);
        sel_gui_dirty_add(d->canvas, r);
    }
}


enum { SEARCH_FIELD_Y = 16, SEARCH_FIELD_H = 40 };

/* The one rect of the search field, on Home (the header field) and on the Search page alike, inside the main region mn:
 * drawn by both pages, hit by the Home click and ringed by desk_draw_focus. */
enum { TOPBAR_HOME, TOPBAR_FIELD, TOPBAR_BROWSE };

/* I615: a part of the layout's top bar in canvas px; empty when the layout has no bar (the bitmap tier, the mini layout). */
static sel_gui_rect topbar_part(const desk* d, int part) {
    const sel_uim_region* r = part == TOPBAR_HOME ? &d->layout.topbar_home : part == TOPBAR_FIELD ? &d->layout.topbar_field : &d->layout.topbar_browse;
    return rect_of(lpx(d, r->x), lpx(d, r->y), lpx(d, (int32_t)r->w), lpx(d, (int32_t)r->h));
}

static int topbar_shown(const desk* d) {
    return d->layout.topbar.h > 0u;
}

/* I619: the round Hide / Show Now Playing button at the top bar's right end; empty unless the layout has the three columns and the bar. */
static sel_gui_rect np_toggle_rect(const desk* d) {
    sel_gui_rect hm = topbar_part(d, TOPBAR_HOME);
    if (!topbar_shown(d) || d->layout.breakpoint != SEL_UIM_BP_THREE_COLUMN) {
        return rect_of(0, 0, 0, 0);
    }
    return rect_of(lpx(d, (int32_t)d->layout.width) - lpx(d, 16) - hm.w, hm.y, hm.w, hm.h);
}

static sel_gui_rect search_field_rect(const desk* d, sel_gui_rect mn) {
    int32_t mg = lpx(d, HOME_MARGIN);
    if (topbar_shown(d)) { /* I615: the field lives in the top bar on every page */
        return topbar_part(d, TOPBAR_FIELD);
    }
    int32_t sw = mn.w - 2 * mg < lpx(d, HOME_SEARCH_MAX) ? mn.w - 2 * mg : lpx(d, HOME_SEARCH_MAX);
    if (nav_arrows_shown(d, mn) && sw > nav_arrow_rect(d, mn, 0).x - lpx(d, 8) - (mn.x + mg)) { /* I554: ends left of the history arrows, which share its row */
        sw = nav_arrow_rect(d, mn, 0).x - lpx(d, 8) - (mn.x + mg);
    }
    return rect_of(mn.x + mg, mn.y + lpx(d, SEARCH_FIELD_Y), sw, lpx(d, SEARCH_FIELD_H));
}

/* The Home header (Main.dc.html): the tinted band at the top of the main panel, the search field with its icon, the
 * greeting, the filter chips and the section heading. */
/* The empty-field hint, "Search songs, albums, artists": the chip names after the leading All chip, lowercased, so the
 * hint names what the page's own chips search (I366). */
static void search_hint(const desk* d, char* out, size_t cap, const char* const* chips, uint32_t n) {
    size_t o;
    if (topbar_shown(d)) { /* I615: the top bar field has one hint on every page */
        snprintf(out, cap, "What do you want to play?");
        return;
    }
    o = (size_t)snprintf(out, cap, "Search");
    uint32_t k;
    for (k = 1; k < n && o + 2u < cap; k++) {
        const char* p = chips[k];
        o += (size_t)snprintf(out + o, cap - o, "%s ", k == 1u ? "" : ",");
        for (; *p != '\0' && o + 1u < cap; p++) {
            out[o++] = (char)((*p >= 'A' && *p <= 'Z') ? *p + ('a' - 'A') : *p);
        }
        out[o] = '\0';
    }
}

static void desk_draw_query(desk* d, sel_gui_rect sr, const char* hint) {
    sel_gui_rect tb = rect_of(sr.x + lpx(d, 42), sr.y, sr.w - lpx(d, 56), sr.h);
    if (topbar_shown(d)) { /* I615: the Browse button takes the field's right end */
        tb.w -= topbar_part(d, TOPBAR_BROWSE).w;
    }
    d->field = sr; /* the field as last drawn, for the stats line */
    if (d->qlen > 0u) { /* the typed query (I239); a caret follows it while the field has the focus */
        draw_text(d, tb, d->query, 0, 14, ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT]));
    } else {
        draw_text(d, tb, hint, 0, 14, ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT_MUTED]));
    }
    if (d->focus == (int32_t)d->layout.count) { /* the caret: a thin bar at the text end, also in the empty field */
        const sel_gui_atlas* at = desk_font(d, 0, 14);
        int32_t tw = at != NULL ? sel_gui_text_width(at, d->query, d->qlen) : 0;
        int32_t ch = lpx(d, 18), cw = lpx(d, 2) > 0 ? lpx(d, 2) : 1;
        d->caret = rect_of(tb.x + tw, sr.y + (sr.h - ch) / 2, cw, ch);
        sel_gui_fill_rect(d->canvas, d->caret, ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT]));
        d->caret_draws++;
    }
}

/* The Home header chips: All opens Search, Songs the Songs page, Folders Browse Folders, Playlists the
 * Playlists page (desk_click). */
enum { HOME_CHIPS = 4 };
static const char* const home_chip_name[HOME_CHIPS] = {"All", "Songs", "Folders", "Playlists"};

static sel_gui_rect home_chip_rect(desk* d, sel_gui_rect mn, uint32_t k) {
    const sel_gui_atlas* chip = desk_font(d, 1, 13);
    int32_t x = mn.x + lpx(d, HOME_MARGIN), i;
    for (i = 0; i < (int32_t)k && chip != NULL; i++) {
        x += sel_gui_text_width(chip, home_chip_name[i], strlen(home_chip_name[i])) + 2 * lpx(d, 14) + lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_SPACING]);
    }
    return rect_of(x, mn.y + lpx(d, HOME_CHIPS_Y),
                   (chip != NULL ? sel_gui_text_width(chip, home_chip_name[k], strlen(home_chip_name[k])) : 40) + 2 * lpx(d, 14), lpx(d, HOME_CHIP_H));
}

static void draw_home_header(desk* d, sel_gui_rect mn, int with_section) {
    uint32_t text = ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT]);
    uint32_t muted = ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT_MUTED]);
    int32_t rad = lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_RADIUS_LARGE]);
    int32_t mg = lpx(d, HOME_MARGIN);
    sel_gui_rect sr = search_field_rect(d, mn);
    const sel_gui_atlas* chip = desk_font(d, 1, 13);
    int32_t k, ic = lpx(d, 18);
    char hint[64];
    if (d->canvas->copper_pens > 0u) {
        /* I628: with a copper the tint is one copper bar down the whole page: it costs one pen however tall, and it
         * stays put while the shelves scroll (the main region, not the scrolled header rect), so its key is stable. */
        sel_gui_rect pg = region_rect(d, "main");
        sel_gui_round_rect(d->canvas, rect_of(pg.x, pg.y, pg.w, 2 * rad), rad, ARGB(d->theme.token[SEL_GUI_TOKEN_HEADER_TINT]));
        sel_gui_gradient(d->canvas, rect_of(pg.x, pg.y + rad, pg.w, pg.h - rad), 1, ARGB(d->theme.token[SEL_GUI_TOKEN_HEADER_TINT]),
                         ARGB(d->theme.token[SEL_GUI_TOKEN_PANEL]));
    } else {
        sel_gui_round_rect(d->canvas, rect_of(mn.x, mn.y, mn.w, 2 * rad), rad, ARGB(d->theme.token[SEL_GUI_TOKEN_HEADER_TINT]));
        sel_gui_gradient(d->canvas, rect_of(mn.x, mn.y + rad, mn.w, lpx(d, HOME_TINT_H) - rad), 1,
                         ARGB(d->theme.token[SEL_GUI_TOKEN_HEADER_TINT]), ARGB(d->theme.token[SEL_GUI_TOKEN_PANEL]));
    }
    d->home_parts |= HP_HEADER_TINT;
    if (!topbar_shown(d)) { /* I615: with a top bar the field is drawn there */
        sel_gui_round_rect(d->canvas, sr, rad, ARGB(d->theme.token[SEL_GUI_TOKEN_SURFACE]));
        sel_gui_icon_draw(d->canvas, &d->icons, SEL_GUI_ICON_SEARCH, rect_of(sr.x + lpx(d, 14), sr.y + (sr.h - ic) / 2, ic, ic), ic,
                          muted);
        d->home_parts |= HP_SEARCH_ICON;
        search_hint(d, hint, sizeof hint, home_chip_name, HOME_CHIPS);
        desk_draw_query(d, sr, hint);
    }
    draw_text(d, rect_of(mn.x + mg, mn.y + lpx(d, HOME_GREET_Y), mn.w - 2 * mg, lpx(d, HOME_GREET_H)), "Welcome back", 1, 32, text);
    d->home_parts |= HP_GREETING;
    for (k = 0; k < HOME_CHIPS && chip != NULL; k++) {
        sel_gui_rect cr = home_chip_rect(d, mn, (uint32_t)k);
        draw_chip(d, cr, home_chip_name[k], k == 0, SEL_GUI_TOKEN_SURFACE_RAISED);
    }
    d->home_parts |= HP_CHIPS;
    if (with_section) {
        draw_text(d, rect_of(mn.x + mg, mn.y + lpx(d, HOME_SECTION_Y), mn.w - 2 * mg, lpx(d, HOME_SECTION_H)), "Recently played", 1, 22, text);
        d->home_parts |= HP_SECTION;
    }
}

/* ---- Home shelves (DUI-P4-2): the HOME view source (engine) as rows of tiles. A heading row starts a shelf; the
 * song and album rows after it are its tiles; a row without a key (the "new this week" count line) is plain text.
 * Each shelf scrolls horizontally on its own. ---- */
enum { NAV_PREV = 1, NAV_NEXT = 2, HOME_NAV_GROW_Q16 = 6554 /* the hovered button grows to about 1.1x (I630) */ }; /* I607: the shelf buttons, bits of shelf_nav_drawn */
enum { SHELF_MAX = 6, SHELF_HEAD_H = HOME_SECTION_H + HOME_SHADOW_R + HOME_HEAD_GAP, SHELF_GAP = 16, SHELF_WINDOW = 32, SHELF_NONE = 0x7FFFFFFF };

typedef struct shelf {
    uint32_t head;  /* index of the heading in the view window */
    uint32_t first; /* index of the first row after it */
    uint32_t tiles; /* rows with a key */
    uint32_t rows;  /* rows after the heading */
} shelf;

/* Splits the HOME window into shelves; 0 when the view is not the HOME source. A heading with no row under it is not a
 * shelf (I311): it draws nothing and the shelves below move up. */
static uint32_t home_shelves(const sel_state* st, shelf* out) {
    const sel_view_state* v = st->view;
    uint32_t i, k, n = 0;
    int in_quick = 0;
    if (v == NULL || v->source != SEL_VIEW_HOME) {
        return 0;
    }
    for (i = 0; i < v->row_n; i++) {
        if (v->rows[i].kind == SEL_ROW_HEADING) {
            in_quick = (int)v->rows[i].quick; /* I478: the quick-pick grid is not a shelf; home_quick reads it */
            if (in_quick) {
                continue;
            }
            if (n == SHELF_MAX) {
                break;
            }
            out[n].head = i;
            out[n].first = i + 1u;
            out[n].tiles = 0;
            out[n].rows = 0;
            n++;
        } else if (n > 0u && !in_quick) {
            out[n - 1u].rows++;
            out[n - 1u].tiles += v->rows[i].key != NULL;
        }
    }
    for (i = 0, k = 0; i < n; i++) {
        if (out[i].rows > 0u) {
            out[k++] = out[i];
        }
    }
    return k;
}

/* 1 when the HOME source holds at least one tile: the shelves replace the queue grid. */
static int home_shelf_mode(const sel_state* st);

/* 1 when Home has nothing to show: the HOME source holds no tile (an empty library still lists its shelf headings)
 * and no song is queued. The view stays on Home and draws the empty state (one line and a Browse files action). */
static int home_empty(const sel_state* st) {
    return st->view != NULL && st->view->source == SEL_VIEW_HOME && !home_shelf_mode(st) && st->queue_count == 0u;
}

/* The Browse files action of the empty Home state. */
static sel_gui_rect home_empty_button(desk* d) {
    sel_gui_rect mn = region_rect(d, "main");
    return rect_of(mn.x + lpx(d, HOME_MARGIN), mn.y + lpx(d, HOME_SECTION_Y + 40), lpx(d, 140), lpx(d, HOME_CHIP_H));
}

/* 1 when the HOME source holds at least one tile: the shelves replace the queue grid. */
static int home_shelf_mode(const sel_state* st) {
    shelf sh[SHELF_MAX];
    uint32_t i, n = home_shelves(st, sh);
    for (i = 0; i < n; i++) {
        if (sh[i].tiles > 0u) {
            return 1;
        }
    }
    return 0;
}

/* Window row of tile k of a shelf (keyed rows only), or UINT32_MAX. */
static uint32_t shelf_tile_row(const sel_state* st, const shelf* sh, uint32_t k) {
    uint32_t i;
    for (i = sh->first; i < sh->first + sh->rows; i++) {
        if (st->view->rows[i].key != NULL) {
            if (k == 0u) {
                return i;
            }
            k--;
        }
    }
    return 0xFFFFFFFFu;
}

/* ---- I478: the Spotify-style quick-pick grid at the top of Home: compact horizontal tiles (art left, name right) of
 * the most recent albums. The engine marks its rows with a Quick picks heading (row.quick); a tile opens through the
 * same activation path as a shelf tile. 4 columns when each cell keeps QUICK_MIN_W, else 2 columns. ---- */
enum { QUICK_ART = 56, QUICK_MIN_W = 160, QUICK_COLS_WIDE = 4, QUICK_COLS_NARROW = 2 };

/* Fills q with the quick-pick rows (first, rows, tiles); returns the tile count, 0 when Home has no grid. */
static uint32_t home_quick(const sel_state* st, shelf* q) {
    const sel_view_state* v = st->view;
    uint32_t i;
    if (v == NULL || v->source != SEL_VIEW_HOME) {
        return 0;
    }
    for (i = 0; i < v->row_n; i++) {
        if (v->rows[i].kind == SEL_ROW_HEADING && v->rows[i].quick) {
            q->head = i;
            q->first = i + 1u;
            q->rows = 0;
            q->tiles = 0;
            while (q->first + q->rows < v->row_n && v->rows[q->first + q->rows].kind != SEL_ROW_HEADING) {
                q->tiles += v->rows[q->first + q->rows].key != NULL;
                q->rows++;
            }
            return q->tiles;
        }
    }
    return 0;
}

static uint32_t quick_cols(desk* d) {
    sel_gui_rect mn = region_rect(d, "main");
    int32_t gap = lpx(d, HOME_TILE_GAP) * 2;
    int32_t cell = (mn.w - 2 * lpx(d, HOME_MARGIN) - 3 * gap) / 4;
    return cell >= lpx(d, QUICK_MIN_W) ? QUICK_COLS_WIDE : QUICK_COLS_NARROW;
}

/* Height of the grid plus the gap under it in canvas pixels, 0 without a grid. */
static int32_t quick_h(desk* d) {
    shelf q;
    uint32_t tiles = home_quick(sel_engine_state(d->engine), &q), cols = quick_cols(d), rows;
    if (tiles == 0u) {
        return 0;
    }
    rows = (tiles + cols - 1u) / cols;
    return (int32_t)rows * (lpx(d, QUICK_ART) + lpx(d, HOME_TILE_GAP) * 2) + lpx(d, SHELF_GAP);
}

static sel_gui_rect quick_tile_rect(desk* d, uint32_t k) {
    sel_gui_rect mn = region_rect(d, "main");
    uint32_t cols = quick_cols(d);
    int32_t gap = lpx(d, HOME_TILE_GAP) * 2;
    int32_t w = (mn.w - 2 * lpx(d, HOME_MARGIN) - ((int32_t)cols - 1) * gap) / (int32_t)cols;
    return rect_of(mn.x + lpx(d, HOME_MARGIN) + (int32_t)(k % cols) * (w + gap),
                   mn.y - lpx(d, d->home_y_q16 >> 16) + lpx(d, HOME_SECTION_Y) + (int32_t)(k / cols) * (lpx(d, QUICK_ART) + gap), w,
                   lpx(d, QUICK_ART));
}

/* The art keys of what Home shows: the queue paths of the tile grid, or the song keys of the shelf tiles (shelf mode).
 * Returns how many were written to out[0..cap). */
static uint32_t art_keys(const sel_state* st, const char** out, uint32_t cap) {
    uint32_t i, n = 0;
    if (home_shelf_mode(st)) {
        shelf sh[SHELF_MAX], qs;
        uint32_t s, k, ns = home_shelves(st, sh);
        for (k = 0; k < home_quick(st, &qs) && n < cap; k++) {
            const char* key = st->view->rows[shelf_tile_row(st, &qs, k)].art_ref;
            if (key != NULL) {
                out[n++] = key;
            }
        }
        for (s = 0; s < ns; s++) {
            for (k = 0; k < sh[s].tiles && n < cap; k++) {
                const char* key = st->view->rows[shelf_tile_row(st, &sh[s], k)].art_ref;
                if (key != NULL) {
                    out[n++] = key;
                }
            }
        }
        return n;
    }
    for (i = 0; i < st->queue_count && n < cap; i++) {
        out[n++] = st->queue_text + st->queue[i].path_offset;
    }
    return n;
}

static int32_t shelf_card_h(desk* d) {
    int32_t pad = lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_TILE_PAD]);
    return pad + lpx(d, sel_uim_tile_step(d->tile_step)) + lpx(d, HOME_TILE_TEXT_GAP + HOME_TITLE_H + HOME_META_H) + pad;
}

/* Whole Home content height in logical pixels: the header, then every shelf. */
static int32_t home_content_h(desk* d, uint32_t shelves) {
    return HOME_SECTION_Y + quick_h(d) / (lpx(d, 1) > 0 ? lpx(d, 1) : 1) + (int32_t)shelves * (SHELF_HEAD_H + SHELF_GAP + shelf_card_h(d) / (lpx(d, 1) > 0 ? lpx(d, 1) : 1));
}

static int shelf_page_on(const desk* d);
static uint32_t shelf_page_cols(desk* d);

/* The vertical scroll limit in Q16 logical pixels: the content beyond the main panel height, 0 when it fits. */
static int32_t home_max_y_q16(desk* d) {
    shelf sh[SHELF_MAX];
    uint32_t n = home_shelves(sel_engine_state(d->engine), sh);
    int32_t unit = lpx(d, 1) > 0 ? lpx(d, 1) : 1;
    int32_t over = home_content_h(d, n) - region_rect(d, "main").h / unit;
    if (shelf_page_on(d) && d->shelf_open <= n) {
        uint32_t cols = shelf_page_cols(d), rows = (sh[d->shelf_open - 1u].tiles + cols - 1u) / cols;
        over = HOME_SECTION_Y + SHELF_HEAD_H + (int32_t)rows * (SHELF_GAP + shelf_card_h(d) / unit) - region_rect(d, "main").h / unit;
    }
    return over > 0 ? over << 16 : 0;
}

/* I617: the Shelf page shows one shelf of Home as a wrapped grid of all its tiles. */
static int shelf_page_on(const desk* d) {
    return d->page == PG_SHELF && d->shelf_open > 0u;
}

/* I617: tiles per grid row on the Shelf page. */
static uint32_t shelf_page_cols(desk* d) {
    int32_t pad = lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_TILE_PAD]);
    sel_gui_rect mn = region_rect(d, "main");
    int32_t cw = lpx(d, sel_uim_tile_step(d->tile_step)) + 2 * pad, gap = lpx(d, HOME_TILE_GAP);
    int32_t c = (mn.w - 2 * lpx(d, HOME_MARGIN) + 2 * pad + gap) / (cw + gap);
    return c > 0 ? (uint32_t)c : 1u;
}

static int32_t shelf_top(desk* d, uint32_t s) {
    sel_gui_rect mn = region_rect(d, "main");
    if (shelf_page_on(d)) { /* only the open shelf has a place, right under the page top */
        return s + 1u == d->shelf_open ? mn.y - lpx(d, d->home_y_q16 >> 16) + lpx(d, HOME_SECTION_Y) : mn.y - (1 << 20);
    }
    return mn.y - lpx(d, d->home_y_q16 >> 16) + lpx(d, HOME_SECTION_Y) + quick_h(d) + (int32_t)s * (lpx(d, SHELF_HEAD_H + SHELF_GAP) + shelf_card_h(d));
}

/* I630: the scroll of shelf s as painted: a button click eases it from where it was to shelf_q16[s], the target. */
static int32_t shelf_pos_q16(const desk* d, uint32_t s) {
    int32_t v = s == d->nav_scroll_shelf ? sel_gui_tween_value(&d->tl, ANIM_KEY_NAV_SCROLL, 65536) : 65536;
    return v >= 65536 ? d->shelf_q16[s] : d->nav_scroll_from + (int32_t)(((int64_t)(d->shelf_q16[s] - d->nav_scroll_from) * v) >> 16);
}

/* The scroll limit of a shelf in Q16 logical pixels: the tiles' width beyond the visible width. */
static int32_t shelf_max_q16(desk* d, const shelf* sh) {
    int32_t pad = lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_TILE_PAD]);
    sel_gui_rect mn = region_rect(d, "main");
    int32_t cw = lpx(d, sel_uim_tile_step(d->tile_step)) + 2 * pad + lpx(d, HOME_TILE_GAP);
    int32_t total = (int32_t)sh->tiles * cw;
    int32_t vis = mn.w - 2 * lpx(d, HOME_MARGIN) + 2 * pad;
    int32_t unit = lpx(d, 1) > 0 ? lpx(d, 1) : 1;
    return total > vis ? ((total - vis) / unit) << 16 : 0;
}

static sel_gui_rect shelf_tile_rect(desk* d, uint32_t s, uint32_t k) {
    int32_t pad = lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_TILE_PAD]);
    sel_gui_rect mn = region_rect(d, "main");
    int32_t cw = lpx(d, sel_uim_tile_step(d->tile_step)) + 2 * pad;
    if (shelf_page_on(d)) {
        uint32_t cols = shelf_page_cols(d);
        return rect_of(mn.x + lpx(d, HOME_MARGIN) - pad + (int32_t)(k % cols) * (cw + lpx(d, HOME_TILE_GAP)),
                       shelf_top(d, s) + lpx(d, SHELF_HEAD_H) - pad + (int32_t)(k / cols) * (shelf_card_h(d) + lpx(d, SHELF_GAP)), cw, shelf_card_h(d));
    }
    return rect_of(mn.x + lpx(d, HOME_MARGIN) - pad + (int32_t)k * (cw + lpx(d, HOME_TILE_GAP)) - lpx(d, shelf_pos_q16(d, s) >> 16),
                   shelf_top(d, s) + lpx(d, SHELF_HEAD_H) - pad, cw, shelf_card_h(d));
}

static sel_gui_rect shelf_nav_rect(desk* d, uint32_t s, int next);
static int32_t nav_fade_now(const desk* d);
static int32_t nav_scale_now(const desk* d, uint32_t dir);

/* Writes "shelf S tile K heading|key|title" lines in order into home_dump; returns the shelf count. */
static uintptr_t home_dump_build(desk* d) {
    const sel_state* st = sel_engine_state(d->engine);
    shelf sh[SHELF_MAX];
    uint32_t s, k, n = home_shelves(st, sh);
    size_t o = 0;
    shelf qs;
    uint32_t qn = home_quick(st, &qs);
    d->home_dump[0] = '\0';
    for (k = 0; k < qn && o + 200u < sizeof d->home_dump; k++) { /* I478: "quick K cols=C key|title|artist" */
        const sel_view_row* r = &st->view->rows[shelf_tile_row(st, &qs, k)];
        o += (size_t)snprintf(d->home_dump + o, sizeof d->home_dump - o, "quick %u cols=%u %s|%s|%s\n", (unsigned)k, (unsigned)quick_cols(d),
                              r->key, r->text, r->artist != NULL ? r->artist : "");
    }
    if (d->quick_hover_fill != 0u && o + 64u < sizeof d->home_dump) { /* I490: "quick_hover fill=AARRGGBB", present only while a quick tile is hovered */
        o += (size_t)snprintf(d->home_dump + o, sizeof d->home_dump - o, "quick_hover fill=%08x\n", (unsigned)d->quick_hover_fill);
    }
    if (d->art_probe.w > 0 && o + 160u < sizeof d->home_dump) { /* I477: "tile_art corner=RRGGBB centre=RRGGBB fill=RRGGBB": the art's rounded corner shows the card, its middle the cover */
        o += (size_t)snprintf(d->home_dump + o, sizeof d->home_dump - o, "tile_art corner=%06x centre=%06x fill=%06x\n",
                              (unsigned)(sel_gui_pixel(d->canvas, d->art_probe.x, d->art_probe.y) & 0xFFFFFFu),
                              (unsigned)(sel_gui_pixel(d->canvas, d->art_probe.x + d->art_probe.w / 2, d->art_probe.y + d->art_probe.h / 2) & 0xFFFFFFu),
                              (unsigned)(d->art_probe_fill & 0xFFFFFFu));
        o += (size_t)snprintf(d->home_dump + o, sizeof d->home_dump - o, "tile_art_rect %d %d %d %d\n", (int)d->art_probe.x, (int)d->art_probe.y, (int)d->art_probe.w, (int)d->art_probe.h);
    }
    if (d->canvas->copper_pens > 0u && o + 64u < sizeof d->home_dump) { /* I628: "copper pens=P keys=K miss=M changes=C", then "copper_key pen ry rh y0 y1" per key */
        static sel_gui_copper_change ch[600];
        uint32_t i;
        o += (size_t)snprintf(d->home_dump + o, sizeof d->home_dump - o, "copper pens=%u keys=%u miss=%u changes=%u\n", (unsigned)d->canvas->copper_pens,
                              (unsigned)d->canvas->copper_n, (unsigned)d->canvas->copper_miss, (unsigned)sel_gui_copper_changes(d->canvas, ch, 600u));
        for (i = 0; i < d->canvas->copper_n && o + 64u < sizeof d->home_dump; i++) {
            const struct sel_gui_copper_key* ck = &d->canvas->copper_key[i];
            o += (size_t)snprintf(d->home_dump + o, sizeof d->home_dump - o, "copper_key %u %d %d %d %d\n", (unsigned)ck->pen, (int)ck->ry, (int)ck->rh,
                                  (int)ck->y0, (int)ck->y1);
        }
    }
    for (s = 0; s < n; s++) {
        if (s < 8u && o + 80u < sizeof d->home_dump) { /* I607: "shelf_nav S prev=P next=N" is what the last paint drew */
            sel_gui_rect pr = shelf_nav_rect(d, s, 0), nr = shelf_nav_rect(d, s, 1);
            o += (size_t)snprintf(d->home_dump + o, sizeof d->home_dump - o, "shelf_nav %u prev=%u next=%u prev_at=%d,%d next_at=%d,%d labels=Previous,Next alpha=%d scale_prev=%d scale_next=%d\n",
                                  (unsigned)s, (unsigned)(d->shelf_nav_drawn[s] & NAV_PREV ? 1u : 0u), (unsigned)(d->shelf_nav_drawn[s] & NAV_NEXT ? 1u : 0u),
                                  (int)(pr.x + pr.w / 2), (int)(pr.y + pr.h / 2), (int)(nr.x + nr.w / 2), (int)(nr.y + nr.h / 2), (int)(d->nav_shelf1 == s + 1u ? nav_fade_now(d) : 0), (int)nav_scale_now(d, 0), (int)nav_scale_now(d, 1));
        }
        for (k = 0; k < sh[s].tiles && o + 200u < sizeof d->home_dump; k++) {
            const sel_view_row* r = &st->view->rows[shelf_tile_row(st, &sh[s], k)];
            o += (size_t)snprintf(d->home_dump + o, sizeof d->home_dump - o, "shelf %u tile %u %s|%s|%s|%s\n", (unsigned)s, (unsigned)k,
                                  st->view->rows[sh[s].head].text, r->key, r->text, r->artist != NULL ? r->artist : "");
        }
    }
    return (uintptr_t)n;
}

/* The shelf and tile under a canvas point, or 0. */
static int shelf_hit(desk* d, int32_t x, int32_t y, uint32_t* ps, uint32_t* pk) {
    const sel_state* st = sel_engine_state(d->engine);
    sel_gui_rect mn = region_rect(d, "main");
    shelf sh[SHELF_MAX];
    uint32_t s, k, n = home_shelves(st, sh);
    for (s = 0; s < n; s++) {
        for (k = 0; k < sh[s].tiles; k++) {
            sel_gui_rect t = shelf_tile_rect(d, s, k);
            if (x >= t.x && x < t.x + t.w && y >= t.y && y < t.y + t.h && x >= mn.x && x < mn.x + mn.w && y >= mn.y && y < mn.y + mn.h) {
                *ps = s;
                *pk = k;
                return 1;
            }
        }
    }
    return 0;
}

/* The quick-pick tile under a canvas point, or 0. */
static int quick_hit(desk* d, int32_t x, int32_t y, uint32_t* pk) {
    shelf q;
    uint32_t k, n = shelf_page_on(d) ? 0u : home_quick(sel_engine_state(d->engine), &q);
    sel_gui_rect mn = region_rect(d, "main");
    for (k = 0; k < n; k++) {
        sel_gui_rect t = quick_tile_rect(d, k);
        if (x >= t.x && x < t.x + t.w && y >= t.y && y < t.y + t.h && x >= mn.x && x < mn.x + mn.w) {
            *pk = k;
            return 1;
        }
    }
    return 0;
}

/* The flat hover index of quick-pick tile k (I490): the quick tiles follow the shelf tiles in d->hover_tile's numbering, so the shelf indices stay as they were. */
static uint32_t quick_hover_index(const sel_state* st, uint32_t k) {
    shelf sh[SHELF_MAX];
    uint32_t s, cnt = home_shelves(st, sh), flat = 0;
    for (s = 0; s < cnt; s++) {
        flat += sh[s].tiles;
    }
    return flat + k;
}

/* The shelf whose cover row holds the canvas row y, or SHELF_NONE (I630). The cover row is the art row the round buttons sit
 * on: not the shelf title above it and not the title and artist lines under it. */
static uint32_t shelf_at_y(desk* d, int32_t y) {
    shelf sh[SHELF_MAX];
    uint32_t s, n = home_shelves(sel_engine_state(d->engine), sh);
    for (s = 0; s < n; s++) {
        int32_t top = shelf_top(d, s);
        int32_t art_y = top + lpx(d, SHELF_HEAD_H);
        if (y >= art_y && y < art_y + lpx(d, sel_uim_tile_step(d->tile_step))) {
            return s;
        }
    }
    return SHELF_NONE;
}

/* Activates a shelf tile through the engine's own HOME path: the cursor goes to its row and VIEW_ACTIVATE plays a
 * song or opens an album search; the HOME source is then asked for again so the shelves follow the new history. */
static int desk_page_goto(desk* d, desk_page pg);
static int desk_route(desk* d, sel_route_target target, uint32_t item_kind, uint32_t id, const char* key, const char* name, const char* sub);
static int desk_route_door(desk* d, desk_page pg);
static void desk_composer_open(desk* d, const char* canon, const char* display);

static void shelf_activate_row(desk* d, uint32_t row);

static void shelf_activate(desk* d, uint32_t s, uint32_t k) {
    const sel_state* st = sel_engine_state(d->engine);
    shelf sh[SHELF_MAX];
    if (home_shelves(st, sh) <= s) {
        return;
    }
    shelf_activate_row(d, shelf_tile_row(st, &sh[s], k));
}

/* I478: a quick-pick tile opens through the same row activation as a shelf tile. */
static void quick_activate(desk* d, uint32_t k) {
    const sel_state* st = sel_engine_state(d->engine);
    shelf q;
    if (home_quick(st, &q) > k) {
        shelf_activate_row(d, shelf_tile_row(st, &q, k));
    }
}

static void shelf_activate_row(desk* d, uint32_t row) {
    const sel_state* st = sel_engine_state(d->engine);
    char line[48];
    if (row == 0xFFFFFFFFu) {
        return;
    }
    if (st->view->rows[row].composer) { /* I479: a composer tile opens that composer's page */
        char canon[SEL_COMPOSER_MAX]; /* copied first: the page change replaces the view */
        snprintf(canon, sizeof canon, "%s", st->view->rows[row].key);
        desk_route(d, SEL_ROUTE_HOME_TILE, (uint32_t)SEL_LIB_ITEM_COMPOSER, 0u, canon, canon, ""); /* I678f/I678h2: the composer's collection page */
        d->shelf_plays++;
        return;
    }
    if (st->view->rows[row].album && st->view->rows[row].art_id != 0u) { /* I678h2: an album tile opens the album page by the id its row carries */
        char key[16], name[96], sub[96];
        uint32_t id = st->view->rows[row].art_id; /* copied first: the page change replaces the view */
        snprintf(key, sizeof key, "%u", (unsigned)id);
        snprintf(name, sizeof name, "%s", st->view->rows[row].key != NULL ? st->view->rows[row].key : "");
        snprintf(sub, sizeof sub, "%s", st->view->rows[row].artist != NULL ? st->view->rows[row].artist : "");
        desk_route(d, SEL_ROUTE_HOME_TILE, (uint32_t)SEL_LIB_ITEM_ALBUM, id, key, name, sub);
        d->shelf_plays++;
        return;
    }
    snprintf(line, sizeof line, "VIEW_CURSOR %u", (unsigned)(st->view->window_start + row));
    desk_send(d, line);
    desk_send(d, "VIEW_ACTIVATE");
    desk_send(d, "VIEW_SOURCE home");
    d->shelf_plays++;
}

/* Scrolls the whole Home content by wheel notches (positive = toward the end), clamped to the overflow. */
static void home_scroll_y(desk* d, int32_t notches_q16) {
    int64_t v = (int64_t)d->home_y_q16 + (int64_t)notches_q16 * SCROLL_NOTCH;
    int32_t max = home_max_y_q16(d);
    d->home_y_q16 = v < 0 ? 0 : (v > max ? max : (int32_t)v);
}

/* I607: Spotify-style round previous/next buttons at the ends of a shelf. They show only while the pointer is over that
 * shelf, and a button whose direction has nothing more to show (previous at the start, next at the end) stays hidden.
 * The wheel never scrolls a shelf sideways; a click on a button moves it by one page of covers. The round fill and the
 * HOME_PLAY_SIZE diameter are the tile play button's; the chevron is a baked icon in the theme text colour. */
/* The button rect of a shelf end: centred on the cover art vertically, inset by the Home margin. */
static sel_gui_rect shelf_nav_rect(desk* d, uint32_t s, int next) {
    sel_gui_rect mn = region_rect(d, "main");
    int32_t ps = lpx(d, HOME_PLAY_SIZE);
    int32_t y = shelf_top(d, s) + lpx(d, SHELF_HEAD_H) + (lpx(d, sel_uim_tile_step(d->tile_step)) - ps) / 2;
    int32_t x = next ? mn.x + mn.w - lpx(d, HOME_MARGIN) - ps : mn.x + lpx(d, HOME_MARGIN);
    return rect_of(x, y, ps, ps);
}

/* I630: a tween value between its two ends, Q16. */
static int32_t nav_mix(const desk* d, uint32_t key, int32_t from, int32_t to) {
    return from + (int32_t)(((int64_t)(to - from) * sel_gui_tween_value(&d->tl, key, 65536)) >> 16);
}

static int32_t nav_fade_now(const desk* d) {
    return nav_mix(d, ANIM_KEY_NAV_FADE, d->nav_fade_from, d->nav_fade_to);
}

/* I630: a Q16 opacity as the 0-255 alpha byte of a colour. Full opacity is 65536, so a plain >> 8 gives 256 and wraps to 0
 * once shifted into the top byte: the buttons would vanish the moment their fade-in ended. */
static uint32_t nav_alpha(int32_t fade) {
    return fade >= 65536 ? 255u : fade <= 0 ? 0u : (uint32_t)fade >> 8;
}

static int32_t nav_scale_now(const desk* d, uint32_t dir) {
    return nav_mix(d, ANIM_KEY_NAV_SCALE + dir, d->nav_scale_from[dir], d->nav_scale_to[dir]);
}

/* True while any shelf button tween (fade, grow, click scroll) runs: the main region repaints each step. */
static int nav_animating(const desk* d) {
    return sel_gui_tween_value(&d->tl, ANIM_KEY_NAV_FADE, 65536) < 65536 || sel_gui_tween_value(&d->tl, ANIM_KEY_NAV_SCROLL, 65536) < 65536 ||
           sel_gui_tween_value(&d->tl, ANIM_KEY_NAV_SCALE, 65536) < 65536 || sel_gui_tween_value(&d->tl, ANIM_KEY_NAV_SCALE + 1, 65536) < 65536;
}

/* The buttons shelf s has by its scroll position (NAV_* bits): previous unless at the start, next unless at the end. */
static uint32_t shelf_nav_bits(desk* d, uint32_t s, const shelf* sh) {
    int32_t max = shelf_max_q16(d, sh);
    uint32_t bits = 0u;
    bits |= d->shelf_q16[s] > 0 ? NAV_PREV : 0u;
    bits |= d->shelf_q16[s] < max ? NAV_NEXT : 0u;
    return bits;
}

/* The buttons shelf s shows now (NAV_* bits). */
static uint32_t shelf_nav_visible(desk* d, uint32_t s, const shelf* sh) {
    if (s >= 8u || d->hover_shelf1 != s + 1u) {
        return 0u;
    }
    return shelf_nav_bits(d, s, sh);
}

/* I630: starts a tween from the current value to a new target; a no-op while the target is unchanged. Without motion the
 * tween does not start and the value is the target at once. Returns 1 when the target changed. */
static int nav_retarget(desk* d, uint32_t key, int32_t* from, int32_t* to, int32_t now, int32_t target) {
    if (*to == target) {
        return 0;
    }
    *from = now;
    *to = target;
    desk_tween(d, key, SEL_GUI_ANIM_FAST_MS, SEL_GUI_EASE_OUT);
    return 1;
}

/* I630: the pointer is over canvas point (x, y) and shelf1 (1-based, 0 = none) is the shelf under it: fades the buttons of
 * that shelf in and the last shelf's out, and grows the button under the pointer while shrinking the other. Returns 1 when a
 * target changed. d->hover_shelf1 already equals shelf1. */
static int shelf_nav_hover(desk* d, int32_t x, int32_t y, uint32_t shelf1) {
    shelf sh[SHELF_MAX];
    uint32_t dir, bits = 0u;
    int changed = 0;
    if (shelf1 != 0u && d->nav_shelf1 != shelf1) { /* another shelf: its buttons start from nothing, the old shelf's are gone */
        d->nav_shelf1 = shelf1;
        d->nav_fade_from = d->nav_fade_to = 0;
        d->nav_scale_from[0] = d->nav_scale_to[0] = d->nav_scale_from[1] = d->nav_scale_to[1] = 65536;
        changed = 1;
    }
    if (shelf1 != 0u && home_shelves(sel_engine_state(d->engine), sh) >= shelf1) {
        bits = shelf_nav_visible(d, shelf1 - 1u, &sh[shelf1 - 1u]);
    }
    changed |= nav_retarget(d, ANIM_KEY_NAV_FADE, &d->nav_fade_from, &d->nav_fade_to, nav_fade_now(d), shelf1 != 0u ? 65536 : 0);
    for (dir = 0; dir < 2u; dir++) {
        int hot = shelf1 != 0u && (bits & (dir ? NAV_NEXT : NAV_PREV)) && in_rect(shelf_nav_rect(d, shelf1 - 1u, (int)dir), x, y);
        changed |= nav_retarget(d, ANIM_KEY_NAV_SCALE + dir, &d->nav_scale_from[dir], &d->nav_scale_to[dir], nav_scale_now(d, dir),
                                hot ? 65536 + HOME_NAV_GROW_Q16 : 65536);
    }
    return changed;
}

/* One page of covers in Q16 logical pixels: as many whole covers as fit the visible width, at least one. */
static int32_t shelf_page_q16(desk* d) {
    int32_t pad = lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_TILE_PAD]);
    sel_gui_rect mn = region_rect(d, "main");
    int32_t cw = lpx(d, sel_uim_tile_step(d->tile_step)) + 2 * pad + lpx(d, HOME_TILE_GAP);
    int32_t vis = mn.w - 2 * lpx(d, HOME_MARGIN) + 2 * pad;
    int32_t unit = lpx(d, 1) > 0 ? lpx(d, 1) : 1;
    int32_t n = cw > 0 ? vis / cw : 1;
    return ((n < 1 ? cw : n * cw) / unit) << 16;
}

/* The shelf (1-based) under canvas point (x, y), else 0. */
static uint32_t shelf_hover_of(desk* d, int32_t x, int32_t y) {
    uint32_t s = shelf_at_y(d, y);
    return s != SHELF_NONE && s < 8u && in_rect(region_rect(d, "main"), x, y) ? s + 1u : 0u;
}

/* I630: the one place that decides which shelf owns the buttons. The pointer is at canvas point (x, y): hover_shelf1 (what the
 * click hits) and the fade/grow targets (what is drawn) are set together, from the cover row under the pointer. Called by
 * pointer moves and by the wheel, which moves the content under a still pointer. Returns 1 when a target changed. */
static int shelf_nav_pointer(desk* d, int32_t x, int32_t y, uint32_t shelf1) {
    int changed = shelf1 != d->hover_shelf1;
    d->hover_shelf1 = shelf1;
    if (d->page != PG_HOME || !d->home) { /* no shelves on this page: no button tween, so a Library drag repaints only its own band */
        return changed;
    }
    return shelf_nav_hover(d, x, y, shelf1) | changed;
}

/* A click on a visible button moves its shelf by one page. Returns 1 when the click hit a button. */
static int shelf_nav_click(desk* d, int32_t x, int32_t y) {
    shelf sh[SHELF_MAX];
    uint32_t s = d->hover_shelf1 - 1u, bits;
    int32_t v, max;
    if (shelf_page_on(d) || d->hover_shelf1 == 0u || home_shelves(sel_engine_state(d->engine), sh) <= s) {
        return 0;
    }
    bits = shelf_nav_visible(d, s, &sh[s]);
    max = shelf_max_q16(d, &sh[s]);
    if ((bits & NAV_PREV) && in_rect(shelf_nav_rect(d, s, 0), x, y)) {
        v = d->shelf_q16[s] - shelf_page_q16(d);
    } else if ((bits & NAV_NEXT) && in_rect(shelf_nav_rect(d, s, 1), x, y)) {
        v = d->shelf_q16[s] + shelf_page_q16(d);
    } else {
        return 0;
    }
    d->nav_scroll_from = shelf_pos_q16(d, s); /* a click mid-scroll goes on from where the covers are */
    d->nav_scroll_shelf = s;
    d->shelf_q16[s] = v < 0 ? 0 : (v > max ? max : v);
    desk_tween(d, ANIM_KEY_NAV_SCROLL, SEL_GUI_ANIM_NORMAL_MS, SEL_GUI_EASE_OUT);
    d->shelf_nav_clicks++;
    return 1;
}

/* ---- The shared song menu (I305, DUI-P3-16): every page that shows a song row registers it while drawing
 * (song_row_reg); one opener serves right-click, ctrl-click, the trailing "..." button and Shift+F10. ---- */

/* Every song row offers five items. The last removes the queue entry on the Queue page and the song from the library
 * everywhere else (DUI-P4-18). */

static int song_menu_count(const desk* d) {
    return SONG_MENU_ITEMS + (d->sm_ref.pl_id != 0u ? 1 : 0); /* I633: a playlist row adds Remove from Playlist */
}

static const char* song_menu_label(const desk* d, int32_t k) {
    if (k == SONG_MENU_ITEMS) {
        return "Remove from Playlist";
    }
    return sel_uim_song_menu_label((uint32_t)k, d->sm_ref.removable != 0);
}

/* Items on the open menu: the song items, the playlists (at least the "No playlists" line) or the three scope choices. */
static int32_t song_menu_n(const desk* d) {
    int32_t n = 1;
    if (d->sm_step == 0) {
        return song_menu_count(d);
    }
    if (d->sm_step == 2) {
        return 3;
    }
    if (d->sm_step == 4) { /* I611: Sort by header, four sorts, separator, View as header, the view icon strip */
        return 8;
    }
    if (d->sm_step == 3) {
        return d->sm_pl_folder ? 1 : 2;
    }
    n = (int32_t)d->sm_pl_n;
    return n > 0 ? n : 1;
}

/* The window in canvas pixels: the one rectangle a floating menu is placed in and clipped to, whichever region the pointer was in. */
static sel_gui_rect window_rect(const desk* d) {
    return rect_of(0, 0, lpx(d, (int32_t)d->layout.width), lpx(d, (int32_t)d->layout.height));
}

static sel_gui_rect song_menu_item_rect(const desk* d, int32_t k) {
    sel_gui_rect mn = window_rect(d);
    int32_t w = lpx(d, SONG_MENU_W + 40), x = d->menu_x, y = d->menu_y;
    int32_t n = song_menu_n(d);
    if (x + w > mn.x + mn.w) {
        x = mn.x + mn.w - w;
    }
    if (y + n * lpx(d, SONG_MENU_ITEM_H) > mn.y + mn.h) {
        y = mn.y + mn.h - n * lpx(d, SONG_MENU_ITEM_H);
    }
    if (x < mn.x) {
        x = mn.x;
    }
    if (y < mn.y) {
        y = mn.y;
    }
    return rect_of(x, y + k * lpx(d, SONG_MENU_ITEM_H), w, lpx(d, SONG_MENU_ITEM_H));
}

static void song_menu_open(desk* d, int32_t x, int32_t y, const song_ref* ref) {
    d->sm_ref = *ref;
    d->sm_open = 1;
    d->sm_step = 0;
    d->menu_x = x;
    d->menu_y = y;
    d->menus++;
    d->sm_opens++;
}

/* I647: the same menu with a playlist's (Rename, Delete) or a folder's (Delete) items. */
static void plmenu_open(desk* d, int32_t x, int32_t y, uint32_t id, uint32_t folder, const char* name) {
    d->sm_pl_id = id;
    d->sm_pl_folder = folder;
    snprintf(d->sm_pl_name, sizeof d->sm_pl_name, "%s", name != NULL ? name : "");
    d->sm_open = 1;
    d->sm_step = 3;
    d->menu_x = x;
    d->menu_y = y;
    d->menus++;
}

static void pl_rename_begin(desk* d, uint32_t id, const char* name);
static void pls_close(desk* d);
static sel_gui_rect add_btn_rect(desk* d, sel_gui_rect row);
static int add_mode(const desk* d);
static int pl_send(desk* d, const char* line);

/* The Queue page Scope button opens the same menu with the three scope choices (I258); a pick sends SCOPE <name>. */
static void scope_menu_open(desk* d, int32_t x, int32_t y) {
    d->sm_open = 1;
    d->sm_step = 2;
    d->menu_x = x;
    d->menu_y = y;
    d->menus++;
}

/* Registers one song row drawn this frame and, while the pointer is over it, draws its trailing "..." button. A row
 * taller than a list row (a Home tile) puts the button at its top right corner. */
/* Adds a song row to the registry: the one list every page feeds while it draws. A row with the rectangle of an earlier one
 * replaces it, so a panel drawn on its own (Now Playing) can register again without filling the list. The trailing "..."
 * button sits inside the row's right end. NULL when the path is empty or too long, or the registry is full. */
static song_row* song_row_add(desk* d, sel_gui_rect r, const char* path, uint32_t qid, int removable, int home) {
    song_row* e = NULL;
    int32_t eh = lpx(d, 20), in = lpx(d, 8);
    uint32_t i;
    if (path == NULL || path[0] == '\0' || strlen(path) >= sizeof e->ref.path) {
        return NULL;
    }
    for (i = 0; i < d->sm_n; i++) {
        if (rect_same(d->sm_rows[i].r, r)) {
            e = &d->sm_rows[i];
        }
    }
    if (e == NULL) {
        if (d->sm_n >= SM_ROWS) {
            return NULL;
        }
        e = &d->sm_rows[d->sm_n++];
    }
    e->r = r;
    snprintf(e->ref.path, sizeof e->ref.path, "%s", path);
    e->ref.qid = qid;
    e->ref.removable = removable;
    e->ref.home = home;
    e->ref.pl_id = 0u;
    e->ref.pl_index = 0u;
    e->ref.list_kind = 0u;
    e->ref.list_id = 0u;
    e->ref.list_version = 0u;
    e->ref.list_index = 0u;
    e->more = rect_of(r.x + r.w - eh - in, r.h > lpx(d, 64) ? r.y + in : r.y + (r.h - eh) / 2, eh, eh);
    return e;
}

static song_row* song_row_reg(desk* d, sel_gui_rect r, const char* path, uint32_t qid, int removable, int home) {
    song_row* e = song_row_add(d, r, path, qid, removable, home);
    int32_t eh = lpx(d, 20);
    if (e == NULL) {
        return NULL;
    }
    hover_reg(d, r);
    if (add_mode(d) && e->more.w > 0) { /* I652b: the Library page adds to a playlist: every song row ends in Add, the same control as the empty playlist's results */
        sel_gui_rect ab = add_btn_rect(d, r);
        draw_button(d, ab, ARGB(d->theme.token[SEL_GUI_TOKEN_ACCENT]), "Add", 13, ARGB(d->theme.token[SEL_GUI_TOKEN_ACCENT_TEXT]));
        hover_reg(d, ab);
        d->add_drawn++;
        return e;
    }
    if (in_rect(r, d->ptr_x, d->ptr_y) || (d->sm_open && strcmp(d->sm_ref.path, path) == 0)) {
        hover_reg(d, e->more);
        sel_gui_icon_draw(d->canvas, &d->icons, SEL_GUI_ICON_ELLIPSIS, e->more, eh,
                          row_ink(d, e->more, ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT_MUTED])));
    }
    return e;
}

/* I632d: a registered row joins a list: its place in the whole list, and a selected row gets a tint of the accent token. */
static void song_row_set_list(desk* d, song_row* e, uint32_t kind, uint32_t id, uint32_t version, uint32_t index) {
    if (e == NULL) {
        return;
    }
    e->ref.list_kind = kind;
    e->ref.list_id = id;
    e->ref.list_version = version;
    e->ref.list_index = index;
    if (d->sel.list.kind == kind && d->sel.list.id == id && d->sel.list.version == version && sel_uim_selset_contains(&d->sel, index)) {
        sel_gui_scrim(d->canvas, e->r, d->theme.token[SEL_GUI_TOKEN_ACCENT], 72u);
    }
}

/* The playing song's title lines in the Now Playing panel (I372, DUI-P4-16). The panel is drawn on its own and the main
 * region clears the registry before it draws, so both call this: the row is added without a trailing button (the panel
 * title has no room for one) and an earlier row with the same rectangle is replaced. */
static void np_title_reg(desk* d) {
    const char* path = sel_engine_state(d->engine)->song.path;
    song_row* e;
    if (d->np_title.w <= 0 || path == NULL) {
        return;
    }
    e = song_row_add(d, d->np_title, path, 0, 0, 0);
    if (e != NULL) {
        e->more = rect_of(0, 0, 0, 0);
    }
}

static void song_menu_draw(desk* d) {
    int32_t k, n;
    const sel_gui_atlas* semi = desk_font(d, 1, 13);
    if (!d->sm_open || semi == NULL) {
        return;
    }
    n = song_menu_n(d);
    sel_gui_clip_push(d->canvas, window_rect(d));
    {
        /* One raised panel with the overlay shadow; items are text on it, and only the item under the pointer takes the accent
         * fill (the macOS menu convention, switching at once like the sidebar doors: I356, I373). */
        sel_gui_rect first = song_menu_item_rect(d, 0), panel = rect_of(first.x, first.y, first.w, first.h * n);
        int32_t rad = lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_RADIUS]), inset = lpx(d, 4);
        if (d->shadow_ready) {
            sel_gui_shadow_draw(d->canvas, &d->shadow, panel, rad);
        }
        sel_gui_round_rect(d->canvas, panel, rad, ARGB(d->theme.token[SEL_GUI_TOKEN_SURFACE_RAISED]));
        for (k = 0; k < n; k++) {
            sel_gui_rect m = song_menu_item_rect(d, k);
            int lit = hover_weight(d, m) > 0;
            const char* label = d->sm_step == 0 ? song_menu_label(d, k) : d->sm_step == 2 ? sel_uim_scope_label((uint32_t)k)
                              : d->sm_step == 3 ? sel_uim_plmenu_label(sel_uim_plmenu_action(d->sm_pl_folder, (uint32_t)k)) : "No playlists";
            hover_reg(d, m);
            if (d->sm_step == 4) { /* I611: section headers, the checked sort, a separator and the view icon strip */
                uint32_t muted = ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT_MUTED]), accent = ARGB(d->theme.token[SEL_GUI_TOKEN_ACCENT]);
                if (k == 0 || k == 6) {
                    sel_gui_text_wrap(d->canvas, semi, rect_of(m.x + lpx(d, 12), m.y + lpx(d, 4), m.w - lpx(d, 24), semi->line_height),
                                      k == 0 ? "Sort by" : "View as", muted, 1, &d->text);
                } else if (k == 5) {
                    sel_gui_fill_rect(d->canvas, rect_of(m.x + lpx(d, 8), m.y + m.h / 2, m.w - lpx(d, 16), 1), ARGB(d->theme.token[SEL_GUI_TOKEN_FRAME]));
                } else if (k == 7) {
                    uint32_t g;
                    static const uint32_t seg_icon[SEL_UIM_LVIEWS] = {SEL_GUI_ICON_LIST, SEL_GUI_ICON_GALLERY_HORIZONTAL, SEL_GUI_ICON_LAYOUT_GRID, SEL_GUI_ICON_SQUARE};
                    for (g = 0; g < SEL_UIM_LVIEWS; g++) {
                        sel_gui_rect sr = lmenu_strip_rect(d, g);
                        int on = d->side_view == g;
                        hover_reg(d, sr);
                        if (on || hover_weight(d, sr) > 0) {
                            sel_gui_round_rect(d->canvas, sr, lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_RADIUS_SMALL]),
                                               ARGB(d->theme.token[SEL_GUI_TOKEN_SURFACE]));
                        }
                        sel_gui_icon_draw(d->canvas, &d->icons, seg_icon[g], rect_of(sr.x + (sr.w - lpx(d, 18)) / 2, sr.y + (sr.h - lpx(d, 18)) / 2, lpx(d, 18), lpx(d, 18)),
                                          lpx(d, 18), on ? accent : ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT]));
                    }
                } else {
                    int on = d->side_sort == (uint32_t)k - 1u;
                    if (lit) {
                        sel_gui_round_rect(d->canvas, rect_of(m.x + inset, m.y, m.w - 2 * inset, m.h), lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_RADIUS_SMALL]),
                                           ARGB(d->theme.token[SEL_GUI_TOKEN_SURFACE]));
                    }
                    sel_gui_text_wrap(d->canvas, semi, rect_of(m.x + lpx(d, 12), m.y + lpx(d, 4), m.w - lpx(d, 40), semi->line_height),
                                      sel_uim_lsort_label((uint32_t)k - 1u), on ? accent : ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT]), 1, &d->text);
                    if (on) {
                        sel_gui_icon_draw(d->canvas, &d->icons, SEL_GUI_ICON_CHECK, rect_of(m.x + m.w - lpx(d, 28), m.y + (m.h - lpx(d, 16)) / 2, lpx(d, 16), lpx(d, 16)),
                                          lpx(d, 16), accent);
                    }
                }
                continue;
            }
            if (lit) {
                sel_gui_round_rect(d->canvas, rect_of(m.x + inset, m.y, m.w - 2 * inset, m.h),
                                   lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_RADIUS_SMALL]),
                                   ARGB(d->theme.token[SEL_GUI_TOKEN_ACCENT]));
            }
            if (d->sm_step == 1 && (uint32_t)k < d->sm_pl_n) {
                label = d->sm_pls[k].name;
            }
            sel_gui_text_wrap(d->canvas, semi, rect_of(m.x + lpx(d, 8), m.y + lpx(d, 4), m.w - lpx(d, 16), semi->line_height), label,
                              ARGB(d->theme.token[lit ? SEL_GUI_TOKEN_ACCENT_TEXT : SEL_GUI_TOKEN_TEXT]), 1, &d->text);
        }
        sel_gui_dirty_add(d->canvas, panel);
    }
    sel_gui_clip_pop(d->canvas);
}

/* ---- The confirm dialog of the two library removals (DUI-P4-18): Remove from Library names the song, Clear Library names
 * the song count. Nothing is sent until Confirm; Cancel, Escape or a click outside close it. ---- */

enum { NAG_BOX = 20, NAG_COL_ROWS = 4, NAG_ROW_H = 32, NAG_MAX = 16 };
enum { DLG_NONE = 0, DLG_REMOVE = 1, DLG_CLEAR = 2, DLG_SYNC = 3, DLG_QUIT = 4, DLG_PLDEL = 5 };

static const char* path_base(const char* p);

static sel_gui_rect dlg_panel_rect(const desk* d) {
    sel_gui_rect mn = region_rect(d, "main");
    int32_t w = lpx(d, 440), h = lpx(d, 168);
    if (d->dlg_open == DLG_SYNC) { /* I602: a column of NAG_COL_ROWS source rows, a second column beside it past that */
        uint32_t rows = d->nagdlg_n < (uint32_t)NAG_COL_ROWS ? d->nagdlg_n : (uint32_t)NAG_COL_ROWS;
        w = lpx(d, d->nagdlg_n > (uint32_t)NAG_COL_ROWS ? 560 : 440);
        w = w < mn.w - 2 * lpx(d, 16) ? w : mn.w - 2 * lpx(d, 16);
        h = lpx(d, 20 + 28 + 12 + NAG_ROW_H * (int32_t)rows + 16 + 32 + 20);
    }
    return rect_of(mn.x + (mn.w - w) / 2, mn.y + (mn.h - h) / 3, w, h);
}

/* Button 0 is Cancel, button 1 is the confirming one; both sit on the bottom row, right aligned. */
static sel_gui_rect dlg_button_rect(const desk* d, uint32_t i) {
    sel_gui_rect p = dlg_panel_rect(d);
    int32_t w = lpx(d, 132), h = lpx(d, 32), gap = lpx(d, 12), m = lpx(d, 20);
    return rect_of(p.x + p.w - m - w - (int32_t)(1u - i) * (w + gap), p.y + p.h - m - h, w, h);
}

static void dlg_open_remove(desk* d, const char* path) {
    snprintf(d->dlg_path, sizeof d->dlg_path, "%s", path);
    char title[256];
    if (!sel_engine_song_title(d->engine, path, title, sizeof title)) {
        snprintf(title, sizeof title, "%s", path_base(path));
    }
    sel_uim_remove_text(title, d->dlg_text, sizeof d->dlg_text);
    d->dlg_open = DLG_REMOVE;
    d->dlg_opens++;
}

static void dlg_open_clear(desk* d) {
    uint32_t n = sel_engine_state(d->engine)->library_songs;
    uint32_t pl = sel_engine_library_playlists(d->engine);
    d->dlg_playlists = pl;
    sel_uim_clear_text(n, pl, d->dlg_text, sizeof d->dlg_text);
    d->dlg_open = DLG_CLEAR;
    d->dlg_opens++;
}

static void src_sync_send(desk* d, uint32_t i);
static int src_off_send(desk* d, uint32_t flip, uint32_t force_off);
static void nag_snooze(desk* d, uint32_t unticked);
static void nag_dismiss(desk* d);
static void draw_home(desk* d);

/* The sync offer dialog (I602): title, a checkbox per due source (all ticked), and Never / Not now / Sync now on the bottom row,
 * right aligned. The rows are the sources due when it opened, so a SOURCES answer during the dialog does not move them. */
static void nagdlg_open(desk* d) {
    uint32_t i;
    d->nagdlg_n = 0;
    for (i = 0; i < d->src_n && d->nagdlg_n < NAG_MAX; i++) {
        if (d->src[i].due) {
            d->nagdlg_idx[d->nagdlg_n++] = i;
        }
    }
    d->nag_off = 0;
    d->nag_done = 1;
    d->dlg_open = DLG_SYNC;
    d->dlg_opens++;
    d->nagdlg_opens++;
    d->repaint = 1;
}

/* Button 0 is Never, 1 Not now, 2 Sync now. */
static sel_gui_rect nagdlg_button_rect(const desk* d, uint32_t i) {
    sel_gui_rect p = dlg_panel_rect(d);
    int32_t w = lpx(d, 112), h = lpx(d, 32), gap = lpx(d, 12), m = lpx(d, 20);
    return rect_of(p.x + p.w - m - (int32_t)(3u - i) * w - (int32_t)(2u - i) * gap, p.y + p.h - m - h, w, h);
}

static sel_gui_rect nagdlg_row_rect(const desk* d, uint32_t k) {
    sel_gui_rect p = dlg_panel_rect(d);
    int32_t m = lpx(d, 20), cw = (p.w - 2 * m) / (d->nagdlg_n > (uint32_t)NAG_COL_ROWS ? 2 : 1);
    return rect_of(p.x + m + (int32_t)(k / NAG_COL_ROWS) * cw, p.y + lpx(d, 20 + 28 + 12) + (int32_t)(k % NAG_COL_ROWS) * lpx(d, NAG_ROW_H), cw, lpx(d, NAG_ROW_H));
}

static sel_gui_rect nagdlg_box_rect(const desk* d, sel_gui_rect row) {
    int32_t b = lpx(d, NAG_BOX);
    return rect_of(row.x, row.y + (row.h - b) / 2, b, b);
}

/* Sync now sends the ticked rows (the Sources page's message) and snoozes the unticked ones; Not now stores the global notice time;
 * Never switches the ticked sources off in library.sources_off, the setting the Sources tab toggles. */
static void nagdlg_close(desk* d, int action) {
    uint32_t k, ticked = 0, unticked = 0;
    for (k = 0; k < d->nagdlg_n; k++) {
        if (d->nag_off & (1u << d->nagdlg_idx[k])) {
            unticked |= 1u << d->nagdlg_idx[k];
        } else {
            ticked |= 1u << d->nagdlg_idx[k];
        }
    }
    d->dlg_open = DLG_NONE;
    if (action == 2) {
        for (k = 0; k < d->nagdlg_n; k++) {
            uint32_t idx = d->nagdlg_idx[k];
            if ((ticked & (1u << idx)) && !(d->sync_pending & (1u << idx))) {
                src_sync_send(d, idx);
                d->nag_syncs++;
            }
        }
        if (unticked != 0u) {
            nag_snooze(d, unticked);
        }
        d->dlg_confirms++;
    } else if (action == 0) {
        if (ticked != 0u) {
            (void)src_off_send(d, 0u, ticked);
        }
        d->dlg_confirms++;
    } else {
        nag_dismiss(d);
        d->dlg_cancels++;
    }
    d->nag_off = 0;
    d->repaint = 1;
    draw_home(d);
}

static void nagdlg_draw(desk* d, sel_gui_rect p) {
    const sel_gui_atlas* head = desk_font(d, 1, 16);
    uint32_t k, tx = ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT]), mu = ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT_MUTED]);
    int32_t m = lpx(d, 20), pad = lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_SPACING]);
    char title[96];
    snprintf(title, sizeof title, "%u source%s not synced for a while", (unsigned)d->nagdlg_n, d->nagdlg_n == 1u ? "" : "s");
    sel_gui_text_wrap(d->canvas, head, rect_of(p.x + m, p.y + m, p.w - 2 * m, head->line_height), title, tx, 1, &d->text);
    for (k = 0; k < d->nagdlg_n; k++) {
        uint32_t idx = d->nagdlg_idx[k];
        sel_gui_rect row = nagdlg_row_rect(d, k), bx = nagdlg_box_rect(d, row);
        int on = !(d->nag_off & (1u << idx));
        char line[96];
        sel_gui_round_rect(d->canvas, bx, lpx(d, 4), ARGB(d->theme.token[on ? SEL_GUI_TOKEN_ACCENT : SEL_GUI_TOKEN_TRACK]));
        if (on) {
            sel_gui_round_rect(d->canvas, rect_of(bx.x + bx.w / 4, bx.y + bx.h / 4, bx.w / 2, bx.h / 2), lpx(d, 2), ARGB(d->theme.token[SEL_GUI_TOKEN_ACCENT_TEXT]));
        }
        if (d->sync_pending & (1u << idx)) {
            snprintf(line, sizeof line, "%s, Syncing", d->src[idx].name);
        } else if (d->src[idx].synced > 0) {
            snprintf(line, sizeof line, "%s, last synced %lld days ago", d->src[idx].name, (long long)((time(NULL) - d->src[idx].synced) / 86400));
        } else {
            snprintf(line, sizeof line, "%s, never synced", d->src[idx].name);
        }
        draw_text(d, rect_of(bx.x + bx.w + pad, row.y, row.x + row.w - (bx.x + bx.w + pad), row.h), line, 0, 14, on ? tx : mu);
    }
    draw_button(d, nagdlg_button_rect(d, 0u), ARGB(d->theme.token[SEL_GUI_TOKEN_TRACK]), "Never", 14, tx);
    draw_button(d, nagdlg_button_rect(d, 1u), ARGB(d->theme.token[SEL_GUI_TOKEN_TRACK]), "Not now", 14, tx);
    draw_button(d, nagdlg_button_rect(d, 2u), ARGB(d->theme.token[SEL_GUI_TOKEN_ACCENT]), "Sync now", 14, ARGB(d->theme.token[SEL_GUI_TOKEN_ACCENT_TEXT]));
}

static int nagdlg_pointer(desk* d, const sel_gui_input* ev) {
    uint32_t k;
    if (ev->kind != SEL_GUI_IN_CLICK) {
        return ev->kind == SEL_GUI_IN_WHEEL;
    }
    for (k = 0; k < 3u; k++) {
        if (in_rect(nagdlg_button_rect(d, k), ev->x, ev->y)) {
            nagdlg_close(d, (int)k);
            return 1;
        }
    }
    for (k = 0; k < d->nagdlg_n; k++) {
        if (in_rect(nagdlg_row_rect(d, k), ev->x, ev->y)) {
            d->nag_off ^= 1u << d->nagdlg_idx[k];
            d->repaint = 1;
            break;
        }
    }
    return 1; /* a decision dialog: a click outside does nothing */
}

/* I623: Escape with nothing open asks before it quits. */
static void dlg_open_quit(desk* d) {
    snprintf(d->dlg_text, sizeof d->dlg_text, "%s", "Quit Selecta?");
    d->dlg_open = DLG_QUIT;
    d->dlg_opens++;
}

static void dlg_open_pldel(desk* d, uint32_t id, uint32_t folder, const char* name) {
    d->dlg_pl_id = id;
    d->dlg_pl_folder = folder;
    sel_uim_pldel_text(folder, name, d->dlg_text, sizeof d->dlg_text);
    d->dlg_open = DLG_PLDEL;
    d->dlg_opens++;
}

static const char* dlg_title(const desk* d) {
    if (d->dlg_open == DLG_PLDEL) {
        return d->dlg_pl_folder ? "Delete Folder" : "Delete Playlist";
    }
    return d->dlg_open == DLG_CLEAR ? "Clear Library" : d->dlg_open == DLG_QUIT ? "Quit" : "Remove from Library";
}

static const char* dlg_confirm_label(const desk* d) {
    if (d->dlg_open == DLG_PLDEL) {
        return "Delete";
    }
    return d->dlg_open == DLG_CLEAR ? "Clear Library" : d->dlg_open == DLG_QUIT ? "Quit" : "Remove";
}

static void dlg_close(desk* d, int confirm) {
    int kind = d->dlg_open;
    d->dlg_open = DLG_NONE;
    if (confirm && kind == DLG_REMOVE) {
        desk_send_path(d, "LIBRARY_REMOVE", d->dlg_path);
        desk_toast(d, "Removed from library");
    } else if (confirm && kind == DLG_CLEAR) {
        desk_send(d, "LIBRARY_CLEAR");
        desk_toast(d, "Library cleared");
    }
    if (confirm && kind == DLG_QUIT) {
        d->quit_req = 1;
    }
    if (confirm && kind == DLG_PLDEL) { /* I647: the verb goes out, and an opened page of that playlist gives way to the list */
        char line[48];
        sel_uim_pldel_line(d->dlg_pl_folder, d->dlg_pl_id, line, sizeof line);
        d->pl_sel_id = 0u;
        pl_send(d, line);
        if (!d->dlg_pl_folder && d->pls_id == d->dlg_pl_id) {
            pls_close(d);
        }
        desk_toast(d, d->dlg_pl_folder ? "Folder deleted" : "Playlist deleted");
    }
    if (confirm) {
        d->dlg_confirms++;
    } else {
        d->dlg_cancels++;
    }
    draw_home(d);
}

static void dlg_draw(desk* d) {
    sel_gui_rect p, tr;
    const sel_gui_atlas* head = desk_font(d, 1, 16);
    const sel_gui_atlas* body = desk_font(d, 0, 14);
    int32_t m = lpx(d, 20), rad = lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_RADIUS]);
    if (!d->dlg_open || head == NULL || body == NULL) {
        return;
    }
    p = dlg_panel_rect(d);
    if (d->shadow_ready) {
        sel_gui_shadow_draw(d->canvas, &d->shadow, p, rad);
    }
    sel_gui_round_rect(d->canvas, p, rad, ARGB(d->theme.token[SEL_GUI_TOKEN_SURFACE_RAISED]));
    if (d->dlg_open == DLG_SYNC) {
        nagdlg_draw(d, p);
        sel_gui_dirty_add(d->canvas, p);
        return;
    }
    tr = rect_of(p.x + m, p.y + m, p.w - 2 * m, head->line_height);
    sel_gui_text_wrap(d->canvas, head, tr, dlg_title(d),
                      ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT]), 1, &d->text);
    tr = rect_of(p.x + m, p.y + m + head->line_height + lpx(d, 10), p.w - 2 * m, body->line_height * 3);
    sel_gui_text_wrap(d->canvas, body, tr, d->dlg_text, ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT]), 3, &d->text);
    draw_button(d, dlg_button_rect(d, 0u), ARGB(d->theme.token[SEL_GUI_TOKEN_TRACK]), "Cancel", 14, ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT]));
    draw_button(d, dlg_button_rect(d, 1u), ARGB(d->theme.token[SEL_GUI_TOKEN_ACCENT]), dlg_confirm_label(d),
                14, ARGB(d->theme.token[SEL_GUI_TOKEN_ACCENT_TEXT]));
    sel_gui_dirty_add(d->canvas, p);
}

/* While the dialog is open it owns every click: Confirm and Cancel act, a click outside cancels, the rest is swallowed. */
static int dlg_pointer(desk* d, const sel_gui_input* ev) {
    if (!d->dlg_open) {
        return 0;
    }
    if (d->dlg_open == DLG_SYNC) {
        return nagdlg_pointer(d, ev);
    }
    if (ev->kind == SEL_GUI_IN_CLICK) {
        if (in_rect(dlg_button_rect(d, 1u), ev->x, ev->y)) {
            dlg_close(d, 1);
        } else if (in_rect(dlg_button_rect(d, 0u), ev->x, ev->y) || !in_rect(dlg_panel_rect(d), ev->x, ev->y)) {
            dlg_close(d, 0);
        }
        return 1;
    }
    return ev->kind == SEL_GUI_IN_WHEEL;
}

static int dlg_key(desk* d, const sel_gui_input* ev) {
    if (d->dlg_open == DLG_SYNC) { /* Enter syncs the ticked sources, Escape is Not now */
        if (ev->code == 13u || ev->code == 27u) {
            nagdlg_close(d, ev->code == 13u ? 2 : 1);
        }
        return 1;
    }
    if (ev->code == 13u) {
        dlg_close(d, 1);
    } else if (ev->code == 27u) {
        dlg_close(d, 0);
    }
    return 1;
}

/* A click while the menu is open picks an item or dismisses it; otherwise right-click, ctrl-click or a click on a
 * row's "..." button opens it. Returns 1 when the click was consumed. */
static int song_menu_pointer(desk* d, const sel_gui_input* ev) {
    uint32_t i;
    sel_gui_rect mn = region_rect(d, "main");
    if (ev->kind != SEL_GUI_IN_CLICK) {
        return 0;
    }
    if (d->sm_open) {
        int32_t k, n = d->sm_step == 0 ? song_menu_count(d) : d->sm_step == 2 ? 3 : d->sm_step == 3 ? song_menu_n(d) : 8;
        int keep = 0;
        if (d->sm_step == 4) { /* I611: the library sort and view menu: sort rows pick a sort, the strip a view, headers and the separator stay */
            for (k = 0; k < 8; k++) {
                if (!in_rect(song_menu_item_rect(d, k), ev->x, ev->y)) {
                    continue;
                }
                if (k >= 1 && k <= 4) {
                    side_sort_pick(d, (uint32_t)k - 1u);
                    d->sm_open = 0;
                } else if (k == 7) {
                    uint32_t g;
                    keep = 1;
                    for (g = 0; g < SEL_UIM_LVIEWS; g++) {
                        if (in_rect(lmenu_strip_rect(d, g), ev->x, ev->y)) {
                            side_view_pick(d, g);
                            d->sm_open = 0;
                            keep = 0;
                        }
                    }
                } else {
                    keep = 1;
                }
                draw_home(d);
                return 1;
            }
            d->sm_open = 0;
            draw_home(d);
            return 1;
        }
        for (k = 0; k < n; k++) {
            if (!in_rect(song_menu_item_rect(d, k), ev->x, ev->y)) {
                continue;
            }
            if (d->sm_step == 3) { /* I647: Rename puts the name field into editing, Delete asks first */
                uint32_t act = sel_uim_plmenu_action(d->sm_pl_folder, (uint32_t)k);
                d->sm_open = 0;
                if (act == SEL_UIM_PLACT_RENAME) {
                    pl_rename_begin(d, d->sm_pl_id, d->sm_pl_name);
                } else if (act == SEL_UIM_PLACT_DELETE) {
                    dlg_open_pldel(d, d->sm_pl_id, d->sm_pl_folder, d->sm_pl_name);
                }
                draw_home(d);
                return 1;
            }
            if (d->sm_step == 2) { /* the same verb the old cycle button sent, now for the chosen scope */
                char line[32];
                sel_uim_scope_line((uint32_t)k, line, sizeof line);
                d->queue_cmds++;
                desk_send(d, line);
            } else if (d->sm_step == 1) {
                uint32_t pid = (uint32_t)k < d->sm_pl_n ? d->sm_pls[k].id : 0u;
                if (pid != 0u) {
                    char line[640];
                    snprintf(line, sizeof line, "PLAYLIST_ADD_SONG %u \"%s\"", (unsigned)pid, d->sm_ref.path);
                    desk_send(d, line);
                    d->sm_pl_adds++;
                    desk_toast(d, "Added to playlist");
                }
            } else if (k == SONG_MENU_ITEMS) { /* I633: the playlist page's own item */
                char line[48];
                snprintf(line, sizeof line, "PLAYLIST_REMOVE_SONG %u %u", (unsigned)d->sm_ref.pl_id, (unsigned)d->sm_ref.pl_index);
                desk_send(d, line);
                d->pls_want = d->pls_n; /* the page must show fewer songs than this */
                d->pls_stale = 1u; /* the engine applies the command on its next pump: the page reads the list again then */
                d->side_stale = 1u;
                d->repaint = 1;
                d->uim.dirty |= SEL_UIM_DIRTY_ALL;
                desk_toast(d, "Removed from playlist");
            } else if (k == 3) {
                d->sm_pl_n = sel_engine_library_items(d->engine, SEL_LIB_ITEM_BIT(SEL_LIB_ITEM_PLAYLIST), 0u, d->sm_pls, 8u, NULL); /* I631: every saved playlist, unfiltered by the sidebar chip, filter field or sort */
                d->sm_step = 1;
                keep = 1;
            } else {
                static const char* const done[SONG_MENU_ITEMS] = {"Playing", "Playing next", "Added to queue", "", "Removed from queue"};
                uint32_t slot = k == 4 ? 3u : (uint32_t)k;
                if (k == 4 && !d->sm_ref.removable) { /* the song leaves the library: ask first */
                    dlg_open_remove(d, d->sm_ref.path);
                    d->sm_open = 0;
                    draw_home(d);
                    return 1;
                }
                if (k == 4) {
                    char line[48];
                    snprintf(line, sizeof line, "QUEUE_REMOVE %u", (unsigned)d->sm_ref.qid);
                    desk_send(d, line);
                } else if (k == 0) {
                    desk_play_path(d, d->sm_ref.path);
                    d->row_plays++;
                } else {
                    desk_send_path(d, k == 1 ? "QUEUE_PLAY_NEXT" : "QUEUE_ADD", d->sm_ref.path);
                    d->tile_queued += k == 1 && d->sm_ref.home;
                }
                d->menu_picks[slot]++;
                d->sm_picks++;
                {
                    /* I442: the toast names the song, not only the action. */
                    char title[128], text[160];
                    if (!sel_engine_song_title(d->engine, d->sm_ref.path, title, sizeof title)) {
                        snprintf(title, sizeof title, "%s", path_base(d->sm_ref.path));
                    }
                    snprintf(text, sizeof text, "%s: %s", done[k], title);
                    desk_toast(d, text);
                }
            }
            break;
        }
        d->sm_open = keep;
        draw_home(d);
        return 1;
    }
    if (!in_rect(mn, ev->x, ev->y) && !in_rect(d->np_title, ev->x, ev->y)) {
        return 0;
    }
    for (i = 0; i < d->sm_n; i++) {
        const song_row* e = &d->sm_rows[i];
        int ctx = ev->button == 1u;
        int more = ev->button == 0u && in_rect(e->more, ev->x, ev->y) && in_rect(e->r, d->ptr_x, d->ptr_y);
        if ((ctx && in_rect(e->r, ev->x, ev->y)) || more) {
            song_menu_open(d, more ? e->more.x : ev->x, more ? e->more.y + e->more.h : ev->y, &e->ref);
            draw_home(d);
            return 1;
        }
    }
    return 0;
}

/* Shift+F10: the menu opens on the row under the pointer, else the first row on screen. */
static int song_menu_key(desk* d) {
    uint32_t i, pick = 0;
    if (d->sm_n == 0u) {
        return 0;
    }
    for (i = 0; i < d->sm_n; i++) {
        if (in_rect(d->sm_rows[i].r, d->ptr_x, d->ptr_y)) {
            pick = i;
        }
    }
    song_menu_open(d, d->sm_rows[pick].r.x + lpx(d, 16), d->sm_rows[pick].r.y + d->sm_rows[pick].r.h, &d->sm_rows[pick].ref);
    draw_home(d);
    return 1;
}

/* I478: the quick-pick grid: a card of the shelf tiles' surface and radius, small square art on the left, the name beside it. */
static void draw_quick(desk* d, sel_gui_rect mn) {
    const sel_state* st = sel_engine_state(d->engine);
    uint32_t panel = ARGB(d->theme.token[SEL_GUI_TOKEN_PANEL]);
    int32_t radius = lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_RADIUS_LARGE]);
    int32_t gap = lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_SPACING]);
    shelf q;
    uint32_t k, n = home_quick(st, &q);
    d->quick_hover_fill = 0u;
    for (k = 0; k < n; k++) {
        const sel_view_row* r = &st->view->rows[shelf_tile_row(st, &q, k)];
        sel_gui_rect t = quick_tile_rect(d, k);
        sel_gui_rect ar = rect_of(t.x, t.y, t.h, t.h);
        int hovered = d->hover_tile > 0 && (uint32_t)d->hover_tile - 1u == quick_hover_index(st, k);
        uint32_t fill = hovered ? desk_mix(panel, ARGB(d->theme.token[SEL_GUI_TOKEN_SURFACE]), 65536) : panel; /* the shelf card's hover fill */
        const uint32_t* art;
        if (t.y + t.h <= mn.y || t.y >= mn.y + mn.h) {
            continue;
        }
        if (hovered) {
            d->quick_hover_fill = fill;
        }
        if (hovered) { /* I600: an idle card has no fill, so the Home tint shows through; only the hover paints the card */
            sel_gui_round_rect(d->canvas, t, radius, fill);
        }
        d->tile_cards++;
        d->home_parts |= HP_TILE_CARD;
        art = r->art_ref != NULL ? art_tile(d->art, r->art_ref) : NULL;
        if (art != NULL) {
            sel_gui_set_region(d->canvas, SEL_GUI_RGN_TILE);
            sel_gui_blit_image_round(d->canvas, ar, art, DESK_ART_TILE_EDGE, DESK_ART_TILE_EDGE, DESK_ART_TILE_EDGE, radius);
            sel_gui_set_region(d->canvas, SEL_GUI_RGN_UI);
        } else {
            uint32_t slot;
            family_platform_cover(d, d->canvas, ar, row_family(r, NULL), r->text, &slot);
        }
        draw_text(d, rect_of(t.x + t.h + gap, t.y, t.w - t.h - 2 * gap, t.h), r->text, 1, 14, ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT]));
    }
}

static void draw_shelves(desk* d, sel_gui_rect mn) {
    d->shelf_album_art_draws = 0u; /* the stat counts the album covers of the last shelf paint */
    const int on_page = shelf_page_on(d); /* I617: the Shelf page draws one shelf as a grid, with no quick picks, links or buttons */
    if (!on_page) {
        draw_quick(d, mn);
    }
    const sel_state* st = sel_engine_state(d->engine);
    int32_t pad = lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_TILE_PAD]);
    int32_t mg = lpx(d, HOME_MARGIN);
    int32_t hv = 65536; /* a hovered tile is lit at once (I351); no tile fades out */
    uint32_t panel = ARGB(d->theme.token[SEL_GUI_TOKEN_PANEL]);
    shelf sh[SHELF_MAX];
    uint32_t s, k, flat = 0, n = home_shelves(st, sh);
    memset(d->shelf_nav_drawn, 0, sizeof d->shelf_nav_drawn);
    memset(d->shelf_nav_alpha, 0, sizeof d->shelf_nav_alpha);
    memset(d->show_all_rect, 0, sizeof d->show_all_rect);
    d->shelf_page_draws += on_page ? 1u : 0u;
    d->shelf_page_tiles = 0u;
    for (s = 0; s < n; s++) {
        const sel_view_row* head = &st->view->rows[sh[s].head];
        int32_t top = shelf_top(d, s);
        if (on_page && s + 1u != d->shelf_open) {
            flat += sh[s].tiles; /* the hover index counts every shelf's tiles */
            continue;
        }
        if (top >= mn.y + mn.h) {
            break;
        }
        draw_text(d, rect_of(mn.x + mg, top, mn.w - 2 * mg, lpx(d, HOME_SECTION_H)), head->text, 1, 22,
                  ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT]));
        d->home_parts |= HP_SECTION;
        if (!on_page && sh[s].tiles > 0u) { /* I617: the grey Show all link at the right end of the header opens the whole shelf */
            const sel_gui_atlas* at = desk_font(d, 1, SHOW_ALL_PX);
            int32_t tw = at != NULL ? sel_gui_text_width(at, SHOW_ALL_LABEL, strlen(SHOW_ALL_LABEL)) : 0;
            sel_gui_rect lk = rect_of(mn.x + mn.w - mg - tw, top, tw, lpx(d, HOME_SECTION_H));
            int lit = in_rect(lk, d->ptr_x, d->ptr_y);
            if (tw > 0) {
                draw_text(d, lk, SHOW_ALL_LABEL, 1, SHOW_ALL_PX, ARGB(d->theme.token[lit ? SEL_GUI_TOKEN_TEXT : SEL_GUI_TOKEN_TEXT_MUTED]));
                hover_reg(d, lk);
                d->show_all_rect[s] = lk; /* s < SHELF_MAX <= 8 */
                d->show_all_draws++;
            }
        }
        if (sh[s].tiles == 0u && sh[s].rows > 0u) { /* a count line, no tiles */
            draw_text(d, rect_of(mn.x + mg, top + lpx(d, SHELF_HEAD_H), mn.w - 2 * mg, lpx(d, HOME_TITLE_H)),
                      st->view->rows[sh[s].first].text, 0, 14, ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT_MUTED]));
        }
        for (k = 0; k < sh[s].tiles; k++, flat++) {
            const sel_view_row* r = &st->view->rows[shelf_tile_row(st, &sh[s], k)];
            sel_gui_rect t = shelf_tile_rect(d, s, k);
            d->shelf_page_tiles += on_page ? 1u : 0u; /* I617: the grid counts every tile it lays out, drawn or scrolled off */
            sel_gui_rect ar = rect_of(t.x + pad, t.y + pad, t.w - 2 * pad, t.w - 2 * pad);
            int hovered = d->hover_tile > 0 && (uint32_t)d->hover_tile - 1u == flat;
            const uint32_t* art;
            if (t.x + t.w <= mn.x || t.x >= mn.x + mn.w) {
                continue;
            }
            if (hovered) { /* I600: the idle card has no fill (the Home tint shows through, no dark frame on AGA); the hover fades one in */
                sel_gui_round_rect(d->canvas, t, lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_RADIUS_LARGE]),
                                   desk_mix(panel, ARGB(d->theme.token[SEL_GUI_TOKEN_SURFACE]), (uint32_t)hv));
            }
            d->tile_cards++;
            d->home_parts |= HP_TILE_CARD;
            if (d->shadow_ready) {
                sel_gui_shadow_draw_round(d->canvas, &d->shadow, ar, lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_RADIUS_LARGE]), lpx(d, HOME_COVER_SHADOW_LIFT));
            }
            art = r->art_ref != NULL ? art_tile(d->art, r->art_ref) : NULL;
            if (art != NULL) { /* the library cover; generated art only when the song or album has none (design 2.7) */
                d->shelf_album_art_draws += r->album ? 1u : 0u;
                sel_gui_set_region(d->canvas, SEL_GUI_RGN_TILE);
                sel_gui_blit_image_round(d->canvas, ar, art, DESK_ART_TILE_EDGE, DESK_ART_TILE_EDGE, DESK_ART_TILE_EDGE, lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_RADIUS_LARGE]));
                d->art_probe = ar; /* I477: the dump samples this tile's art corner and centre */
                d->art_probe_fill = sel_gui_pixel(d->canvas, ar.x, ar.y - 1); /* the card or the tint just above the art, before it is drawn over */
                sel_gui_set_region(d->canvas, SEL_GUI_RGN_UI);
            } else {
                if (r->album || r->composer) { /* I691d: the majority family of the album's or person's songs */
                    family_platform_cover(d, d->canvas, ar, sel_platform_of_ext(r->format != NULL ? r->format : ""), r->text, &d->pc_shelf);
                } else {
                    uint32_t slot;
                    song_platform_cover(d, ar, r->key, r->text, &slot);
                }
            }
            draw_text(d, rect_of(ar.x, ar.y + ar.h + lpx(d, HOME_TILE_TEXT_GAP), ar.w, lpx(d, HOME_TITLE_H)), r->text, 1, 14,
                      ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT]));
            if (r->artist != NULL) { /* I476: the album artist; a tile without one draws no line */
                draw_text(d, rect_of(ar.x, ar.y + ar.h + lpx(d, HOME_TILE_TEXT_GAP + HOME_TITLE_H), ar.w, lpx(d, HOME_META_H)),
                          r->artist, 0, 13, ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT_MUTED]));
                d->home_parts |= HP_ARTIST_LINE;
            }
            if (!r->album && !r->composer && r->key != NULL) {
                song_row_reg(d, t, r->key, 0, 0, 1);
            }
            if (hovered && hv > 0) {
                int32_t ps = lpx(d, HOME_PLAY_SIZE), in = lpx(d, HOME_PLAY_INSET);
                sel_gui_rect pb = rect_of(ar.x + ar.w - in - ps, ar.y + ar.h - in - ps, ps, ps);
                sel_gui_round_rect(d->canvas, pb, ps / 2, ARGB(d->theme.token[SEL_GUI_TOKEN_ACCENT]));
                sel_gui_icon_draw(d->canvas, &d->icons, SEL_GUI_ICON_PLAY, rect_of(pb.x + ps / 4, pb.y + ps / 4, ps / 2, ps / 2),
                                  ps / 2, ARGB(d->theme.token[SEL_GUI_TOKEN_ACCENT_TEXT]));
                d->home_parts |= HP_PLAY_HOVER;
            }
        }
        if (!on_page && s < 8u) { /* I607: the round previous/next buttons over the covers of the hovered shelf */
            int32_t fade = d->nav_shelf1 == s + 1u ? nav_fade_now(d) : 0;
            uint32_t bits = fade > 0 ? shelf_nav_bits(d, s, &sh[s]) : 0u, dir;
            for (dir = 0; dir < 2u; dir++) {
                sel_gui_rect b = shelf_nav_rect(d, s, (int)dir);
                int32_t ps = b.w, grown = (int32_t)(((int64_t)ps * nav_scale_now(d, dir)) >> 16);
                uint32_t a = nav_alpha(fade) << 24; /* I630: the whole button fades with its opacity */
                if (!(bits & (dir ? NAV_NEXT : NAV_PREV))) {
                    continue;
                }
                b = rect_of(b.x - (grown - ps) / 2, b.y - (grown - ps) / 2, grown, grown);
                ps = grown;
                sel_gui_round_rect(d->canvas, b, ps / 2, a | (d->theme.token[SEL_GUI_TOKEN_SURFACE] & 0xFFFFFFu));
                sel_gui_icon_draw(d->canvas, &d->icons, dir ? SEL_GUI_ICON_CHEVRON_RIGHT : SEL_GUI_ICON_CHEVRON_LEFT,
                                  rect_of(b.x + ps / 4, b.y + ps / 4, ps / 2, ps / 2), ps / 2, a | (d->theme.token[SEL_GUI_TOKEN_TEXT] & 0xFFFFFFu));
                d->shelf_nav_drawn[s] |= (uint8_t)(dir ? NAV_NEXT : NAV_PREV);
                d->shelf_nav_alpha[s] = (uint8_t)nav_alpha(fade);
            }
        }
    }
}

/* ---- Library page (DUI-P4-3 slice A): the engine's composers with their song counts, a composer drilldown and the
 * Stats page (F24) beside them. Every number comes from the engine state (COMPOSERS_SHOW, STATS_SHOW).
 * TODO DUI-P4-3 slice B/C: the tree, column browser, sources, Files and History pages hook in here, as further pages
 * of this view chosen by a segmented control above the list. */
enum { LIB_MARGIN = 24, LIB_TITLE_Y = 24, LIB_TITLE_H = 40, LIB_TAB_H = 32, LIB_ROW_H = 48, LIB_STATS_H = 48, LIB_STATS_LINE = 24, LIB_STATS_MORE_W = 72, LIB_STATS_PANEL_COLS = 2 };

/* I445: the Library header is three rows, the title, the tab row and the list, with a gap of two theme spacings between
 * each, so the title descenders, the buttons and the tabs never touch. A composer drilldown puts its song count and
 * back line where the tabs are. */
static int32_t lib_gap(desk* d) {
    return 2 * lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_SPACING]);
}

/* Sync on launch (I527, I602): SOURCES rows the engine marked due are offered once per launch in a modal dialog (DLG_SYNC) on
 * the first visit to the Library page. The Library layout itself carries no banner: the list starts under the tabs. */

static uint32_t nag_count(const desk* d) {
    uint32_t i, n = 0;
    for (i = 0; i < d->src_n; i++) {
        n += d->src[i].due ? 1u : 0u;
    }
    return n;
}

static int lib_header_inline(desk* d);

/* I548: when the measured title and both header buttons do not fit one row, the buttons take a row of their own under the title
 * and everything below moves down by it. */
static int32_t lib_header_row_h(desk* d) {
    return !lib_header_inline(d) ? lpx(d, 36) : 0;
}

static int32_t lib_tab_y(desk* d) {
    return lpx(d, LIB_TITLE_Y + page_title_h(d)) + lib_header_row_h(d) + lib_gap(d);
}

static int lib_style_below(desk* d);

/* I537: a page too narrow for the tabs and even the icon-only chooser on one row puts the chooser on its own row under the tabs. */
static int32_t lib_style_row_h(desk* d) {
    return lib_style_below(d) ? lpx(d, LIB_TAB_H) + lib_gap(d) : 0;
}

/* I611: the Library page's search field and Sort by row, under the tabs. */
static int32_t lib_tools_h(desk* d) {
    return lpx(d, 36);
}

static int32_t lib_list_y(desk* d) {
    return lib_tab_y(d) + lpx(d, LIB_TAB_H) + lib_gap(d) + lib_style_row_h(d) + lib_tools_h(d);
}

/* I548: the bitmap tier lays each statistic on ONE row (label, then value), so the strip is one line high; elsewhere the label sits over the value. */
static int32_t lib_stats_h(const desk* d) {
    return lpx(d, desk_bitmap_text(d) ? LIB_STATS_LINE : LIB_STATS_H);
}

static int lib_stats_on(const sel_state* st) {
    int flow = st->view != NULL && st->view->style == (uint32_t)SEL_VIEW_STYLE_COVERFLOW;
    return st->stats != NULL && !flow;
}

/* Top edge of the Library body (the rows, the tiles): under the tab row, and under the Statistics strip when it shows
 * (I381), a lib_gap below it like the other header rows. */
static int32_t lib_body_y(desk* d, const sel_state* st) {
    return lib_list_y(d) + (lib_stats_on(st) ? lib_stats_h(d) + lib_gap(d) : 0);
}

/* Next NUL-separated field after `p`. */
static const char* lib_next(const char* p) {
    return p + strlen(p) + 1u;
}

static int lib_view_on(const desk* d, const sel_state* st);
static int lib_grid_on(const sel_state* st);
static void src_sync_send(desk* d, uint32_t i);
static sel_gui_rect src_toggle_rect(desk* d, sel_gui_rect r);
static int lib_bar_rects(desk* d, sel_gui_rect* track, sel_gui_rect* handle);
static void draw_lib_bar(desk* d);
static sel_gui_rect src_sync_rect(desk* d, sel_gui_rect r);
/* ---- I615c: the Browse page behind the top bar Browse icon. Big coloured tiles built from the catalog: the formats (FORMATS_SHOW,
 * the catalog's song formats with their counts), the composers (COMPOSERS_SHOW, the list the Library page
 * and the Home composers shelf use) and the sources (the SOURCES rows of the Sources tab). A format or source tile opens the
 * Search page with that filter as its query (format:mod, source:local), drawn as the tile grid by draw_grid_rows; a composer
 * tile opens the composer's songs as the Home composers shelf does. Each is a page change, so Back returns to the tiles. */
enum { BROWSE_MAX = 48, BROWSE_FORMATS = 24, BROWSE_COMPOSERS = 6, BROWSE_SOURCES = 6, BROWSE_TILE_H = 88, BROWSE_TILE_MIN_W = 200, BROWSE_HEAD_H = 40, BROWSE_PALETTE = 5 };
typedef struct browse_tile {
    uint32_t kind; /* 0 format, 1 composer, 2 source, 3 the Browse Folders tile (I678i) */
    char label[48], arg[96];
    uint32_t count;
} browse_tile;

static uint32_t browse_tiles(const desk* d, browse_tile* out, uint32_t cap) {
    const sel_state* st = sel_engine_state(d->engine);
    uint32_t n = 0, i, k;
    if (st->formats != NULL) { /* FORMATS_SHOW: format, song count per entry */
        const char* p = st->formats;
        while (*p != '\0' && n < cap && n < BROWSE_FORMATS) {
            const char* cnt = lib_next(p);
            out[n].kind = 0u;
            out[n].count = (uint32_t)strtoul(cnt, NULL, 10);
            snprintf(out[n].label, sizeof out[n].label, "%s", p);
            snprintf(out[n].arg, sizeof out[n].arg, "%s", p);
            for (k = 0; out[n].arg[k] != '\0'; k++) { /* the query values are lower case */
                out[n].arg[k] = (char)tolower((unsigned char)out[n].arg[k]);
            }
            n++;
            p = lib_next(cnt);
        }
    }
    if (st->composers != NULL) {
        const char* p = st->composers;
        for (k = 0; *p != '\0' && k < BROWSE_COMPOSERS && n < cap; k++) {
            const char* disp = lib_next(p);
            const char* cnt = lib_next(disp);
            out[n].kind = 1u;
            out[n].count = (uint32_t)strtoul(cnt, NULL, 10);
            snprintf(out[n].label, sizeof out[n].label, "%s", disp);
            snprintf(out[n].arg, sizeof out[n].arg, "%s", p);
            n++;
            p = lib_next(cnt);
        }
    }
    for (i = 0, k = 0; i < d->src_n && k < BROWSE_SOURCES && n < cap; i++) {
        if (d->src[i].enabled) {
            out[n].kind = 2u;
            out[n].count = d->src[i].count;
            snprintf(out[n].label, sizeof out[n].label, "%s", d->src[i].name);
            snprintf(out[n].arg, sizeof out[n].arg, "%s", d->src[i].key);
            n++;
            k++;
        }
    }
    if (n < cap) { /* I678i: Tree and Columns of the retired Library page live on Browse Folders, reached from this tile */
        out[n].kind = 3u;
        out[n].count = 0u;
        snprintf(out[n].label, sizeof out[n].label, "Browse Folders");
        out[n].arg[0] = '\0';
        n++;
    }
    return n;
}

/* The Library lists the UI draws itself (the composers and a composer's songs) scroll by a pixel offset, lib_q16, as the
 * Songs page does (I303); the engine view modes scroll the engine's window instead (lib_wheel) and draw at offset 0. */
static int lib_ui_list(const desk* d, const sel_state* st) {
    return !lib_view_on(d, st);
}

/* The one source of the UI-drawn Library list's layout (I505): a composer row is canon, display, count (3 fields); a
 * drilldown starts after its header (canon, display, count: 3 fields) and a song row is key, name (2 fields). The draw
 * code, lib_ui_row and lib_list_n all walk the rows through these two. */
static const char* lib_rows_first(const sel_state* st) {
    return st->composers;
}

static const char* lib_rows_next(const sel_state* st, const char* p) {
    (void)st;
    return lib_next(lib_next(lib_next(p)));
}

/* Rows of the UI-drawn list: the composers, or the songs of the drilled-into composer. */
static uint32_t lib_list_n(const sel_state* st) {
    uint32_t n = 0;
    const char* p;
    for (p = lib_rows_first(st); p != NULL && *p != '\0'; n++) {
        p = lib_rows_next(st, p);
    }
    return n;
}


/* Top edge of the scrolling rows: under the tab row. Rows scrolled above it are clipped and not clickable. */
static int32_t lib_list_top(desk* d, sel_gui_rect mn, const sel_state* st) {
    return mn.y + lib_body_y(d, st);
}

static int32_t lib_off_px(desk* d, const sel_state* st) {
    return lib_ui_list(d, st) ? lpx_q16(d, d->lib_q16) : 0;
}

/* The one row rectangle of the Library page: drawing, hit-testing and the dump all go through it. */
static sel_gui_rect lib_row_rect(desk* d, sel_gui_rect mn, uint32_t i) {
    int32_t mg = lpx(d, LIB_MARGIN);
    int32_t w = mn.w - 2 * mg;
    return rect_of(mn.x + mg, mn.y + lib_body_y(d, sel_engine_state(d->engine)) + (int32_t)i * lpx(d, LIB_ROW_H) - lib_off_px(d, sel_engine_state(d->engine)),
                   w > 0 ? w : 0, lpx(d, LIB_ROW_H));
}

/* The Statistics strip of the Library page, across the top of the main region between the tab row and the rows or the tile
 * grid, which take the whole main width below it (I381; the column of I379 left the grid one tile wide at 1280x800). Zero
 * width when there are no statistics and in cover flow, which uses the whole main width (I391). */
static sel_gui_rect lib_stats_rect(desk* d, sel_gui_rect mn, const sel_state* st) {
    int32_t mg = lpx(d, LIB_MARGIN), y = mn.y + lib_list_y(d);
    return lib_stats_on(st) ? rect_of(mn.x + mg, y, mn.w - 2 * mg > 0 ? mn.w - 2 * mg : 0, lib_stats_h(d)) : rect_of(mn.x + mn.w, y, 0, 0);
}

/* I455: the More control at the right end of the strip, and the panel it expands over the grid: every statistics row (up
 * to 18) as label and value in two columns, under the strip. The grid keeps its full height while the panel is closed. */
static sel_gui_rect lib_stats_more_rect(desk* d, sel_gui_rect sr) {
    int32_t w = lpx(d, LIB_STATS_MORE_W);
    return rect_of(sr.x + sr.w - w, sr.y, w, lpx(d, LIB_STATS_LINE)); /* on the first line, the one the heading and the labels are on */
}

static uint32_t lib_stats_rows(const sel_state* st) {
    return sel_uim_stats_rows(st->stats);
}

static sel_gui_rect lib_stats_panel_rect(desk* d, sel_gui_rect sr, const sel_state* st) {
    uint32_t per = (lib_stats_rows(st) + LIB_STATS_PANEL_COLS - 1u) / LIB_STATS_PANEL_COLS;
    int32_t pad = 2 * lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_SPACING]);
    return rect_of(sr.x, sr.y + sr.h + lib_gap(d) / 2, sr.w, (int32_t)per * lpx(d, LIB_STATS_LINE) + 2 * pad);
}

static void lib_stats_toggle(desk* d, int open) {
    if (d->stats_open != open) {
        d->stats_open = open;
        draw_home(d);
    }
}

/* 1 when (x, y) is on row i of the Library list and not on a part of it scrolled out above the list top. */
static int lib_row_hit(desk* d, sel_gui_rect mn, uint32_t i, int32_t x, int32_t y) {
    return y >= lib_list_top(d, mn, sel_engine_state(d->engine)) && in_rect(lib_row_rect(d, mn, i), x, y);
}

/* The Library page modes (slice B): the composers list, the tree and the column browser. The last two show the engine's
 * own view rows (VIEW_STYLE tree / drilldown over a LIBRARY_SEARCH result), the same commands the text UI sends. */
enum { LIB_MODES = SEL_UIM_LIB_MODES, LIB_TAB_PAD = 10, LIB_WINDOW_ROWS = 12, LIB_TABS = 2 };
/* I678i: the Library page is retired (design 2026-10-29 section 10, D3). Browse Folders keeps two tabs, Tree and Columns; Composers,
 * Files, History and Sources moved to the sidebar chips, the Local Files page, Recently Played and the source pages. The
 * tab strip is these modes; the older mode numbers stay the engine view keys (sel_uim_libmode_lines). */
static const uint32_t k_folder_tab_mode[LIB_TABS] = {1u, 2u};

/* I440: a tab is as wide as its label (bold, the active look) plus a pad on each side, so the right end of the tab row
 * is free for the style chooser. */
static int32_t lib_tab_text_w(desk* d, uint32_t i) {
    const sel_gui_atlas* at = desk_font(d, 1, 14);
    const char* nm = sel_uim_libmode_name(k_folder_tab_mode[i]);
    return at != NULL ? sel_gui_text_width(at, nm, strlen(nm)) : 0;
}

/* I537: the pad on each side of a tab label. LIB_TAB_PAD when the whole row fits the page between its margins, less (down to
 * 2) when the measured labels would run past the right margin, so the tab row never overflows the main pane. */
static int32_t lib_tab_pad(desk* d) {
    sel_gui_rect mn = panel_rect(d, "main");
    int32_t full = lpx(d, LIB_TAB_PAD), text = 0, gaps = (LIB_TABS - 1) * lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_SPACING]);
    int32_t avail = mn.w - 2 * lpx(d, LIB_MARGIN) - gaps, pad;
    uint32_t k;
    for (k = 0; k < (uint32_t)LIB_TABS; k++) {
        text += lib_tab_text_w(d, k);
    }
    if (text + 2 * LIB_TABS * full <= avail) {
        return full;
    }
    pad = (avail - text) / (2 * LIB_TABS);
    return pad < lpx(d, 2) ? lpx(d, 2) : pad;
}

static int32_t lib_tab_w(desk* d, uint32_t i) {
    return lib_tab_text_w(d, i) + 2 * lib_tab_pad(d);
}

static sel_gui_rect lib_tab_rect(desk* d, sel_gui_rect mn, uint32_t i) {
    int32_t x = mn.x + lpx(d, LIB_MARGIN);
    uint32_t k;
    for (k = 0; k < i; k++) { /* I445: one theme spacing between tabs */
        x += lib_tab_w(d, k) + lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_SPACING]);
    }
    return rect_of(x, mn.y + lib_tab_y(d), lib_tab_w(d, i), lpx(d, LIB_TAB_H));
}

/* I553: the two title row buttons are set in the 12 semibold role (the 6x8 family) on the bitmap tier, so both fit right of the
 * "Library" title on a 640 px screen instead of dropping to a second row. */
static int lib_title_btn_px(const desk* d) { return desk_bitmap_text(d) ? 12 : 14; }
static int32_t lib_title_btn_w(desk* d, const char* label) {
    const sel_gui_atlas* at = desk_font(d, 1, lib_title_btn_px(d));
    return (at != NULL ? sel_gui_text_width(at, label, strlen(label)) : 0) + 2 * lpx(d, 8);
}

static sel_gui_rect lib_add_rect(desk* d, sel_gui_rect mn) {
    int32_t w = lib_title_btn_w(d, "Add Folder"), right = nav_arrow_rect(d, mn, 0).x - lib_gap(d) - lib_gap(d) / 2;
    return rect_of(right - w, mn.y + lpx(d, LIB_TITLE_Y) + lpx(d, 4) + (lib_header_inline(d) ? 0 : lpx(d, page_title_h(d))), w, lpx(d, 32));
}

/* DUI-P4-18: the Clear Library button sits left of Add Folder on the same row. */
static sel_gui_rect lib_clear_rect(desk* d, sel_gui_rect mn) {
    sel_gui_rect ab = lib_add_rect(d, mn);
    int32_t w = lib_title_btn_w(d, "Clear Library");
    return rect_of(ab.x - lib_gap(d) - w, ab.y, w, ab.h);
}

/* I548: the measured "Library" title (the atlas draw_text picks for its box) plus a gap fits left of the Clear Library button when
 * both buttons sit on the title row. Measured on the row's own geometry, so the answer does not depend on where the buttons are drawn. */
static int lib_header_inline(desk* d) {
    sel_gui_rect mn = region_rect(d, "main");
    const sel_gui_atlas* at = desk_text_atlas(d, 1, 32, lpx(d, page_title_h(d)));
    int32_t tw = at != NULL ? sel_gui_text_width(at, "Library", 7u) : 0;
    int32_t right = nav_arrow_rect(d, mn, 0).x - lib_gap(d) - lib_gap(d) / 2;
    int32_t btn = lib_title_btn_w(d, "Add Folder") + lib_gap(d) + lib_title_btn_w(d, "Clear Library");
    return mn.x + lpx(d, LIB_MARGIN) + tw + lib_gap(d) <= right - btn;
}

/* I390: the Library style chooser, a segmented control of three buttons (Grid, Cover Flow, List). It is the only
 * desktop control for VIEW_STYLE. While a scan runs the chooser is not drawn. I440: each segment shows its icon and its
 * name; icons alone were not found. A segment is as wide as its icon plus its label, so no label is ellipsized. The
 * title row has no room for the labels next to the heading and the two buttons, so the chooser sits at the right end
 * of the tab row, its right edge on the page's right margin. A window too narrow for the labels left of it and right
 * of the tabs gets the icons alone (the tooltip still names each one). */
enum { LIB_STYLES = 3, LIB_STYLE_PAD = 8, LIB_STYLE_GAP = 4, LIB_STYLE_ICON = 18, LIB_STYLE_LABEL_PX = 14, LIB_STYLE_TAB_GAP = 16 };
static const char* const lib_style_name[LIB_STYLES] = {"Grid", "Cover Flow", "List"};
static const int32_t lib_style_icon[LIB_STYLES] = {SEL_GUI_ICON_LAYOUT_GRID, SEL_GUI_ICON_GALLERY_HORIZONTAL, SEL_GUI_ICON_LIST};
static const char* const lib_style_cmd[LIB_STYLES] = {"VIEW_STYLE grid", "VIEW_STYLE coverflow", "VIEW_STYLE regular"};

static uint32_t lib_style_selected(const sel_state* st);

/* The label box of segment i: right of the icon, the label's width plus the centred-label pad on each side. */
static int32_t lib_style_label_w(desk* d, uint32_t i) {
    const sel_gui_atlas* at = desk_font(d, 1, LIB_STYLE_LABEL_PX);
    const char* s = lib_style_name[i];
    return (at != NULL ? sel_gui_text_width(at, s, strlen(s)) : 0) + 2 * lpx(d, 8);
}

/* The right edge the chooser ends at (the page's right margin; the tab row has nothing under the navigation arrows) and
 * the left edge it must stay right of (the last tab). */
static int32_t lib_style_right(desk* d, sel_gui_rect mn) {
    return mn.x + mn.w - lpx(d, LIB_MARGIN);
}

static int32_t lib_tabs_end(desk* d, sel_gui_rect mn) {
    sel_gui_rect t = lib_tab_rect(d, mn, LIB_TABS - 1u);
    return t.x + t.w;
}

/* 1 when the labelled chooser does not fit between the tabs and its right edge: the segments then show icons alone. */
static int lib_style_compact(desk* d, sel_gui_rect mn) {
    int32_t w = (LIB_STYLES - 1) * lpx(d, LIB_STYLE_GAP);
    uint32_t k;
    if (lib_style_below(d)) { /* a row of its own: the labels fit */
        return 0;
    }
    for (k = 0; k < (uint32_t)LIB_STYLES; k++) {
        w += 2 * lpx(d, LIB_STYLE_PAD) + lpx(d, LIB_STYLE_ICON) + lib_style_label_w(d, k) - lpx(d, LIB_STYLE_PAD);
    }
    return lib_style_right(d, mn) - w < lib_tabs_end(d, mn) + lpx(d, LIB_STYLE_TAB_GAP);
}

/* 1 when the icon-only chooser does not fit right of the last tab either (I537). */
static int lib_style_below(desk* d) {
    sel_gui_rect mn = region_rect(d, "main");
    int32_t w = (LIB_STYLES - 1) * lpx(d, LIB_STYLE_GAP) + LIB_STYLES * (2 * lpx(d, LIB_STYLE_PAD) + lpx(d, LIB_STYLE_ICON));
    return lib_style_right(d, mn) - w < lib_tabs_end(d, mn) + lpx(d, LIB_STYLE_TAB_GAP);
}

/* The label box width of segment i as laid out: 0 in the compact chooser. */
static int32_t lib_style_text_w(desk* d, sel_gui_rect mn, uint32_t i) {
    return lib_style_compact(d, mn) ? 0 : lib_style_label_w(d, i);
}

static sel_gui_rect lib_style_rect(desk* d, sel_gui_rect mn, uint32_t i) {
    int32_t gap = lpx(d, LIB_STYLE_GAP), lead = lpx(d, LIB_STYLE_PAD) + lpx(d, LIB_STYLE_ICON), x = lib_style_right(d, mn), w = 0;
    int compact = lib_style_compact(d, mn);
    uint32_t k;
    for (k = LIB_STYLES; k-- > i;) { /* lay the segments out right to left from the right edge */
        w = lead + (compact ? lpx(d, LIB_STYLE_PAD) : lib_style_label_w(d, k));
        x -= w + (k + 1u < (uint32_t)LIB_STYLES ? gap : 0);
    }
    return rect_of(x, mn.y + lib_tab_y(d) + lib_style_row_h(d), w, lpx(d, LIB_TAB_H));
}

/* I315: while a scan runs the Add Folder button gives way to the file being added and a progress bar. Same row and
 * right edge as the button, wider so a file name fits. */
static sel_gui_rect lib_scan_rect(desk* d, sel_gui_rect mn) {
    sel_gui_rect ab = lib_add_rect(d, mn);
    int32_t w = lpx(d, 360);
    return rect_of(ab.x + ab.w - w, ab.y, w, ab.h);
}

static const char* path_base(const char* p) {
    const char* b = strrchr(p, '/');
    return b != NULL ? b + 1 : p;
}

/* Draws the scan progress into its rectangle (the panel colour first, so it repaints in place) and marks only that
 * rectangle dirty (plan 2.9, dirty rectangles). */
static void lib_scan_draw(desk* d, sel_gui_rect mn) {
    sel_gui_rect r = lib_scan_rect(d, mn), bar;
    int32_t bh = lpx(d, 4), fill = d->scan_total > 0u ? (int32_t)(((uint64_t)d->scan_done * (uint32_t)r.w) / d->scan_total) : 0;
    char line[SEL_SCAN_FILE_MAX + 40];
    sel_gui_fill_rect(d->canvas, r, ARGB(d->theme.token[SEL_GUI_TOKEN_PANEL]));
    if (d->scan_total > 0u) {
        snprintf(line, sizeof line, "%u of %u  %s", (unsigned)d->scan_done, (unsigned)d->scan_total, path_base(d->scan_file));
    } else {
        snprintf(line, sizeof line, "Reading folder");
    }
    draw_text(d, rect_of(r.x, r.y, r.w, r.h - bh - lpx(d, 4)), line, 0, 14, ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT]));
    bar = rect_of(r.x, r.y + r.h - bh, r.w, bh);
    sel_gui_round_rect(d->canvas, bar, bh / 2, ARGB(d->theme.token[SEL_GUI_TOKEN_TRACK]));
    if (fill >= bh) {
        sel_gui_round_rect(d->canvas, rect_of(bar.x, bar.y, fill, bh), bh / 2, ARGB(d->theme.token[SEL_GUI_TOKEN_ACCENT]));
    }
    sel_gui_dirty_add(d->canvas, r);
}

/* The crumb after the tabs: the values picked in the column browser; a click on it clears them. */
static sel_gui_rect lib_crumb_rect(desk* d, sel_gui_rect mn) {
    int32_t x = lib_tabs_end(d, mn) + lpx(d, LIB_TAB_PAD), w = lib_style_rect(d, mn, 0).x - lpx(d, LIB_TAB_PAD) - x;
    return rect_of(x, mn.y + lib_tab_y(d), w > 0 ? w : 0, lpx(d, LIB_TAB_H));
}

static int lib_view_on(const desk* d, const sel_state* st) {
    if (d->lib_mode == 0u || st->view == NULL) { /* I678i: modes 3 to 5 (Files, History, Sources) are retired: nothing sets them */
        return 0;
    }
    return st->view->source == SEL_VIEW_QUERY &&
           st->view->style == (d->lib_mode == 1u ? (uint32_t)SEL_VIEW_STYLE_TREE : (uint32_t)SEL_VIEW_STYLE_DRILLDOWN);
}

static uint32_t lib_hash(const desk* d, const sel_state* st) {
    uint32_t h = d->lib_mode * 977u;
    if (lib_view_on(d, st)) {
        uint32_t k;
        h += st->view->row_count * 31u + st->view->drill_n * 131u + st->view->cursor;
        for (k = 0; k < st->view->row_n; k++) {
            h += st->view->rows[k].expanded * (k + 7u) + st->view->rows[k].count * (k + 3u);
        }
    }
    if (st->view != NULL) {
        h += st->view->style * 7919u + st->view->cursor * 131u + st->view->row_n * 17u; /* the grid and the cover flow redraw on a move */
    }
    return h + (st->composers != NULL ? 1u : 0u) + (st->stats != NULL ? 2u : 0u);
}

/* ---- Queue page (DUI-P4-8, F30, F49): the engine's play queue as a list with its groups (divider rows), a Play Next and a
 * Remove action per row, drag reorder, and the Undo, Redo, Reshuffle and Scope buttons. The engine owns the queue, the
 * undo history and the shuffle (QUEUE_MOVE, QUEUE_REMOVE, QUEUE_PLAY_NEXT, UNDO, REDO, RESHUFFLE, SCOPE); the page only
 * draws and sends those verbs. */
enum { QUEUE_BTN_Y = 64, QUEUE_BTN_H = 32, QUEUE_BTN_W = 128, QUEUE_BTN_GAP = 8, QUEUE_LIST_Y = 112, QUEUE_ROW_H = 40,
       QUEUE_NEXT_W = 104, QUEUE_DEL_W = 40, QUEUE_BUTTONS = 4 };

static uint32_t queue_scope_index(const sel_state* st) {
    uint32_t k;
    for (k = 0; k < 3u; k++) {
        if (strcmp(st->options[SEL_OPT_SCOPE], sel_uim_scope_name((uint32_t)k)) == 0) {
            return k;
        }
    }
    return 0u;
}

static sel_gui_rect queue_btn_rect(desk* d, sel_gui_rect mn, uint32_t k) {
    return rect_of(mn.x + lpx(d, HOME_MARGIN) + (int32_t)k * lpx(d, QUEUE_BTN_W + QUEUE_BTN_GAP), mn.y + lpx(d, page_y(d, QUEUE_BTN_Y)),
                   lpx(d, QUEUE_BTN_W), lpx(d, QUEUE_BTN_H));
}

/* Largest Queue scroll offset in logical pixels, Q16: the rows' height beyond the room under the list top (I258). */
static int32_t queue_max_q16(desk* d, sel_gui_rect mn, const sel_state* st) {
    int32_t max = (int32_t)(st->queue_count + (d->qg.valid ? d->qg.headers : 0u)) * QUEUE_ROW_H - px_to_logical(d, mn.h - lpx(d, page_y(d, QUEUE_LIST_Y)));
    return max > 0 ? max << 16 : 0;
}

/* The scroll offset in device pixels, clamped to the queue (it shrinks when entries are removed). */
static int32_t queue_scroll_px(desk* d, sel_gui_rect mn) {
    int32_t max = queue_max_q16(d, mn, sel_engine_state(d->engine));
    if (d->queue_q16 > max) {
        d->queue_q16 = max;
    }
    return lpx_q16(d, d->queue_q16);
}

/* The rows' area under the buttons: rows scrolled above it or past the panel bottom are clipped to it. */
static sel_gui_rect queue_list_rect(desk* d, sel_gui_rect mn) {
    return rect_of(mn.x, mn.y + lpx(d, page_y(d, QUEUE_LIST_Y)), mn.w, mn.h - lpx(d, page_y(d, QUEUE_LIST_Y)));
}

/* Layout slot of entry i: its own index plus the album group headers above it (I258; the cache is rebuilt by queue_groups_sync). */
static uint32_t queue_slot(const desk* d, uint32_t i) {
    return d->qg.valid && i <= d->qg.count ? i + d->qg.before[i] : i;
}

static sel_gui_rect queue_row_rect(desk* d, sel_gui_rect mn, uint32_t i) {
    return rect_of(mn.x + lpx(d, HOME_MARGIN), mn.y + lpx(d, page_y(d, QUEUE_LIST_Y)) + (int32_t)queue_slot(d, i) * lpx(d, QUEUE_ROW_H) - queue_scroll_px(d, mn),
                   mn.w - 2 * lpx(d, HOME_MARGIN), lpx(d, QUEUE_ROW_H));
}

/* 1 when any part of row i is in the list area (a half scrolled row is drawn clipped). */
static int queue_row_visible(desk* d, sel_gui_rect mn, uint32_t i) {
    sel_gui_rect r = queue_row_rect(d, mn, i), l = queue_list_rect(d, mn);
    return r.y + r.h > l.y && r.y < l.y + l.h;
}

static sel_gui_rect queue_del_rect(desk* d, sel_gui_rect mn, uint32_t i) {
    sel_gui_rect r = queue_row_rect(d, mn, i);
    return rect_of(r.x + r.w - lpx(d, QUEUE_DEL_W), r.y, lpx(d, QUEUE_DEL_W), r.h);
}

static sel_gui_rect queue_next_rect(desk* d, sel_gui_rect mn, uint32_t i) {
    sel_gui_rect r = queue_del_rect(d, mn, i);
    return rect_of(r.x - lpx(d, QUEUE_NEXT_W), r.y, lpx(d, QUEUE_NEXT_W), r.h);
}

/* The row under (x, y), -1 for none. */
static int32_t queue_row_at(desk* d, sel_gui_rect mn, int32_t x, int32_t y) {
    const sel_state* st = sel_engine_state(d->engine);
    uint32_t i;
    for (i = 0; i < st->queue_count; i++) {
        if (queue_row_visible(d, mn, i) && in_rect(queue_row_rect(d, mn, i), x, y) && in_rect(queue_list_rect(d, mn), x, y)) {
            return (int32_t)i;
        }
    }
    return -1;
}

/* FNV-1a over every entry's id, flags and path in queue order: equal hashes mean the same queue in the same order. */
static uint32_t queue_hash(const sel_state* st) {
    uint32_t h = 2166136261u, i;
    const char* p;
    for (i = 0; i < st->queue_count; i++) {
        uint32_t v[2], k;
        v[0] = st->queue[i].id;
        v[1] = st->queue[i].flags;
        for (k = 0; k < 8u; k++) {
            h = (h ^ (uint32_t)((const unsigned char*)v)[k]) * 16777619u;
        }
        for (p = st->queue_text + st->queue[i].path_offset; *p != '\0'; p++) {
            h = (h ^ (uint32_t)(unsigned char)*p) * 16777619u;
        }
        h = (h ^ 0xFFu) * 16777619u;
    }
    return h;
}

/* Rebuilds the album groups when the queue changed. A queue holding a playlist divider (.prg) keeps its dividers as the
 * only groups. Otherwise consecutive songs of one album share a header; a song without an album falls back to its
 * source, one the library does not know gets no header. The engine answers (sel_engine_song_group); the UI reads no
 * database. */
static void queue_groups_sync(desk* d) {
    const sel_state* st = sel_engine_state(d->engine);
    uint32_t h = queue_hash(st), i, n = st->queue_count, run = 0, cur = 0;
    int dividers = 0;
    char grp[160];
    if (d->qg.valid && d->qg.hash == h && d->qg.count == n) {
        return;
    }
    if (d->qg.before == NULL || d->qg.count < n) { /* grow (never shrinks) */
        free(d->qg.before);
        free(d->qg.name);
        d->qg.before = (uint32_t*)malloc(((size_t)n + 1u) * sizeof *d->qg.before);
        d->qg.name = (uint32_t*)malloc(((size_t)n + 1u) * sizeof *d->qg.name);
        if (d->qg.before == NULL || d->qg.name == NULL) {
            d->qg.valid = 0;
            return;
        }
    }
    d->qg.pool_n = 0;
    d->qg.headers = 0;
    for (i = 0; i < n; i++) {
        dividers |= (st->queue[i].flags & SEL_QUEUE_FLAG_DIVIDER) != 0u;
    }
    for (i = 0; i < n; i++) {
        d->qg.before[i] = d->qg.headers;
        d->qg.name[i] = 0;
        grp[0] = '\0';
        if (!dividers && (st->queue[i].flags & SEL_QUEUE_FLAG_DIVIDER) == 0u) {
            sel_engine_song_group(d->engine, st->queue_text + st->queue[i].path_offset, grp, sizeof grp);
        }
        if (grp[0] == '\0') {
            run = 0;
            continue;
        }
        if (run == 0u || strcmp(grp, d->qg.pool + cur) != 0) {
            size_t need = strlen(grp) + 1u;
            if (d->qg.pool_n + need > d->qg.pool_cap) {
                size_t nc = d->qg.pool_cap != 0u ? d->qg.pool_cap * 2u : 4096u;
                char* np;
                while (nc < d->qg.pool_n + need) {
                    nc *= 2u;
                }
                np = (char*)realloc(d->qg.pool, nc);
                if (np == NULL) {
                    d->qg.valid = 0;
                    return;
                }
                d->qg.pool = np;
                d->qg.pool_cap = nc;
            }
            cur = (uint32_t)d->qg.pool_n;
            memcpy(d->qg.pool + cur, grp, need);
            d->qg.pool_n += need;
            d->qg.name[i] = cur + 1u;
            d->qg.headers++;
            d->qg.before[i] = d->qg.headers; /* the header sits above this entry */
            run = 1;
        }
    }
    d->qg.before[n] = d->qg.headers;
    d->qg.hash = h;
    d->qg.count = n;
    d->qg.valid = 1;
}

/* The one-line empty state of a page: a muted line at y under the page heading. Search, Queue, Playlists and Song Information
 * share it; the text is kept for the dump so a test sees what was drawn. */
static void draw_empty_state(desk* d, sel_gui_rect mn, int32_t y, const char* text) {
    draw_text(d, rect_of(mn.x + lpx(d, HOME_MARGIN), y, mn.w - 2 * lpx(d, HOME_MARGIN), lpx(d, 28)), text, 0, 14,
              ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT_MUTED]));
    snprintf(d->empty_text, sizeof d->empty_text, "%s", text);
}

static void draw_queue(desk* d) {
    const sel_state* st = sel_engine_state(d->engine);
    sel_gui_rect mn = draw_panel(d, "main");
    int32_t mg = lpx(d, HOME_MARGIN), ic = lpx(d, 18);
    uint32_t text = ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT]), muted = ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT_MUTED]);
    uint32_t accent = ARGB(d->theme.token[SEL_GUI_TOKEN_ACCENT]);
    sel_engine_stats es;
    uint32_t i, shown = 0;
    char buf[300];
    sel_engine_stats_get(d->engine, &es);
    queue_groups_sync(d);
    d->empty_text[0] = '\0';
    sel_gui_clip_push(d->canvas, mn);
    draw_text(d, rect_of(mn.x + mg, mn.y + lpx(d, 16), mn.w - 2 * mg, lpx(d, page_title_h(d))), "Queue", 1, 32, text);
    for (i = 0; i < QUEUE_BUTTONS; i++) {
        sel_gui_rect b = queue_btn_rect(d, mn, i);
        const char* label = i == 0u ? "Undo" : i == 1u ? "Redo" : i == 2u ? "Reshuffle" : sel_uim_scope_label(queue_scope_index(st));
        int live = i == 0u ? es.undo_depth > 0u : i == 1u ? es.redo_depth > 0u : 1;
        draw_button(d, b, ARGB(d->theme.token[SEL_GUI_TOKEN_SURFACE]), label, 13, live ? text : muted);
    }
    sel_gui_clip_push(d->canvas, queue_list_rect(d, mn));
    for (i = 0; i < st->queue_count; i++) {
        sel_gui_rect r = queue_row_rect(d, mn, i);
        if (d->qg.valid && d->qg.name[i] != 0u) { /* an album group header takes the row above its first song */
            sel_gui_rect hr = rect_of(r.x, r.y - lpx(d, QUEUE_ROW_H), r.w, r.h), lr = queue_list_rect(d, mn);
            if (hr.y + hr.h > lr.y && hr.y < lr.y + lr.h) {
                draw_text(d, hr, d->qg.pool + d->qg.name[i] - 1u, 1, 13, muted);
                shown++;
            }
        }
        if (!queue_row_visible(d, mn, i)) {
            continue;
        }
        shown++;
        int divider = (st->queue[i].flags & SEL_QUEUE_FLAG_DIVIDER) != 0u;
        int playing = st->queue[i].id == st->queue_playing;
        sel_gui_rect nx = queue_next_rect(d, mn, i), dl = queue_del_rect(d, mn, i);
        uint32_t qink = divider ? muted : row_ink(d, r, playing ? accent : text), qmuted = divider ? muted : row_ink(d, r, muted);
        if ((int32_t)i == d->q_focus) { /* the keyboard focus row (I498) */
            sel_gui_fill_rect(d->canvas, r, ARGB(d->theme.token[SEL_GUI_TOKEN_SURFACE_RAISED]));
        }
        if (!divider) {
            song_row_set_list(d, song_row_reg(d, rect_of(r.x, r.y, nx.x - r.x, r.h), st->queue_text + st->queue[i].path_offset, st->queue[i].id, 1, 0), SEL_UIM_LIST_QUEUE, 0u, st->queue_version, i);
        }
        if (divider) { /* a group header: its name in the muted bold face, nothing to play next */
            draw_text(d, rect_of(r.x, r.y, nx.x - r.x, r.h), st->queue_text + st->queue[i].path_offset, 1, 13, muted);
        } else {
            queue_title(st, i, buf, sizeof buf);
            if (playing) {
                sel_gui_icon_draw(d->canvas, &d->icons, SEL_GUI_ICON_PLAY, rect_of(r.x, r.y + (r.h - ic) / 2, ic, ic), ic, accent);
            }
            draw_text(d, rect_of(r.x + ic + lpx(d, 12), r.y, nx.x - r.x - ic - lpx(d, 12), r.h), buf, playing, 14, qink);
            draw_text(d, nx, "Play Next", 1, 12, qmuted);
        }
        sel_gui_icon_draw(d->canvas, &d->icons, SEL_GUI_ICON_X, rect_of(dl.x + (dl.w - ic) / 2, dl.y + (dl.h - ic) / 2, ic, ic), ic, qmuted);
    }
    if (d->qdrag_from >= 0 && d->qdrag_from < (int32_t)st->queue_count) { /* I258e: the lifted row at the pointer and the insertion line */
        uint32_t f = (uint32_t)d->qdrag_from, g = (uint32_t)d->qdrag_gap;
        sel_gui_rect fr = queue_row_rect(d, mn, f), gr = queue_row_rect(d, mn, g);
        int32_t ly = g > f ? gr.y + gr.h : gr.y; /* the line sits below the target row when moving down, above it when moving up */
        sel_gui_rect lift = rect_of(fr.x, d->qdrag_y - fr.h / 2, fr.w, fr.h);
        if (g != f) {
            sel_gui_fill_rect(d->canvas, rect_of(gr.x, ly - lpx(d, 1), gr.w, lpx(d, 2)), accent);
        }
        sel_gui_fill_rect(d->canvas, lift, ARGB(d->theme.token[SEL_GUI_TOKEN_SURFACE_HOVER]));
        queue_title(st, f, buf, sizeof buf);
        draw_text(d, rect_of(lift.x + ic + lpx(d, 12), lift.y, lift.w - ic - lpx(d, 12), lift.h), buf, 1, 14, text);
        d->qdrag_paints++;
    }
    sel_gui_clip_pop(d->canvas);
    if (st->queue_count == 0u) {
        draw_empty_state(d, mn, mn.y + lpx(d, page_y(d, QUEUE_LIST_Y)), sel_uim_state_text(SEL_UIM_SV_QUEUE, SEL_UIM_ST_EMPTY));
    }
    d->queue_draws++;
    d->queue_rows = shown;
    sel_gui_clip_pop(d->canvas);
    sel_gui_dirty_add(d->canvas, mn);
}

/* One pointer event on the Queue page. A click on a button or a row action, or a drag from one row onto another,
 * becomes one engine verb. Returns 1 when the picture may have changed. */
static int queue_pointer(desk* d, const sel_gui_input* ev) {
    const sel_state* st = sel_engine_state(d->engine);
    sel_gui_rect mn = region_rect(d, "main");
    char line[340];
    uint32_t k;
    int32_t row, to;
    queue_groups_sync(d);
    if (ev->kind == SEL_GUI_IN_MOVE) { /* I258e: a held primary button over a queue row lifts it and shows where it would land */
        int was = d->qdrag_from >= 0;
        row = ev->code == 1u ? queue_row_at(d, mn, ev->x2, ev->y2) : -1;
        if (row >= 0 && (st->queue[row].flags & SEL_QUEUE_FLAG_DIVIDER) != 0u) {
            row = -1;
        }
        to = row >= 0 ? queue_row_at(d, mn, ev->x, ev->y) : -1;
        d->qdrag_from = row;
        d->qdrag_gap = to >= 0 ? to : row;
        d->qdrag_x = ev->x;
        d->qdrag_y = ev->y;
        if (row >= 0 || was) {
            draw_home(d); /* the preview follows the pointer, as the wheel scroll redraws the page */
            return 1;
        }
        return 0;
    }
    if (ev->kind == SEL_GUI_IN_DRAG || ev->kind == SEL_GUI_IN_CLICK) {
        int was = d->qdrag_from >= 0;
        d->qdrag_from = -1; /* the release ends the preview */
        if (was && ev->kind == SEL_GUI_IN_CLICK) {
            return 1;
        }
    }
    if (ev->kind == SEL_GUI_IN_DRAG) {
        row = queue_row_at(d, mn, ev->x, ev->y);
        to = queue_row_at(d, mn, ev->x2, ev->y2);
        if (row < 0 || to < 0 || row == to) {
            return 0;
        }
        snprintf(line, sizeof line, "QUEUE_MOVE %u %u", (unsigned)st->queue[row].id, (unsigned)st->queue[to].id); /* entry ids */
        d->queue_cmds++;
        return desk_send(d, line);
    }
    if (ev->kind != SEL_GUI_IN_CLICK || ev->button != 0u) {
        return 0;
    }
    for (k = 0; k < QUEUE_BUTTONS; k++) {
        if (in_rect(queue_btn_rect(d, mn, k), ev->x, ev->y)) {
            if (k == 3u) { /* Scope opens a menu of the three choices under the button; a pick sends SCOPE <name> */
                sel_gui_rect b = queue_btn_rect(d, mn, k);
                scope_menu_open(d, b.x, b.y + b.h);
                draw_home(d);
                return 1;
            }
            snprintf(line, sizeof line, "%s", k == 0u ? "UNDO" : k == 1u ? "REDO" : "RESHUFFLE");
            d->queue_cmds++;
            return desk_send(d, line);
        }
    }
    row = queue_row_at(d, mn, ev->x, ev->y);
    if (row < 0) {
        return 0;
    }
    if (in_rect(queue_del_rect(d, mn, (uint32_t)row), ev->x, ev->y)) {
        snprintf(line, sizeof line, "QUEUE_REMOVE %u", (unsigned)st->queue[row].id);
        d->queue_cmds++;
        return desk_send(d, line);
    }
    if (in_rect(queue_next_rect(d, mn, (uint32_t)row), ev->x, ev->y) && (st->queue[row].flags & SEL_QUEUE_FLAG_DIVIDER) == 0u) {
        snprintf(line, sizeof line, "QUEUE_PLAY_NEXT %s", st->queue_text + st->queue[row].path_offset);
        d->queue_cmds++;
        return desk_send(d, line);
    }
    return 0;
}

/* ---- Playlists page (DUI-P4-9): the library's folder tree (folders and saved playlists, always shown open) with the
 * buttons New Folder, New Playlist, Rename, Delete (asks to confirm) and Play, and a name field. The engine owns the
 * folders and playlists (FOLDER_ADD, PLAYLIST_CREATE, PLAYLIST_RENAME, PLAYLIST_DEL, FOLDER_DEL, PLAYLIST_MOVE,
 * FOLDER_MOVE, VIEW_ACTIVATE on the folders source); the page only draws the rows and sends those verbs. A new
 * folder or playlist goes into the selected folder (or the folder that holds the selected playlist). Dragging a row
 * onto a folder row moves it there. */
enum { PL_BUTTONS = 3, PL_NAME_Y = 104, PL_NAME_W = 408, PL_LIST_Y = 152, PL_INDENT = 24, PL_SLOTS = 6 };

static sel_gui_rect pl_name_rect(desk* d, sel_gui_rect mn) {
    return rect_of(mn.x + lpx(d, HOME_MARGIN), mn.y + lpx(d, page_y(d, PL_NAME_Y)), lpx(d, PL_NAME_W), lpx(d, QUEUE_BTN_H));
}

static sel_gui_rect pl_row_rect(desk* d, sel_gui_rect mn, uint32_t i) {
    return rect_of(mn.x + lpx(d, HOME_MARGIN), mn.y + lpx(d, page_y(d, PL_LIST_Y)) + (int32_t)i * lpx(d, QUEUE_ROW_H),
                   mn.w - 2 * lpx(d, HOME_MARGIN), lpx(d, QUEUE_ROW_H));
}

static int pl_row_visible(desk* d, sel_gui_rect mn, uint32_t i) {
    return mn.y + lpx(d, page_y(d, PL_LIST_Y)) + ((int32_t)i + 1) * lpx(d, QUEUE_ROW_H) <= mn.y + mn.h;
}

static const sel_view_state* pl_view(const sel_state* st) {
    return st->view != NULL && st->view->source == (uint32_t)SEL_VIEW_FOLDERS ? st->view : NULL;
}

static int32_t pl_row_at(desk* d, sel_gui_rect mn, int32_t x, int32_t y) {
    const sel_view_state* v = pl_view(sel_engine_state(d->engine));
    uint32_t i;
    for (i = 0; v != NULL && i < v->row_n && pl_row_visible(d, mn, i); i++) {
        if (in_rect(pl_row_rect(d, mn, i), x, y)) {
            return (int32_t)i;
        }
    }
    return -1;
}

/* The row index of the selection, -1 for none (the selection is a kind and a database id, so it survives a reload). */
static int32_t pl_sel_row(const desk* d, const sel_view_state* v) {
    uint32_t i;
    for (i = 0; v != NULL && d->pl_sel_id != 0u && i < v->row_n; i++) {
        if (v->rows[i].kind == d->pl_sel_kind && v->rows[i].id == d->pl_sel_id) {
            return (int32_t)i;
        }
    }
    return -1;
}

/* The folder a new item goes into: the selected folder, the folder above a selected playlist, 0 (the root) otherwise. */
static uint32_t pl_target_folder(const desk* d, const sel_view_state* v) {
    int32_t s = pl_sel_row(d, v), at;
    if (s < 0) {
        return 0u;
    }
    if (v->rows[s].kind == (uint32_t)SEL_ROW_FOLDER) {
        return v->rows[s].id;
    }
    for (at = s - 1; at >= 0; at--) {
        if (v->rows[at].kind == (uint32_t)SEL_ROW_FOLDER && v->rows[at].depth < v->rows[s].depth) {
            return v->rows[at].id;
        }
    }
    return 0u;
}

static uint32_t pl_hash(const sel_view_state* v) {
    uint32_t h = 2166136261u, i;
    const char* p;
    for (i = 0; v != NULL && i < v->row_n; i++) {
        h = (h ^ v->rows[i].id) * 16777619u;
        h = (h ^ v->rows[i].kind) * 16777619u;
        h = (h ^ v->rows[i].depth) * 16777619u;
        for (p = v->rows[i].text; p != NULL && *p != '\0'; p++) {
            h = (h ^ (uint32_t)(unsigned char)*p) * 16777619u;
        }
    }
    return h;
}

static const char* pl_button_label(uint32_t k) {
    return k == 0u ? "New Folder" : k == 1u ? "New Playlist" : "Play";
}

/* ---- The playlist page (I633): the Playlists page shows one saved playlist as a Spotify-style page: cover, name, song count,
 * a Play button and the songs as shared song rows (song_row_reg, so the song menu, keyboard focus and drag work). The keys
 * come from the library (sel_engine_library_playlist_keys); nothing here reads SQL. ---- */
static void col_rebuild(desk* d);
static void col_cover_refresh(desk* d);

#define COL_KIND_ID 0xFFFFFFFFu /* the pls_id of an album, artist or composer page: no playlist has it */
#define COL_LIKED 0x100u        /* I678g: col_kind of Liked Songs; its engine kind SEL_LIB_ITEM_LIKED is 0, which col_kind uses for a playlist */

static uint32_t col_ui_kind(uint32_t eng_kind) { return eng_kind == (uint32_t)SEL_LIB_ITEM_LIKED ? COL_LIKED : eng_kind; }
static uint32_t col_eng_kind(const desk* d) { return d->col_kind == COL_LIKED ? (uint32_t)SEL_LIB_ITEM_LIKED : d->col_kind; }
/* The three pinned rows show the wordmark header instead of a cover (design section 8). */
static int col_wordmark(const desk* d) {
    return d->col_kind == COL_LIKED || d->col_kind == (uint32_t)SEL_LIB_ITEM_RECENT || d->col_kind == (uint32_t)SEL_LIB_ITEM_LOCAL;
}
enum { COL_MAX_SONGS = 8192, COL_SHELF_MAX = 24 };

static void pls_free(desk* d) {
    free(d->col_av);
    d->col_av = NULL;
    d->col_av_w = 0;
    free(d->col_plays);
    d->col_plays = NULL;
    free(d->pls_buf);
    free(d->pls_off);
    free(d->pls_title);
    free(d->pls_meta);
    d->pls_meta = NULL;
    free(d->pls_ord);
    d->pls_ord = NULL;
    d->pls_vn = 0u;
    d->pls_buf = NULL;
    d->pls_off = NULL;
    d->pls_title = NULL;
    d->pls_n = 0u;
}

static const char* pls_key(const desk* d, uint32_t i) {
    return d->pls_buf + d->pls_off[i];
}

static void pls_reload(desk* d) {
    enum { CAP = 262144 };
    char* buf = (char*)malloc(CAP);
    uint32_t n, i;
    const char* k;
    d->pls_stale = 0u;
    d->pls_ver++;
    pls_free(d);
    if (buf == NULL) {
        return;
    }
    if (d->col_kind != 0u) { /* I678f: an album, artist or composer: COLLECTION_OPEN made the page, the UI only reads state.collection */
        const sel_collection_page* pg = sel_engine_state(d->engine)->collection;
        uint32_t sh;
        n = 0u;
        buf[0] = '\0';
        if (pg == NULL || pg->kind != col_eng_kind(d) || strcmp(pg->key, d->col_key) != 0 || pg->serial == d->col_open_serial) {
            d->pls_stale = 1u; /* the engine has not built the page yet: ask again next frame */
        } else {
            const char* r;
            size_t len = 0;
            for (r = pg->rows, i = 0; i < pg->songs && *r != '\0'; i++) {
                len += strlen(r) + 1u;
                r += strlen(r) + 1u;
                r += strlen(r) + 1u;
            }
            free(buf);
            buf = (char*)malloc(len + 2u);
            d->col_plays = (uint32_t*)calloc(pg->songs + 1u, sizeof *d->col_plays);
            if (buf == NULL || d->col_plays == NULL) {
                free(buf);
                free(d->col_plays);
                d->col_plays = NULL;
                return;
            }
            for (r = pg->rows, k = buf, len = 0, i = 0; i < pg->songs && *r != '\0'; i++) {
                size_t kl = strlen(r) + 1u;
                memcpy(buf + len, r, kl);
                len += kl;
                r += kl;
                d->col_plays[i] = (uint32_t)strtoul(r, NULL, 10);
                r += strlen(r) + 1u;
                n++;
            }
            buf[len] = '\0';
            snprintf(d->pls_name, sizeof d->pls_name, "%s", pg->title);
            snprintf(d->col_sub, sizeof d->col_sub, "%s", pg->sub);
            d->col_year = pg->year;
            for (sh = 0; sh < 4u; sh++) {
                d->col_shelf_n[sh] = pg->shelf_n[sh];
                d->col_shelf_total[sh] = pg->shelf_total[sh];
                memcpy(d->col_shelf[sh], pg->shelf[sh], sizeof d->col_shelf[sh]);
            }
        }
    } else {
        n = sel_engine_library_playlist_keys(d->engine, d->pls_id, buf, CAP);
    }
    d->pls_off = (uint32_t*)malloc((n + 1u) * sizeof *d->pls_off);
    d->pls_title = (char(*)[96])malloc((n + 1u) * sizeof *d->pls_title);
    d->pls_meta = (pls_meta_row*)calloc(n + 1u, sizeof *d->pls_meta);
    if (d->pls_off == NULL || d->pls_title == NULL || d->pls_meta == NULL) {
        free(d->pls_off);
        free(d->pls_title);
        free(d->pls_meta);
        d->pls_off = NULL;
        d->pls_title = NULL;
        d->pls_meta = NULL;
        free(buf);
        return;
    }
    d->pls_buf = buf;
    for (i = 0, k = buf; i < n && *k != '\0'; i++, k += strlen(k) + 1u) {
        d->pls_off[i] = (uint32_t)(k - buf);
        if (!sel_engine_song_title(d->engine, k, d->pls_title[i], sizeof d->pls_title[i])) {
            snprintf(d->pls_title[i], sizeof d->pls_title[i], "%s", path_base(k));
        }
        {   /* I678h3: artist, album, album id, composer canon of the db row; the links route on these, never on UI name matching */
            char m[SEL_COMPOSER_MAX + 224];
            if (sel_engine_song_meta(d->engine, k, m, sizeof m)) {
                const char* alb = m + strlen(m) + 1u;
                const char* aid = alb + strlen(alb) + 1u;
                snprintf(d->pls_meta[i].artist, sizeof d->pls_meta[i].artist, "%s", m);
                snprintf(d->pls_meta[i].album, sizeof d->pls_meta[i].album, "%s", alb);
                d->pls_meta[i].album_id = (uint32_t)strtoul(aid, NULL, 10);
            }
        }
    }
    d->pls_n = i;
    {   /* I691d (Q3): the page's cover is the majority family of its songs, like an album's */
        sel_platform_family_tally tally;
        memset(&tally, 0, sizeof tally);
        for (i = 0; i < d->pls_n; i++) {
            sel_platform_family_tally_add(&tally, pls_key(d, i));
        }
        d->pls_family = sel_platform_family_tally_majority(&tally);
    }
    if (d->pls_want != 0u && d->pls_n >= d->pls_want) { /* the engine has not applied the removal yet: ask again next frame */
        d->pls_stale = 1u;
    } else {
        d->pls_want = 0u;
    }
    if (d->pls_n < d->pls_min) { /* I652a: an Add has not been applied yet */
        d->pls_stale = 1u;
    } else {
        d->pls_min = 0u;
    }
    col_rebuild(d);
    col_cover_refresh(d);
    if (d->pls_scroll >= d->pls_vn) {
        d->pls_scroll = 0u;
    }
}

/* ---- I678e: the collection page (design doc 7): one page for a playlist now, an album, artist or composer later. The search
 * filter and the sort live here as an order over the loaded songs; the header colour is the dominant colour of the cover. ---- */
static const desk* g_col_d;

static int col_ci_cmp(const char* a, const char* b) {
    for (; *a != '\0' && *b != '\0'; a++, b++) {
        int x = tolower((unsigned char)*a), y = tolower((unsigned char)*b);
        if (x != y) {
            return x - y;
        }
    }
    return tolower((unsigned char)*a) - tolower((unsigned char)*b);
}

static int col_match(const char* title, const char* q) {
    size_t n = strlen(q), i, m = strlen(title);
    for (i = 0; i + n <= m; i++) {
        size_t j;
        for (j = 0; j < n && tolower((unsigned char)title[i + j]) == tolower((unsigned char)q[j]); j++) {
        }
        if (j == n) {
            return 1;
        }
    }
    return 0;
}

static int col_order_cmp(const void* a, const void* b) {
    uint32_t x = *(const uint32_t*)a, y = *(const uint32_t*)b;
    int c = col_ci_cmp(g_col_d->pls_title[x], g_col_d->pls_title[y]);
    return c != 0 ? c : (x < y ? -1 : x > y);
}

static void col_rebuild(desk* d) {
    uint32_t i, n = 0u;
    free(d->pls_ord);
    d->pls_ord = (uint32_t*)malloc((d->pls_n + 1u) * sizeof *d->pls_ord);
    d->pls_vn = 0u;
    if (d->pls_ord == NULL) {
        return;
    }
    for (i = 0; i < d->pls_n; i++) {
        if (d->col_qlen == 0u || col_match(d->pls_title[i], d->col_q)) {
            d->pls_ord[n++] = i;
        }
    }
    if (d->col_sort == 1u && n > 1u) {
        g_col_d = d;
        qsort(d->pls_ord, n, sizeof *d->pls_ord, col_order_cmp);
    }
    d->pls_vn = n;
}

/* The header colour: the dominant colour of the first song's cover, else the accent. The band fades from it into the panel. */
static void col_cover_refresh(desk* d) {
    uint32_t c = 0u;
    const char* key = d->pls_n > 0u ? pls_key(d, 0u) : NULL;
    d->col_cover = 0;
    if (key != NULL && d->art != NULL) {
        art_load(d->art, &key, 1u);
        d->col_cover = art_dominant(d->art, key, &c);
    }
    d->col_color = d->col_cover ? c : ARGB(d->theme.token[SEL_GUI_TOKEN_ACCENT]);
    d->col_top = desk_mix(ARGB(d->theme.token[SEL_GUI_TOKEN_PANEL]), d->col_color, 40000);
}

static void col_changed(desk* d) {
    col_rebuild(d);
    d->pls_scroll = 0u;
    d->repaint = 1;
    d->uim.dirty |= SEL_UIM_DIRTY_ALL;
}

/* Opens playlist `id` on the Playlists page (the sidebar Your Library row and a playlist row of the tree). */
static void pls_open(desk* d, uint32_t id, const char* name) {
    d->pls_id = id;
    d->col_kind = 0u;
    d->col_focus = -1;
    d->pls_want = 0u;
    d->pls_min = 0u;
    d->pls_scroll = 0u;
    d->qlen = 0u; /* I652a: the song search of an empty playlist starts empty */
    d->query[0] = '\0';
    d->search_on = 0;
    d->search_pending = 0;
    snprintf(d->pls_name, sizeof d->pls_name, "%s", name != NULL ? name : "Playlist");
    d->pl_sel_kind = (uint32_t)SEL_ROW_PLAYLIST;
    d->pl_sel_id = id;
    d->pl_edit = 0;
    d->col_sort = 0u; /* I678e: a collection opens in its own order, unfiltered */
    d->col_qlen = 0u;
    d->col_q[0] = '\0';
    d->col_search = 0;
    pls_reload(d);
    d->repaint = 1;
    d->uim.dirty |= SEL_UIM_DIRTY_ALL;
}

/* I647: Rename from a menu: the playlist's page opens and its title is the name field with the keyboard (as Create playlist, I646). */
static void pl_rename_begin(desk* d, uint32_t id, const char* name) {
    if (d->page != PG_COLLECTION) {
        desk_page_goto(d, PG_COLLECTION);
    }
    if (d->pls_id != id) {
        pls_open(d, id, name);
    }
    d->pl_nlen = 0u;
    d->pl_name[0] = '\0';
    d->pl_edit = 1;
    d->repaint = 1;
    d->uim.dirty |= SEL_UIM_DIRTY_ALL;
}

static void pls_close(desk* d) {
    d->pls_id = 0u;
    d->col_kind = 0u;
    d->pls_ver++;
    pls_free(d);
    d->repaint = 1;
    d->uim.dirty |= SEL_UIM_DIRTY_ALL;
}

enum { PLS_COVER = 160, PLS_GAP = 24 };

static sel_gui_rect pls_back_rect(desk* d, sel_gui_rect mn) {
    return rect_of(mn.x + lpx(d, HOME_MARGIN), mn.y + lpx(d, 16), lpx(d, QUEUE_BTN_W), lpx(d, QUEUE_BTN_H));
}

static sel_gui_rect pls_cover_rect(desk* d, sel_gui_rect mn) {
    sel_gui_rect b = pls_back_rect(d, mn);
    return rect_of(b.x, b.y + b.h + lpx(d, 16), lpx(d, PLS_COVER), lpx(d, PLS_COVER));
}

static sel_gui_rect pls_play_rect(desk* d, sel_gui_rect mn) {
    sel_gui_rect c = pls_cover_rect(d, mn);
    return rect_of(c.x + c.w + lpx(d, PLS_GAP), c.y + c.h - lpx(d, QUEUE_BTN_H), lpx(d, QUEUE_BTN_W), lpx(d, QUEUE_BTN_H));
}

/* I647: More, beside Play: the menu with Rename and Delete. */
static sel_gui_rect pls_more_rect(desk* d, sel_gui_rect mn) {
    sel_gui_rect pb = pls_play_rect(d, mn);
    return rect_of(pb.x + pb.w + lpx(d, QUEUE_BTN_GAP), pb.y, pb.w, pb.h);
}

/* I678e: the action row's sort button and search field, right of More (shown while the collection has songs). */
static sel_gui_rect col_sort_rect(desk* d, sel_gui_rect mn) {
    sel_gui_rect m = pls_more_rect(d, mn);
    return rect_of(m.x + m.w + lpx(d, QUEUE_BTN_GAP), m.y, m.w + lpx(d, 48), m.h);
}

static sel_gui_rect col_find_rect(desk* d, sel_gui_rect mn) {
    sel_gui_rect b = pls_back_rect(d, mn); /* right end of the Back row: the action row has no room left of the content edge */
    return rect_of(mn.x + mn.w - lpx(d, HOME_MARGIN) - lpx(d, 220), b.y, lpx(d, 220), b.h);
}

static sel_gui_rect pls_row_rect(desk* d, sel_gui_rect mn, uint32_t slot) {
    sel_gui_rect c = pls_cover_rect(d, mn);
    return rect_of(c.x, c.y + c.h + lpx(d, PLS_GAP) + (int32_t)slot * lpx(d, SONG_ROW_H), mn.w - 2 * lpx(d, HOME_MARGIN), lpx(d, SONG_ROW_H));
}

static int pls_row_visible(desk* d, sel_gui_rect mn, uint32_t slot) {
    return pls_row_rect(d, mn, slot).y + lpx(d, SONG_ROW_H) <= mn.y + mn.h;
}

/* ---- I678f: the album, artist and composer pages (design doc 8): the same collection page with a kind. The songs are the page's
 * rows (an artist's Popular is the first 5, 10 after Show more); the shelves (Albums, Appears On, More by, Spellings) follow the
 * rows as cards, laid out in the same row slots so one scroll position moves everything. ---- */
enum { COL_CARD = 128, COL_CARD_GAP = 16, COL_SHELF_SLOTS = 7 };

static const char* col_kind_label(const desk* d) {
    return d->col_kind == (uint32_t)SEL_LIB_ITEM_ALBUM      ? "Album"
           : d->col_kind == (uint32_t)SEL_LIB_ITEM_ARTIST   ? "Artist"
           : d->col_kind == (uint32_t)SEL_LIB_ITEM_COMPOSER ? "Composer"
           : d->col_kind == (uint32_t)SEL_LIB_ITEM_SOURCE   ? "Source"
           : d->col_kind == (uint32_t)SEL_LIB_ITEM_LOCAL    ? "Source"
                                                            : "Playlist";
}

static uint32_t col_song_rows(const desk* d) {
    return d->col_kind == (uint32_t)SEL_LIB_ITEM_ARTIST && d->pls_vn > d->col_popular ? d->col_popular : d->pls_vn;
}

static int col_show_more(const desk* d) {
    return d->col_kind == (uint32_t)SEL_LIB_ITEM_ARTIST && d->col_popular == 5u && d->pls_vn > 5u;
}

/* The first virtual row slot of shelf `sh` (0 Albums, 1 Appears On, 2 More by, 3 Spellings), or -1 when the shelf is empty. */
static int32_t col_shelf_slot(const desk* d, uint32_t sh) {
    int32_t slot = (int32_t)col_song_rows(d) + (col_show_more(d) ? 1 : 0);
    uint32_t i;
    for (i = 0; i < 4u; i++) {
        if (d->col_shelf_n[i] == 0u) {
            continue;
        }
        if (i == sh) {
            return slot;
        }
        slot += i == 3u ? 2 : COL_SHELF_SLOTS;
    }
    return -1;
}

static uint32_t col_rows(const desk* d) {
    int32_t end = (int32_t)col_song_rows(d) + (col_show_more(d) ? 1 : 0);
    uint32_t i;
    if (d->col_kind == 0u) {
        return d->pls_vn;
    }
    for (i = 0; i < 4u; i++) {
        end += d->col_shelf_n[i] == 0u ? 0 : (i == 3u ? 2 : COL_SHELF_SLOTS);
    }
    return (uint32_t)end;
}

static const char* col_shelf_title(const desk* d, uint32_t sh, char* buf, size_t cap) {
    if (sh == 0u) {
        return "Albums";
    }
    if (sh == 1u) {
        return "Appears On";
    }
    if (sh == 2u) {
        snprintf(buf, cap, "More by %s", d->col_sub);
        return buf;
    }
    return "Spellings";
}

/* The cover rect of card `i` of shelf `sh`; the heading sits one slot above. */
static sel_gui_rect col_card_rect(desk* d, sel_gui_rect mn, uint32_t sh, uint32_t i) {
    int32_t slot = col_shelf_slot(d, sh) + 1 - (int32_t)d->pls_scroll;
    sel_gui_rect r = pls_row_rect(d, mn, 0u);
    return rect_of(r.x + (int32_t)i * (lpx(d, COL_CARD) + lpx(d, COL_CARD_GAP)), r.y + slot * lpx(d, SONG_ROW_H), lpx(d, COL_CARD), lpx(d, COL_CARD));
}

static uint32_t col_cards_fit(desk* d, sel_gui_rect mn) {
    uint32_t n = (uint32_t)((mn.w - 2 * lpx(d, HOME_MARGIN)) / (lpx(d, COL_CARD) + lpx(d, COL_CARD_GAP)));
    return n == 0u ? 1u : n;
}

static sel_gui_rect col_more_rect(desk* d, sel_gui_rect mn) { /* the Show more row, one slot after the Popular rows */
    sel_gui_rect r = pls_row_rect(d, mn, 0u);
    return rect_of(r.x, r.y + ((int32_t)col_song_rows(d) - (int32_t)d->pls_scroll) * lpx(d, SONG_ROW_H), lpx(d, 160), lpx(d, SONG_ROW_H));
}

static void desk_quoted_request(char* line, size_t cap, const char* head, const char* arg);
static void col_kind_open(desk* d, uint32_t kind, const char* key, const char* name, const char* sub, int restore) {
    pls_open(d, COL_KIND_ID, name);
    d->col_kind = col_ui_kind(kind);
    d->col_popular = 5u;
    memset(d->col_shelf_n, 0, sizeof d->col_shelf_n);
    snprintf(d->col_key, sizeof d->col_key, "%s", key != NULL ? key : "");
    snprintf(d->col_sub, sizeof d->col_sub, "%s", sub != NULL ? sub : "");
    d->col_year = 0u;
    {
        const sel_collection_page* pg = sel_engine_state(d->engine)->collection;
        d->col_open_serial = pg != NULL ? pg->serial : 0u;
    }
    { /* every open of an album, artist or composer page is one COLLECTION_OPEN: it feeds Recents and makes the page (I678f) */
        char open[160];
        char head[32];
        snprintf(head, sizeof head, "COLLECTION_OPEN %u", (unsigned)kind);
        desk_quoted_request(open, sizeof open, head, d->col_key);
        if (restore) { /* Back and Forward rebuild the page without touching Recents (I678h2) */
            size_t n = strlen(open);
            snprintf(open + n, sizeof open - n, " 1");
        }
        desk_send(d, open);
    }
    if (kind == (uint32_t)SEL_LIB_ITEM_SOURCE) { /* I678g: the source status block reads the registered sources */
        d->src_wait = 1;
        desk_send(d, "SOURCES");
    }
    pls_reload(d);
    d->pl_sel_kind = 0u;
    d->pl_sel_id = 0u;
}

static void col_draw_shelves(desk* d, sel_gui_rect mn, uint32_t text, uint32_t muted) {
    uint32_t sh, i;
    char buf[160];
    if (col_show_more(d)) {
        sel_gui_rect r = col_more_rect(d, mn);
        if (r.y + r.h <= mn.y + mn.h) {
            draw_text(d, rect_of(r.x + lpx(d, 12), r.y, r.w, r.h), "Show more", 1, 14, text);
        }
    }
    for (sh = 0; sh < 4u; sh++) {
        int32_t abs_slot = col_shelf_slot(d, sh);
        int32_t slot = abs_slot - (int32_t)d->pls_scroll;
        sel_gui_rect hr;
        if (abs_slot < 0 || slot < 0) {
            continue;
        }
        hr = pls_row_rect(d, mn, (uint32_t)slot);
        if (hr.y + hr.h > mn.y + mn.h) {
            continue;
        }
        draw_text(d, hr, col_shelf_title(d, sh, buf, sizeof buf), 1, 18, text);
        if (sh == 3u) { /* Spellings: one note line, the names the composer is written under */
            size_t o = 0;
            buf[0] = '\0';
            for (i = 0; i < d->col_shelf_n[sh] && o + 2u < sizeof buf; i++) {
                o += (size_t)snprintf(buf + o, sizeof buf - o, "%s%s", i ? ", " : "", d->col_shelf[sh][i].name);
            }
            draw_text(d, pls_row_rect(d, mn, (uint32_t)slot + 1u), buf, 0, 13, muted);
            continue;
        }
        for (i = 0; i < d->col_shelf_n[sh] && i < col_cards_fit(d, mn); i++) {
            const sel_engine_collection_row* c = &d->col_shelf[sh][i];
            sel_gui_rect cr = col_card_rect(d, mn, sh, i);
            char yr[32];
            uint32_t pcs; /* the shelf card never drew real art (a known gap, not this row's) */
            family_platform_cover(d, d->canvas, cr, sel_platform_of_ext(c->format), c->name, &pcs);
            draw_text(d, rect_of(cr.x, cr.y + cr.h + lpx(d, 2), cr.w, lpx(d, 20)), c->name, 0, 13, text);
            if (c->year != 0u) {
                snprintf(yr, sizeof yr, "%u", (unsigned)c->year);
            } else {
                snprintf(yr, sizeof yr, "%u %s", (unsigned)c->songs, c->songs == 1u ? "song" : "songs");
            }
            draw_text(d, rect_of(cr.x, cr.y + cr.h + lpx(d, 22), cr.w, lpx(d, 18)), yr, 0, 12, muted);
        }
    }
}

/* A click on a card opens that album's page; on Show more extends Popular to 10. Returns 1 when it took the click. */
static int col_kind_click(desk* d, sel_gui_rect mn, int32_t x, int32_t y) {
    uint32_t sh, i;
    if (col_show_more(d) && in_rect(col_more_rect(d, mn), x, y)) {
        d->col_popular = 10u;
        d->repaint = 1;
        d->uim.dirty |= SEL_UIM_DIRTY_ALL;
        return 1;
    }
    for (sh = 0; sh < 3u; sh++) {
        for (i = 0; col_shelf_slot(d, sh) >= 0 && i < d->col_shelf_n[sh] && i < col_cards_fit(d, mn); i++) {
            if (in_rect(col_card_rect(d, mn, sh, i), x, y)) {
                const sel_engine_collection_row* c = &d->col_shelf[sh][i];
                char key[16];
                char name[96], sub[96];
                snprintf(key, sizeof key, "%u", (unsigned)c->id);
                snprintf(name, sizeof name, "%s", c->name);
                snprintf(sub, sizeof sub, "%s", c->artist);
                desk_route(d, SEL_ROUTE_ALBUM_CARD, 0u, c->id, key, name, sub);
                return 1;
            }
        }
    }
    return 0;
}

/* Play on a kind page: the first song plays and every other song of the collection queues behind it (queue_count = N). Shuffle
 * plays the same songs in a random order (Fisher-Yates over the indexes). */
static int col_kind_play(desk* d, int shuffle) {
    uint32_t i, n = d->pls_n, *ord, seed;
    if (n == 0u) {
        return 1;
    }
    ord = (uint32_t*)malloc(n * sizeof *ord);
    if (ord == NULL) {
        return 1;
    }
    for (i = 0; i < n; i++) {
        ord[i] = i;
    }
    if (shuffle) {
        seed = (uint32_t)time(NULL) ^ (n * 2654435761u) ^ 0x9E3779B9u;
        for (i = n - 1u; i > 0u; i--) {
            uint32_t j, t;
            seed ^= seed << 13;
            seed ^= seed >> 17;
            seed ^= seed << 5;
            j = seed % (i + 1u);
            t = ord[i];
            ord[i] = ord[j];
            ord[j] = t;
        }
    }
    snprintf(d->np_ctx, sizeof d->np_ctx, "%s", d->pls_name);
    desk_play_path(d, pls_key(d, ord[0]));
    for (i = 1; i < n; i++) {
        desk_send_path(d, "QUEUE_ADD", pls_key(d, ord[i]));
    }
    free(ord);
    d->repaint = 1;
    return 1;
}

/* I646: the playlist's name, drawn big; a click makes it the name field (Enter renames, Escape keeps the name). */
static sel_gui_rect pls_title_rect(desk* d, sel_gui_rect mn) {
    sel_gui_rect cv = pls_cover_rect(d, mn);
    int32_t tx = cv.x + cv.w + lpx(d, PLS_GAP);
    return rect_of(tx, cv.y + lpx(d, 24), mn.x + mn.w - tx - lpx(d, HOME_MARGIN), lpx(d, page_title_h(d)));
}

/* I652a: the empty playlist page offers "Let's find something for your playlist": a search field and, under it, the songs the
 * library search found, each with an Add button. The field is the desktop's one search field (d->query, desk_search_fire);
 * the room under it is kept for the import buttons of I652b-d. */
enum { PLS_FIND_HEAD_H = 32, PLS_FIND_FIELD_H = 40, PLS_FIND_BUTTONS_H = 36, PLS_FIND_ADD_W = 80, PLS_FIND_ADD_H = 28 };

static sel_gui_rect pls_find_head_rect(desk* d, sel_gui_rect mn) {
    sel_gui_rect c = pls_cover_rect(d, mn);
    return rect_of(c.x, c.y + c.h + lpx(d, PLS_GAP), mn.w - 2 * lpx(d, HOME_MARGIN), lpx(d, PLS_FIND_HEAD_H));
}

static sel_gui_rect pls_find_field_rect(desk* d, sel_gui_rect mn) {
    sel_gui_rect h = pls_find_head_rect(d, mn);
    int32_t w = h.w < lpx(d, HOME_SEARCH_MAX) ? h.w : lpx(d, HOME_SEARCH_MAX);
    return rect_of(h.x, h.y + h.h + lpx(d, 8), w, lpx(d, PLS_FIND_FIELD_H));
}

static sel_gui_rect pls_find_row_rect(desk* d, sel_gui_rect mn, uint32_t slot) {
    sel_gui_rect f = pls_find_field_rect(d, mn);
    return rect_of(f.x, f.y + f.h + lpx(d, 8 + PLS_FIND_BUTTONS_H + 16) + (int32_t)slot * lpx(d, SONG_ROW_H), mn.w - 2 * lpx(d, HOME_MARGIN), lpx(d, SONG_ROW_H));
}

/* The Add control at the right end of a song row: the empty playlist's results (I652a) and the Library rows in add mode (I652b). */
static sel_gui_rect add_btn_rect(desk* d, sel_gui_rect r) {
    return rect_of(r.x + r.w - lpx(d, PLS_FIND_ADD_W), r.y + (r.h - lpx(d, PLS_FIND_ADD_H)) / 2, lpx(d, PLS_FIND_ADD_W), lpx(d, PLS_FIND_ADD_H));
}

static int add_mode(const desk* d) {
    return d->add_id != 0u && d->page == PG_FOLDERS;
}

static sel_gui_rect pls_find_add_rect(desk* d, sel_gui_rect mn, uint32_t slot) {
    return add_btn_rect(d, pls_find_row_rect(d, mn, slot));
}

/* I652b: the button strip under the search field (room: PLS_FIND_BUTTONS_H); slot 0 Add from Library, slot 1 Import playlist file (I652c), slot 2 Add Folder (I652d). */
enum { PLS_FIND_BTN_W = 200, PLS_FIND_BTN_H = 32 };
static sel_gui_rect pls_find_btn_rect(desk* d, sel_gui_rect mn, uint32_t slot) {
    sel_gui_rect f = pls_find_field_rect(d, mn);
    return rect_of(f.x + (int32_t)slot * (lpx(d, PLS_FIND_BTN_W) + lpx(d, QUEUE_BTN_GAP)), f.y + f.h + lpx(d, 8), lpx(d, PLS_FIND_BTN_W), lpx(d, PLS_FIND_BTN_H));
}

static int pls_find_row_visible(desk* d, sel_gui_rect mn, uint32_t slot) {
    return pls_find_row_rect(d, mn, slot).y + lpx(d, SONG_ROW_H) <= mn.y + mn.h;
}

/* The slot-th song of the engine's search result (the Songs group of the search view, the rows the Search page draws), or NULL. */
static const sel_view_row* pls_find_result(desk* d, uint32_t slot) {
    const sel_view_state* v = sel_engine_state(d->engine)->view;
    uint32_t k, group = 0, n = 0;
    if (!d->search_on || v == NULL || v->source != SEL_VIEW_SEARCH) {
        return NULL;
    }
    for (k = 0; k < v->row_n; k++) {
        const sel_view_row* r = &v->rows[k];
        const char* t = r->text != NULL ? r->text : "";
        if (r->kind == SEL_ROW_HEADING) {
            group = strncmp(t, "Songs", 5) == 0 ? 1u : 2u;
        } else if (group == 1u && !r->album && r->key != NULL && n++ == slot) {
            return r;
        }
    }
    return NULL;
}

/* A result row's text is "<title>  <artist>" (two spaces): copies the title and points *artist at the rest. */
static void pls_find_split(const char* text, char* title, size_t cap, const char** artist) {
    const char* sep = strstr(text, "  ");
    size_t n = sep != NULL ? (size_t)(sep - text) : strlen(text);
    if (n >= cap) {
        n = cap - 1u;
    }
    memcpy(title, text, n);
    title[n] = '\0';
    *artist = sep != NULL ? sep + 2 : "";
}

static void draw_playlist_find(desk* d, sel_gui_rect mn) {
    uint32_t text = ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT]), muted = ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT_MUTED]);
    uint32_t surf = ARGB(d->theme.token[SEL_GUI_TOKEN_SURFACE]), accent = ARGB(d->theme.token[SEL_GUI_TOKEN_ACCENT]);
    sel_gui_rect sr = pls_find_field_rect(d, mn);
    int32_t ic = lpx(d, 18), rad = lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_RADIUS_LARGE]);
    uint32_t slot;
    draw_text(d, pls_find_head_rect(d, mn), "Let's find something for your playlist", 1, 22, text);
    sel_gui_round_rect(d->canvas, sr, rad, surf);
    sel_gui_icon_draw(d->canvas, &d->icons, SEL_GUI_ICON_SEARCH, rect_of(sr.x + lpx(d, 14), sr.y + (sr.h - ic) / 2, ic, ic), ic, muted);
    desk_draw_query(d, sr, "Search for songs");
    draw_button(d, pls_find_btn_rect(d, mn, 0u), surf, "Add from Library", 13, text);
    draw_button(d, pls_find_btn_rect(d, mn, 1u), surf, "Import playlist file", 13, text);
    draw_button(d, pls_find_btn_rect(d, mn, 2u), surf, "Add Folder", 13, text);
    d->pls_find_shown = 0;
    for (slot = 0; pls_find_row_visible(d, mn, slot); slot++) {
        const sel_view_row* r = pls_find_result(d, slot);
        sel_gui_rect rr = pls_find_row_rect(d, mn, slot);
        char title[96];
        const char* artist;
        if (r == NULL) {
            break;
        }
        pls_find_split(r->text != NULL ? r->text : "", title, sizeof title, &artist);
        draw_text(d, rect_of(rr.x + lpx(d, 12), rr.y, rr.w / 2 - lpx(d, 12), rr.h), title, 0, 14, text);
        draw_text(d, rect_of(rr.x + rr.w / 2, rr.y, rr.w / 2 - lpx(d, PLS_FIND_ADD_W + 16), rr.h), artist, 0, 13, muted);
        draw_button(d, pls_find_add_rect(d, mn, slot), accent, "Add", 13, ARGB(d->theme.token[SEL_GUI_TOKEN_ACCENT_TEXT]));
        d->pls_find_shown++;
    }
}

/* Add on result `slot`: the song key goes to PLAYLIST_ADD_SONG with this playlist's id, whatever source the key names (I653). */
static void pls_add_key(desk* d, uint32_t pid, const char* key) {
    char line[640];
    snprintf(line, sizeof line, "PLAYLIST_ADD_SONG %u \"%s\"", (unsigned)pid, key);
    desk_send(d, line);
    d->pls_adds++;
    d->pls_min = d->pls_n + 1u;
    d->pls_stale = 1u;
    d->side_stale = 1u;
    d->qlen = 0u;
    d->query[0] = '\0';
    d->search_on = 0;
    d->focus = -1;
    d->repaint = 1;
    d->uim.dirty |= SEL_UIM_DIRTY_ALL;
    desk_toast(d, "Added to playlist");
}

static void pls_find_add(desk* d, uint32_t slot) {
    const sel_view_row* r = pls_find_result(d, slot);
    if (r != NULL) {
        pls_add_key(d, d->pls_id, r->key);
    }
}

/* I652b: Add from Library opens the Library page in add mode for this playlist; Back returns to the playlist's page. */
static sel_gui_rect add_back_rect(desk* d, sel_gui_rect mn) {
    char label[160];
    sel_gui_rect ab = lib_add_rect(d, mn);
    int32_t w;
    snprintf(label, sizeof label, "Back to %s", d->add_name);
    w = lib_title_btn_w(d, label);
    return rect_of(ab.x + ab.w - w, ab.y, w, ab.h);
}

static void add_mode_open(desk* d) {
    uint32_t id = d->pls_id;
    char name[sizeof d->add_name];
    snprintf(name, sizeof name, "%s", d->pls_name);
    desk_page_goto(d, PG_FOLDERS);
    d->add_id = id;
    snprintf(d->add_name, sizeof d->add_name, "%s", name);
    d->repaint = 1;
    d->uim.dirty |= SEL_UIM_DIRTY_ALL;
}

static void add_mode_back(desk* d) {
    uint32_t id = d->add_id;
    char name[sizeof d->add_name];
    snprintf(name, sizeof name, "%s", d->add_name);
    desk_page_goto(d, PG_COLLECTION);
    pls_open(d, id, name);
}

/* D6: the artist / composer avatar is the generated art of the name inside a circle. The art is drawn once into an offscreen
 * tile (family_platform_cover) and blitted through the rounded-rect mask with radius = half the edge. */
static void col_round_art(desk* d, sel_gui_rect r) {
    uint32_t hash = sel_gui_str_hash(d->pls_name);
    if (r.w <= 0 || r.h != r.w) {
        return;
    }
    hash += d->pls_family * 0x9E3779B1u; /* the family is part of the cache identity: the page's songs may change it */
    if (d->col_av == NULL || d->col_av_w != r.w || d->col_av_hash != hash) {
        sel_gui_canvas tile;
        free(d->col_av);
        d->col_av = (uint32_t*)calloc((size_t)r.w * (size_t)r.w, sizeof *d->col_av);
        if (d->col_av == NULL) {
            return;
        }
        sel_gui_canvas_init(&tile, SEL_GUI_RGBA32, r.w, r.w, d->col_av);
        family_platform_cover(d, &tile, rect_of(0, 0, r.w, r.w), d->pls_family, d->pls_name, &d->pc_header);
        d->col_av_w = r.w;
        d->col_av_hash = hash;
    }
    sel_gui_blit_image_round(d->canvas, r, d->col_av, r.w, r.w, r.w, r.w / 2);
}

/* I678g: the source status block of a source page: the sync state and the same switch and Sync button as the Library Sources tab
 * (src_toggle_send, src_sync_send). col_src_index is the registered source the page shows, d->src_n when the engine does not know it. */
static int src_toggle_send(desk* d, uint32_t flip);
static int src_remote(const desk* d, uint32_t i);

static uint32_t col_src_index(const desk* d) {
    uint32_t i;
    if (d->col_kind != (uint32_t)SEL_LIB_ITEM_SOURCE) {
        return d->src_n;
    }
    for (i = 0; i < d->src_n; i++) {
        if (strcmp(d->src[i].key, d->col_key) == 0 || strcmp(d->src[i].name, d->col_key) == 0) {
            return i;
        }
    }
    return d->src_n;
}

/* The row the toggle and Sync button are laid out in: under the count line, right edge on the content margin. */
static sel_gui_rect col_src_row(desk* d, sel_gui_rect mn) {
    sel_gui_rect cv = pls_cover_rect(d, mn);
    return rect_of(mn.x, cv.y + lpx(d, 44) + lpx(d, page_title_h(d)), mn.w - lpx(d, HOME_MARGIN) + lpx(d, 16), lpx(d, 40));
}

static void col_src_status(const desk* d, uint32_t i, char* out, size_t cap) {
    if (i >= d->src_n) {
        snprintf(out, cap, "Not a registered source");
    } else if (!d->src[i].enabled) {
        snprintf(out, cap, "%s, switched off", src_remote(d, i) ? "Remote" : "Local");
    } else if (!src_remote(d, i)) {
        snprintf(out, cap, "Local, switched on");
    } else if (d->src[i].synced > 0) {
        snprintf(out, cap, "Remote, last synced %lld days ago", (long long)((time(NULL) - d->src[i].synced) / 86400));
    } else {
        snprintf(out, cap, "Remote, never synced");
    }
}

static void draw_col_src(desk* d, sel_gui_rect mn, uint32_t muted) {
    uint32_t i = col_src_index(d);
    sel_gui_rect r = col_src_row(d, mn), tg = src_toggle_rect(d, r), kn;
    int32_t kd = tg.h - lpx(d, 6), tx = pls_cover_rect(d, mn).x + pls_cover_rect(d, mn).w + lpx(d, PLS_GAP);
    char line[64];
    col_src_status(d, i, line, sizeof line);
    draw_text(d, rect_of(tx, r.y, src_sync_rect(d, r).x - tx, r.h), line, 0, 13, muted);
    if (i < d->src_n) {
        int on = d->src[i].enabled;
        if (src_remote(d, i) && on) {
            draw_button(d, src_sync_rect(d, r), ARGB(d->theme.token[SEL_GUI_TOKEN_TRACK]), "Sync", 14, ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT]));
        }
        sel_gui_round_rect(d->canvas, tg, tg.h / 2, on ? ARGB(d->theme.token[SEL_GUI_TOKEN_ACCENT]) : ARGB(d->theme.token[SEL_GUI_TOKEN_TRACK]));
        kn = rect_of(on ? tg.x + tg.w - kd - lpx(d, 3) : tg.x + lpx(d, 3), tg.y + lpx(d, 3), kd, kd);
        sel_gui_round_rect(d->canvas, kn, kd / 2, on ? ARGB(d->theme.token[SEL_GUI_TOKEN_ACCENT_TEXT]) : muted);
    }
}

static int col_src_click(desk* d, sel_gui_rect mn, int32_t x, int32_t y) {
    uint32_t i = col_src_index(d);
    sel_gui_rect r = col_src_row(d, mn);
    if (i >= d->src_n) {
        return 0;
    }
    if (in_rect(src_toggle_rect(d, r), x, y)) {
        return src_toggle_send(d, i);
    }
    if (src_remote(d, i) && d->src[i].enabled && in_rect(src_sync_rect(d, r), x, y)) {
        src_sync_send(d, i);
        d->src_wait = 1;
        return desk_send(d, "SOURCES");
    }
    return 0;
}

/* I678h3: a collection page song row is the number, the title, the artist link, the album link and (on a kind page) the Plays column. The
 * title takes 45 percent of the room left of the Plays column, the artist 28 percent, the album the rest. */
static int32_t pls_cols_w(desk* d, sel_gui_rect r) {
    int32_t w = r.w - lpx(d, 56) - lpx(d, 180);
    return w > 0 ? w : 0;
}

static int32_t pls_title_w(desk* d, sel_gui_rect r) {
    return pls_cols_w(d, r) * 45 / 100;
}

/* The link rects of song `i` on row `r`: the text width, capped at its column; zero width when the song has no artist or album. */
static void pls_link_rects(desk* d, sel_gui_rect r, uint32_t i, sel_gui_rect* art, sel_gui_rect* alb) {
    const sel_gui_atlas* at = desk_font(d, 0, 13);
    int32_t cw = pls_cols_w(d, r), ax = r.x + lpx(d, 56) + pls_title_w(d, r), aw = cw * 28 / 100, bx = ax + aw, bw = cw - pls_title_w(d, r) - aw, tw;
    memset(art, 0, sizeof *art);
    memset(alb, 0, sizeof *alb);
    if (d->pls_meta == NULL || at == NULL) {
        return;
    }
    if (d->pls_meta[i].artist[0] != '\0') {
        tw = sel_gui_text_width(at, d->pls_meta[i].artist, strlen(d->pls_meta[i].artist));
        *art = rect_of(ax, r.y, tw < aw - lpx(d, 8) ? tw : aw - lpx(d, 8), r.h);
    }
    if (d->pls_meta[i].album[0] != '\0') {
        tw = sel_gui_text_width(at, d->pls_meta[i].album, strlen(d->pls_meta[i].album));
        *alb = rect_of(bx, r.y, tw < bw - lpx(d, 8) ? tw : bw - lpx(d, 8), r.h);
    }
}

static void draw_playlist_page(desk* d, sel_gui_rect mn) {
    uint32_t text = ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT]), muted = ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT_MUTED]);
    uint32_t surf = ARGB(d->theme.token[SEL_GUI_TOKEN_SURFACE]), accent = ARGB(d->theme.token[SEL_GUI_TOKEN_ACCENT]);
    sel_gui_rect cv = pls_cover_rect(d, mn), pb = pls_play_rect(d, mn);
    int32_t tx = cv.x + cv.w + lpx(d, PLS_GAP), rad = lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_RADIUS]);
    char count[48];
    uint32_t slot, shown = 0;
    if (d->pls_stale) {
        pls_reload(d);
    }
    sel_gui_gradient(d->canvas, rect_of(mn.x, mn.y, mn.w, cv.y + cv.h + lpx(d, PLS_GAP) - mn.y), 1, d->col_top, ARGB(d->theme.token[SEL_GUI_TOKEN_PANEL]));
    draw_button(d, pls_back_rect(d, mn), surf, d->col_kind != 0u ? "Back" : "Back to Playlists", 13, text);
    if (d->col_kind == (uint32_t)SEL_LIB_ITEM_ARTIST || d->col_kind == (uint32_t)SEL_LIB_ITEM_COMPOSER) { /* D6: a round avatar in the cover colour */
        col_round_art(d, cv);
    } else if (col_wordmark(d)) { /* I678g: Liked Songs, Recently Played, Local Files: the large wordmark title in the accent colour, no cover art */
        sel_gui_round_rect(d->canvas, cv, rad, accent);
        draw_text(d, rect_of(cv.x + lpx(d, 12), cv.y, cv.w - lpx(d, 24), cv.h), d->pls_name, 1, 22, ARGB(d->theme.token[SEL_GUI_TOKEN_ACCENT_TEXT]));
    } else {
        family_platform_cover(d, d->canvas, cv, d->pls_family, d->pls_name, &d->pc_header);
    }
    draw_text(d, rect_of(tx, cv.y, mn.x + mn.w - tx - lpx(d, HOME_MARGIN), lpx(d, 20)), col_kind_label(d), 0, 13, muted);
    draw_text(d, pls_title_rect(d, mn), d->pl_edit && d->pl_nlen > 0u ? d->pl_name : d->pls_name, 1, 32, d->pl_edit && d->pl_nlen == 0u ? muted : text);
    if (d->pl_edit) { /* the caret: a thin bar where typing goes (the typed text's end, the start while the field is empty) */
        sel_gui_rect tr = pls_title_rect(d, mn);
        const sel_gui_atlas* at = desk_font(d, 1, 32);
        int32_t tw = at != NULL ? sel_gui_text_width(at, d->pl_name, d->pl_nlen) : 0, ch = tr.h - lpx(d, 8), cw = lpx(d, 2) > 0 ? lpx(d, 2) : 1;
        sel_gui_fill_rect(d->canvas, rect_of(tr.x + tw, tr.y + (tr.h - ch) / 2, cw, ch), text);
    }
    snprintf(count, sizeof count, "%u %s", (unsigned)d->pls_n, d->pls_n == 1u ? "song" : "songs");
    if (d->col_kind == (uint32_t)SEL_LIB_ITEM_ARTIST) {
        snprintf(count, sizeof count, "%u %s, %u %s", (unsigned)d->col_shelf_total[0], d->col_shelf_total[0] == 1u ? "album" : "albums", (unsigned)d->pls_n,
                 d->pls_n == 1u ? "song" : "songs");
    } else if (d->col_kind == (uint32_t)SEL_LIB_ITEM_ALBUM && d->col_sub[0] != '\0') {
        if (d->col_year != 0u) {
            snprintf(count, sizeof count, "%s, %u, %u %s", d->col_sub, (unsigned)d->col_year, (unsigned)d->pls_n, d->pls_n == 1u ? "song" : "songs");
        } else {
            snprintf(count, sizeof count, "%s, %u %s", d->col_sub, (unsigned)d->pls_n, d->pls_n == 1u ? "song" : "songs");
        }
    }
    memset(&d->col_artist, 0, sizeof d->col_artist);
    if (d->col_kind == (uint32_t)SEL_LIB_ITEM_ALBUM && d->col_sub[0] != '\0') { /* I678h: the artist name is a link to the artist page */
        const sel_gui_atlas* at = desk_font(d, 0, 13);
        size_t an = strlen(d->col_sub);
        int32_t aw = at != NULL ? sel_gui_text_width(at, d->col_sub, an) : 0, ty = cv.y + lpx(d, 24) + lpx(d, page_title_h(d));
        d->col_artist = rect_of(tx, ty, aw, lpx(d, 20));
        draw_text(d, d->col_artist, d->col_sub, 0, 13, text);
        draw_text(d, rect_of(tx + aw, ty, pb.w * 4, lpx(d, 20)), count + an, 0, 13, muted);
    } else {
        draw_text(d, rect_of(tx, cv.y + lpx(d, 24) + lpx(d, page_title_h(d)), pb.w * 4, lpx(d, 20)), count, 0, 13, muted);
    }
    if (d->col_kind == (uint32_t)SEL_LIB_ITEM_SOURCE) { /* I678g */
        draw_col_src(d, mn, muted);
    }
    draw_button(d, pb, accent, "Play", 13, ARGB(d->theme.token[SEL_GUI_TOKEN_ACCENT_TEXT]));
    draw_button(d, pls_more_rect(d, mn), surf, d->col_kind == 0u ? "More" : "Shuffle", 13, text);
    if (d->pls_n > 0u) { /* I678e: the sort button and the search field */
        sel_gui_rect fr = col_find_rect(d, mn);
        draw_button(d, col_sort_rect(d, mn), surf, d->col_sort == 1u ? "Sort: Title" : "Sort: Custom Order", 13, text);
        sel_gui_round_rect(d->canvas, fr, rad, surf);
        draw_text(d, rect_of(fr.x + lpx(d, 12), fr.y, fr.w - lpx(d, 24), fr.h), d->col_qlen > 0u ? d->col_q : "Search in this collection", 0, 13, d->col_qlen > 0u ? text : muted);
        if (d->col_search) {
            const sel_gui_atlas* at = desk_font(d, 0, 13);
            int32_t tw = at != NULL ? sel_gui_text_width(at, d->col_q, d->col_qlen) : 0, ch = fr.h - lpx(d, 12), cw = lpx(d, 2) > 0 ? lpx(d, 2) : 1;
            sel_gui_fill_rect(d->canvas, rect_of(fr.x + lpx(d, 12) + tw, fr.y + (fr.h - ch) / 2, cw, ch), text);
        }
    }
    d->pls_playing = -1;
    for (slot = 0; d->pls_scroll + slot < col_song_rows(d) && pls_row_visible(d, mn, slot); slot++, shown++) {
        uint32_t pos = d->pls_scroll + slot, i = d->pls_ord[pos];
        sel_gui_rect r = pls_row_rect(d, mn, slot);
        int playing = song_playing(d, pls_key(d, i)); /* I690 */
        char num[16];
        song_row* e = song_row_reg(d, r, pls_key(d, i), 0u, 0, 0);
        if (e != NULL && d->col_kind == 0u) {
            e->ref.pl_id = d->pls_id;
            e->ref.pl_index = i;
            song_row_set_list(d, e, SEL_UIM_LIST_PLAYLIST, d->pls_id, d->pls_ver, i);
        }
        if (d->col_kind != 0u && (int32_t)pos == d->col_focus) { /* I678f2: the keyboard focus row takes the selected-row fill */
            sel_gui_round_rect(d->canvas, r, rad, ARGB(d->theme.token[SEL_GUI_TOKEN_SURFACE_RAISED]));
        } else if (in_rect(r, d->ptr_x, d->ptr_y)) {
            sel_gui_round_rect(d->canvas, r, rad, surf);
        }
        snprintf(num, sizeof num, "%u", (unsigned)(pos + 1u));
        if (playing) { /* I690: the playing row shows the play icon for its number and an accent title */
            int32_t ic = lpx(d, 18);
            d->pls_playing = (int32_t)pos;
            sel_gui_icon_draw(d->canvas, &d->icons, SEL_GUI_ICON_PLAY, rect_of(r.x + lpx(d, 12), r.y + (r.h - ic) / 2, ic, ic), ic, accent);
        } else {
            draw_text(d, rect_of(r.x + lpx(d, 12), r.y, lpx(d, 36), r.h), num, 0, 14, muted);
        }
        draw_text(d, rect_of(r.x + lpx(d, 56), r.y, pls_title_w(d, r), r.h), d->pls_title[i], playing, 14, row_ink(d, r, playing ? accent : text));
        {   /* I678h3: the artist and album columns, drawn as links in the meta-line artist's style */
            sel_gui_rect la, lb;
            pls_link_rects(d, r, i, &la, &lb);
            if (la.w > 0) {
                draw_text(d, la, d->pls_meta[i].artist, 0, 13, text);
            }
            if (lb.w > 0) {
                draw_text(d, lb, d->pls_meta[i].album, 0, 13, text);
            }
        }
        if (d->col_kind == (uint32_t)SEL_LIB_ITEM_RECENT && d->col_plays != NULL && d->col_plays[i] > 0u) { /* I678g: the Played column: the day of the last play */
            char pl[40];
            time_t at = (time_t)d->col_plays[i];
            struct tm tmv;
            size_t n = 0;
            memset(&tmv, 0, sizeof tmv);
            if (gmtime_r(&at, &tmv) != NULL) {
                n = strftime(pl, sizeof pl, "Played %Y-%m-%d", &tmv);
            }
            if (n > 0u) {
                draw_text(d, rect_of(r.x + r.w - lpx(d, 168), r.y, lpx(d, 156), r.h), pl, 0, 13, muted);
            }
        } else if (d->col_kind != 0u && d->col_kind != (uint32_t)SEL_LIB_ITEM_RECENT && d->col_plays != NULL && d->col_plays[i] > 0u) { /* the Plays column */
            char pl[24];
            snprintf(pl, sizeof pl, "%u plays", (unsigned)d->col_plays[i]);
            draw_text(d, rect_of(r.x + r.w - lpx(d, 120), r.y, lpx(d, 108), r.h), pl, 0, 13, muted);
        }
    }
    if (d->col_kind != 0u) {
        col_draw_shelves(d, mn, text, muted);
    }
    if (d->pls_n == 0u && d->col_kind == 0u) {
        draw_playlist_find(d, mn);
    }
    d->pls_draws++;
    d->pls_rows = shown;
}

static void draw_playlists(desk* d) {
    const sel_state* st = sel_engine_state(d->engine);
    const sel_view_state* v = pl_view(st);
    sel_gui_rect mn = draw_panel(d, "main"), nm = pl_name_rect(d, mn);
    int32_t mg = lpx(d, HOME_MARGIN), rad = lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_RADIUS]);
    uint32_t text = ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT]), muted = ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT_MUTED]);
    uint32_t surf = ARGB(d->theme.token[SEL_GUI_TOKEN_SURFACE]), accent = ARGB(d->theme.token[SEL_GUI_TOKEN_ACCENT]);
    int32_t sel = pl_sel_row(d, v);
    uint32_t i, shown = 0;
    d->empty_text[0] = '\0';
    sel_gui_clip_push(d->canvas, mn);
    if (d->pls_id != 0u) { /* I633: one playlist open as a page of songs */
        draw_playlist_page(d, mn);
        sel_gui_clip_pop(d->canvas);
        sel_gui_dirty_add(d->canvas, mn);
        return;
    }
    draw_text(d, rect_of(mn.x + mg, mn.y + lpx(d, 16), mn.w - 2 * mg, lpx(d, page_title_h(d))), "Playlists", 1, 32, text);
    for (i = 0; i < PL_BUTTONS; i++) {
        sel_gui_rect b = queue_btn_rect(d, mn, i);
        int live = i < 2u ? 1 : sel >= 0 && v->rows[sel].kind == (uint32_t)SEL_ROW_PLAYLIST;
        draw_button(d, b, surf, pl_button_label(i), 13, live ? text : muted);
    }
    sel_gui_round_rect(d->canvas, nm, rad, surf);
    draw_text(d, rect_of(nm.x + lpx(d, 12), nm.y, nm.w - lpx(d, 24), nm.h), d->pl_nlen > 0u ? d->pl_name : "Name", d->pl_edit, 13,
              d->pl_nlen > 0u ? text : muted);
    for (i = 0; v != NULL && i < v->row_n && pl_row_visible(d, mn, i); i++, shown++) {
        sel_gui_rect r = pl_row_rect(d, mn, i);
        int32_t ind = (int32_t)v->rows[i].depth * lpx(d, PL_INDENT);
        int folder = v->rows[i].kind == (uint32_t)SEL_ROW_FOLDER;
        if ((int32_t)i == sel) {
            sel_gui_round_rect(d->canvas, r, rad, surf);
        }
        draw_text(d, rect_of(r.x + lpx(d, 12) + ind, r.y, r.w - lpx(d, 24) - ind, r.h), v->rows[i].text, folder, 14,
                  row_ink(d, r, (int32_t)i == sel ? accent : folder ? text : muted));
    }
    if (shown == 0u) {
        draw_empty_state(d, mn, pl_row_rect(d, mn, 0).y, sel_uim_state_text(SEL_UIM_SV_PLAYLISTS, SEL_UIM_ST_EMPTY));
    }
    d->pl_draws++;
    d->pl_rows = shown;
    sel_gui_clip_pop(d->canvas);
    sel_gui_dirty_add(d->canvas, mn);
}

static int pl_send(desk* d, const char* line) {
    d->pl_cmds++;
    d->side_stale = 1u;
    return desk_send(d, line);
}

/* The name from the field, or the fallback when it is empty. The field is cleared once its text is used. */
static const char* pl_take_name(desk* d, const char* fallback) {
    static char name[64];
    snprintf(name, sizeof name, "%s", d->pl_nlen > 0u ? d->pl_name : fallback);
    d->pl_name[0] = '\0';
    d->pl_nlen = 0u;
    d->pl_edit = 0;
    return name;
}

/* The New Playlist action: the Playlists page button and the sidebar's empty-state button (I627) both send it here. */
static int pl_create(desk* d, uint32_t folder) {
    char line[160];
    sel_uim_plbtn_line(1u, pl_take_name(d, "New Playlist"), folder, line, sizeof line);
    return pl_send(d, line);
}

/* I646, I609: Create Playlist from the sidebar (the empty-state button and the header plus): create and open, like Spotify, the
 * new playlist opens once the engine has made it. The one path; the action itself is pl_create. */
static int side_create_playlist(desk* d) {
    d->side_open_n = sel_engine_library_playlists(d->engine) + 1u; /* I678b: playlists only, the list holds more kinds */
    d->side_open_tries = 0u;
    desk_page_goto(d, PG_COLLECTION);
    d->repaint = 1;
    d->uim.dirty |= SEL_UIM_DIRTY_ALL;
    return pl_create(d, 0u);
}

static int pl_button_click(desk* d, const sel_view_state* v, uint32_t k) {
    int32_t sel = pl_sel_row(d, v);
    char line[160];
    d->repaint = 1;
    d->uim.dirty |= SEL_UIM_DIRTY_ALL;
    switch (k) {
    case 0u:
        sel_uim_plbtn_line(0u, pl_take_name(d, "New Folder"), pl_target_folder(d, v), line, sizeof line);
        return pl_send(d, line);
    case 1u:
        return pl_create(d, pl_target_folder(d, v));
    default:
        if (sel < 0 || v->rows[sel].kind != (uint32_t)SEL_ROW_PLAYLIST) {
            return 1;
        }
        snprintf(line, sizeof line, "VIEW_CURSOR %u", (unsigned)(v->window_start + (uint32_t)sel));
        pl_send(d, line);
        return pl_send(d, "VIEW_ACTIVATE");
    }
}

/* Plays the song row at display position pos of the open playlist or collection page: the one path a click and Enter share. */
static int pls_play_pos(desk* d, uint32_t pos) {
    snprintf(d->np_ctx, sizeof d->np_ctx, "%s", d->pls_name); /* I619 */
    desk_play_path(d, pls_key(d, d->pls_ord[pos]));
    d->row_plays++;
    return 1;
}

/* One pointer event on an open playlist page: Back, Play, or a click on a song row plays that song. */
static int pls_pointer(desk* d, const sel_gui_input* ev) {
    sel_gui_rect mn = region_rect(d, "main");
    uint32_t slot;
    char line[48];
    if (ev->kind != SEL_GUI_IN_CLICK || ev->button != 0u) {
        return 0;
    }
    if (d->col_kind == (uint32_t)SEL_LIB_ITEM_ALBUM && d->col_artist.w > 0 && in_rect(d->col_artist, ev->x, ev->y)) { /* I678h: the artist name opens the artist page */
        char artist[96];
        snprintf(artist, sizeof artist, "%s", d->col_sub);
        desk_route(d, SEL_ROUTE_META_ARTIST, (uint32_t)SEL_LIB_ITEM_ARTIST, 0u, artist, artist, "");
        return 1;
    }
    if (d->col_kind != 0u && col_kind_click(d, mn, ev->x, ev->y)) { /* I678f: Show more, a shelf card */
        return 1;
    }
    if (d->col_kind == (uint32_t)SEL_LIB_ITEM_SOURCE && col_src_click(d, mn, ev->x, ev->y)) { /* I678g: the source switch and Sync */
        return 1;
    }
    if (d->pls_n == 0u && d->col_kind == 0u) { /* I652a: the search field of the empty playlist, and an Add on a result */
        int on = in_rect(pls_find_field_rect(d, mn), ev->x, ev->y);
        if (in_rect(pls_find_btn_rect(d, mn, 0u), ev->x, ev->y)) {
            d->pl_edit = 0;
            add_mode_open(d);
            return 1;
        }
        if (in_rect(pls_find_btn_rect(d, mn, 1u), ev->x, ev->y)) {
            d->pl_edit = 0;
            return desk_pls_import_pick(d);
        }
        if (in_rect(pls_find_btn_rect(d, mn, 2u), ev->x, ev->y)) {
            d->pl_edit = 0;
            return desk_pls_folder_pick(d);
        }
        for (slot = 0; pls_find_row_visible(d, mn, slot) && pls_find_result(d, slot) != NULL; slot++) {
            if (in_rect(pls_find_add_rect(d, mn, slot), ev->x, ev->y)) {
                d->pl_edit = 0;
                pls_find_add(d, slot);
                return 1;
            }
        }
        if (on || d->focus == (int32_t)d->layout.count) {
            d->focus = on ? (int32_t)d->layout.count : -1;
            d->repaint = 1;
            d->uim.dirty |= SEL_UIM_DIRTY_ALL;
            if (on) {
                d->pl_edit = 0;
                return 1;
            }
        }
    }
    if (d->pls_n > 0u) { /* I678e: the sort button cycles Custom Order and Title; the search field takes the keyboard */
        if (in_rect(col_sort_rect(d, mn), ev->x, ev->y)) {
            d->col_sort = d->col_sort == 0u ? 1u : 0u;
            d->col_search = 0;
            d->pl_edit = 0;
            col_changed(d);
            return 1;
        }
        d->col_search = in_rect(col_find_rect(d, mn), ev->x, ev->y);
        if (d->col_search) {
            d->pl_edit = 0;
            d->repaint = 1;
            d->uim.dirty |= SEL_UIM_DIRTY_ALL;
            return 1;
        }
    }
    d->pl_edit = d->col_kind == 0u && in_rect(pls_title_rect(d, mn), ev->x, ev->y); /* I646: the name field */
    if (d->pl_edit) {
        d->pl_nlen = 0u;
        d->pl_name[0] = '\0';
        d->repaint = 1;
        d->uim.dirty |= SEL_UIM_DIRTY_ALL;
        return 1;
    }
    if (in_rect(pls_back_rect(d, mn), ev->x, ev->y)) {
        pls_close(d);
        return 1;
    }
    if (d->col_kind == 0u && in_rect(pls_more_rect(d, mn), ev->x, ev->y)) {
        sel_gui_rect mr = pls_more_rect(d, mn);
        plmenu_open(d, mr.x, mr.y + mr.h, d->pls_id, 0u, d->pls_name);
        d->repaint = 1;
        return 1;
    }
    if (d->col_kind != 0u && in_rect(pls_more_rect(d, mn), ev->x, ev->y)) { /* the Shuffle button */
        return col_kind_play(d, 1);
    }
    if (d->col_kind != 0u && in_rect(pls_play_rect(d, mn), ev->x, ev->y)) {
        return col_kind_play(d, 0);
    }
    if (in_rect(pls_play_rect(d, mn), ev->x, ev->y)) {
        snprintf(d->np_ctx, sizeof d->np_ctx, "%s", d->pls_name); /* I619: the Now Playing panel names the playlist */
        snprintf(line, sizeof line, "PLAYLIST_PLAY %u", (unsigned)d->pls_id);
        d->repaint = 1;
        return desk_send(d, line);
    }
    for (slot = 0; d->pls_scroll + slot < col_song_rows(d) && pls_row_visible(d, mn, slot); slot++) {
        sel_gui_rect row = pls_row_rect(d, mn, slot), la, lb;
        uint32_t real = d->pls_ord[d->pls_scroll + slot];
        if (!in_rect(row, ev->x, ev->y)) {
            continue;
        }
        pls_link_rects(d, row, real, &la, &lb);
        if (la.w > 0 && in_rect(la, ev->x, ev->y)) { /* I678h3: the row's artist opens the artist page */
            char artist[96];
            snprintf(artist, sizeof artist, "%s", d->pls_meta[real].artist);
            desk_route(d, SEL_ROUTE_TRACK_ARTIST, (uint32_t)SEL_LIB_ITEM_ARTIST, 0u, artist, artist, "");
            return 1;
        }
        if (lb.w > 0 && in_rect(lb, ev->x, ev->y)) { /* the row's album opens the album page, by the db row's album id */
            char idkey[16];
            if (d->pls_meta[real].album_id != 0u) {
                snprintf(idkey, sizeof idkey, "%u", (unsigned)d->pls_meta[real].album_id);
                desk_route(d, SEL_ROUTE_TRACK_ALBUM, (uint32_t)SEL_LIB_ITEM_ALBUM, d->pls_meta[real].album_id, idkey, d->pls_meta[real].album,
                           d->pls_meta[real].artist);
                return 1;
            }
        }
        return pls_play_pos(d, d->pls_scroll + slot);
    }
    return 0;
}

/* One pointer event on the Playlists page: a button or a row click, or a drag of a row onto a folder row. */
static int pl_pointer(desk* d, const sel_gui_input* ev) {
    const sel_view_state* v = pl_view(sel_engine_state(d->engine));
    sel_gui_rect mn = region_rect(d, "main");
    char line[160];
    uint32_t k;
    int32_t row, to;
    if (d->pls_id != 0u) {
        return pls_pointer(d, ev);
    }
    if (v == NULL) {
        return 0;
    }
    if (ev->kind == SEL_GUI_IN_DRAG) {
        row = pl_row_at(d, mn, ev->x, ev->y);
        to = pl_row_at(d, mn, ev->x2, ev->y2);
        if (row < 0 || to < 0 || row == to || v->rows[to].kind != (uint32_t)SEL_ROW_FOLDER) {
            return 0;
        }
        snprintf(line, sizeof line, "%s %u %u", v->rows[row].kind == (uint32_t)SEL_ROW_FOLDER ? "FOLDER_MOVE" : "PLAYLIST_MOVE",
                 (unsigned)v->rows[row].id, (unsigned)v->rows[to].id);
        return pl_send(d, line);
    }
    if (ev->kind != SEL_GUI_IN_CLICK || ev->button != 0u) {
        return 0;
    }
    for (k = 0; k < PL_BUTTONS; k++) {
        if (in_rect(queue_btn_rect(d, mn, k), ev->x, ev->y)) {
            return pl_button_click(d, v, k);
        }
    }
    d->pl_edit = in_rect(pl_name_rect(d, mn), ev->x, ev->y);
    row = pl_row_at(d, mn, ev->x, ev->y);
    if (row >= 0) {
        d->pl_sel_kind = v->rows[row].kind;
        d->pl_sel_id = v->rows[row].id;
        if (v->rows[row].kind == (uint32_t)SEL_ROW_PLAYLIST) { /* I633: a playlist row opens its page */
            char idkey[16];
            snprintf(idkey, sizeof idkey, "%u", (unsigned)v->rows[row].id);
            desk_route(d, SEL_ROUTE_SIDEBAR_ROW, (uint32_t)SEL_LIB_ITEM_PLAYLIST, v->rows[row].id, idkey, v->rows[row].text, ""); /* I678h3: through the route history */
        }
    }
    d->repaint = 1;
    d->uim.dirty |= SEL_UIM_DIRTY_ALL;
    return 1;
}

/* I647: a right-click (or ctrl-click) on a playlist or folder row of the Playlists list, or on a playlist row of the sidebar's Your
 * Library list, opens the Rename / Delete menu. */
static int plmenu_pointer(desk* d, const sel_gui_input* ev) {
    int32_t row, sr;
    if (ev->kind != SEL_GUI_IN_CLICK || !(ev->button == 1u || (ev->button == 0u && (ev->mods & TXT_MOD_CTRL) != 0u))) {
        return 0;
    }
    sr = side_lib_at(d, ev->x, ev->y);
    if (sr >= 0 && d->side_items[sr].kind != (uint32_t)SEL_LIB_ITEM_PLAYLIST) {
        return 0; /* I678b: only playlists have the Rename / Delete menu */
    }
    if (sr >= 0) {
        plmenu_open(d, ev->x, ev->y, d->side_items[sr].id, 0u, d->side_items[sr].name);
    } else if (d->pl_page && d->pls_id == 0u) {
        const sel_view_state* v = pl_view(sel_engine_state(d->engine));
        row = v != NULL ? pl_row_at(d, region_rect(d, "main"), ev->x, ev->y) : -1;
        if (row < 0) {
            return 0;
        }
        plmenu_open(d, ev->x, ev->y, v->rows[row].id, v->rows[row].kind == (uint32_t)SEL_ROW_FOLDER, v->rows[row].text);
    } else {
        return 0;
    }
    draw_home(d);
    return 1;
}

/* I678e: a typed key while the collection's search field has the keyboard; every change refilters the songs. */
static int col_key(desk* d, const sel_gui_input* ev) {
    if (ev->code == 13u || ev->code == 27u) {
        d->col_search = 0;
        d->repaint = 1;
        d->uim.dirty |= SEL_UIM_DIRTY_ALL;
        return desk_present(d);
    }
    if (ev->code == 127u && d->col_qlen > 0u) {
        d->col_q[--d->col_qlen] = '\0';
    } else if (ev->code >= 32u && ev->code < 127u && (ev->mods & (TXT_MOD_CTRL | TXT_MOD_ALT)) == 0u && d->col_qlen + 1u < sizeof d->col_q) {
        d->col_q[d->col_qlen++] = (char)ev->code;
        d->col_q[d->col_qlen] = '\0';
    } else {
        return 0;
    }
    col_changed(d);
    return desk_present(d);
}

/* A typed key while the name field is focused: printable keys append, Backspace deletes, Enter and Escape leave. */
static int pl_key(desk* d, const sel_gui_input* ev) {
    if (ev->code == 13u && d->pls_id != 0u && d->pl_nlen > 0u) { /* I646: the open playlist's name field: Enter renames it */
        char line[160];
        snprintf(d->pls_name, sizeof d->pls_name, "%s", d->pl_name);
        sel_uim_plrename_line(d->pls_id, pl_take_name(d, ""), line, sizeof line);
        pl_send(d, line);
    } else if (ev->code == 13u || ev->code == 27u) {
        d->pl_edit = 0;
    } else if (ev->code == 127u) {
        if (d->pl_nlen > 0u) {
            d->pl_name[--d->pl_nlen] = '\0';
        }
    } else if (ev->code >= 32u && ev->code < 127u && ev->code != '"' && (ev->mods & (TXT_MOD_CTRL | TXT_MOD_ALT)) == 0u) {
        if (d->pl_nlen + 1u < sizeof d->pl_name) {
            d->pl_name[d->pl_nlen++] = (char)ev->code;
            d->pl_name[d->pl_nlen] = '\0';
        }
    } else {
        return 0;
    }
    d->repaint = 1;
    d->uim.dirty |= SEL_UIM_DIRTY_ALL;
    return desk_present(d);
}

/* ---- Song Information page (DUI-P4-10): module info (the replayer's metadata tags), the catalogue rows of the open song
 * (STIL note, composer, release, year, genre, rating, play count: sel_state.panel) and the flag buttons. The engine owns
 * the flags and the note (FLAG wrong, FLAG skip, NOTE: the library flags table, the rows `selecta-cli wrong` lists); the
 * page draws the data and sends those verbs for the song being played. */
enum { SI_BUTTONS = 3, SI_ROW_H = 20 };

static const char* si_button_label(uint32_t k) {
    return sel_uim_flag_label(k);
}

static void si_clean(char* val) {
    uint32_t i;
    for (i = 0; val[i] != '\0'; i++) {
        if ((unsigned char)val[i] < 32u && val[i] != '\n') { /* I298: a newline stays, the page wraps at it */
            val[i] = ' ';
        }
    }
}

/* I678h3: the canon of the open song's first composer (the db row), cached by song path in si_composer_key. */
static void si_composer_load(desk* d, const sel_state* st) {
    char m[SEL_COMPOSER_MAX + 224];
    const char* p = m;
    size_t k;
    if (st->song.path == NULL || strcmp(d->si_composer_ref, st->song.path) == 0) {
        return;
    }
    snprintf(d->si_composer_ref, sizeof d->si_composer_ref, "%s", st->song.path);
    d->si_composer_key[0] = '\0';
    if (sel_engine_song_meta(d->engine, st->song.path, m, sizeof m)) {
        for (k = 0; k < 3u; k++) {
            p += strlen(p) + 1u;
        }
        snprintf(d->si_composer_key, sizeof d->si_composer_key, "%s", p);
    }
}

/* Calls emit(d, label, value, row index) for every info row in order: the metadata tags, then the panel pairs. The
 * drawing and the geometry dump share this walk, so the dumped rows are the drawn rows. */
static uint32_t si_walk(desk* d, const sel_state* st, void (*emit)(desk*, const char*, const char*, uint32_t)) {
    const uintptr_t absent = (uintptr_t)-1;
    uint32_t n = 0, tag;
    char label[32], val[2048];
    d->si_composer_row = -1;
    for (tag = SEL_META_TITLE; sel_tag_name(tag) != NULL; tag++) {
        uintptr_t v = sel_tag_find(st->song.meta, tag, absent);
        sel_tag_type type = SEL_TT_INT;
        if (v == absent || !sel_tag_type_of(tag, &type)) {
            continue;
        }
        if (type == SEL_TT_STR) {
            snprintf(val, sizeof val, "%s", (const char*)v);
        } else {
            snprintf(val, sizeof val, "%lu", (unsigned long)v);
        }
        si_clean(val);
        sel_uim_tag_label(tag, label, sizeof label);
        emit(d, label, val, n++);
    }
    if (st->panel != NULL) {
        const char* p = st->panel;
        while (*p != '\0') {
            const char* value = p + strlen(p) + 1;
            snprintf(val, sizeof val, "%s", value);
            si_clean(val);
            if (strcmp(p, "Composer") == 0) { /* I678h3: the catalogue's Composer row links to the composer the db row names */
                si_composer_load(d, st);
                d->si_composer_row = d->si_composer_key[0] != '\0' ? (int32_t)n : -1;
            }
            emit(d, p, val, n++);
            p = value + strlen(value) + 1;
        }
    }
    return n;
}

/* I298: a value wraps into the value column and the row grows to its lines; the list scrolls by wheel. */
static int32_t si_value_w(desk* d, sel_gui_rect mn) {
    return mn.w - 2 * lpx(d, HOME_MARGIN) - lpx(d, 160);
}

static int32_t si_lines(desk* d, sel_gui_rect mn, const char* value) {
    const sel_gui_atlas* at = desk_font(d, 0, 13);
    int32_t n = sel_gui_text_wrap(NULL, at, rect_of(0, 0, si_value_w(d, mn), 0), value, 0u, 1 << 20, NULL); /* NULL canvas: count only */
    return n < 1 ? 1 : n;
}

static int32_t si_row_h(desk* d, sel_gui_rect mn, const char* value) {
    int32_t h = si_lines(d, mn, value) * desk_font(d, 0, 13)->line_height;
    return h > lpx(d, SI_ROW_H) ? h : lpx(d, SI_ROW_H);
}

static int32_t si_list_top(desk* d, sel_gui_rect mn) {
    return mn.y + lpx(d, page_y(d, PL_LIST_Y));
}

/* Screen top of the row the walk is at (si_cy is the content y above it), after the scroll. */
static int32_t si_row_y(desk* d, sel_gui_rect mn) {
    return si_list_top(d, mn) + d->si_cy - lpx(d, d->si_y_q16 >> 16);
}

/* I678h3: the composer link rect of the info row `i` at screen top y: the value text's width on its first line. */
static void si_composer_rect(desk* d, sel_gui_rect mn, const char* value, uint32_t i, int32_t y) {
    const sel_gui_atlas* at = desk_font(d, 0, 13);
    int32_t tw = at != NULL ? sel_gui_text_width(at, value, strlen(value)) : 0;
    if ((int32_t)i != d->si_composer_row || si_lines(d, mn, value) > 1) {
        return;
    }
    d->si_composer = rect_of(mn.x + lpx(d, HOME_MARGIN) + lpx(d, 160), y, tw < si_value_w(d, mn) ? tw : si_value_w(d, mn), lpx(d, SI_ROW_H));
}

static void si_draw_row(desk* d, const char* label, const char* value, uint32_t i) {
    sel_gui_rect mn = region_rect(d, "main");
    int32_t h = si_row_h(d, mn, value), y = si_row_y(d, mn), half = lpx(d, 160);
    d->si_cy += h;
    if (y + h <= si_list_top(d, mn) || y >= mn.y + mn.h) {
        return;
    }
    si_composer_rect(d, mn, value, i, y);
    draw_text(d, rect_of(mn.x + lpx(d, HOME_MARGIN), y, half, lpx(d, SI_ROW_H)), label, 0, 12, ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT_MUTED]));
    if (si_lines(d, mn, value) > 1) {
        sel_gui_text_wrap(d->canvas, desk_font(d, 0, 13), rect_of(mn.x + lpx(d, HOME_MARGIN) + half, y, si_value_w(d, mn), h), value,
                          ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT]), 1 << 20, &d->text);
    } else {
        draw_text(d, rect_of(mn.x + lpx(d, HOME_MARGIN) + half, y, si_value_w(d, mn), lpx(d, SI_ROW_H)), value, 0, 13,
                  ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT]));
    }
    d->si_rows++;
}

static void si_hash_row(desk* d, const char* label, const char* value, uint32_t i) {
    const char* s;
    for (s = label; *s != '\0'; s++) {
        d->si_acc = (d->si_acc ^ (unsigned char)*s) * 16777619u;
    }
    for (s = value; *s != '\0'; s++) {
        d->si_acc = (d->si_acc ^ (unsigned char)*s) * 16777619u;
    }
    d->si_acc = (d->si_acc ^ i) * 16777619u;
}

/* Dumps `sirow i label|value` (newlines flattened) and `sigeo i y h lines`: y is the row's screen top after the scroll. */
static void si_dump_row(desk* d, const char* label, const char* value, uint32_t i) {
    size_t o = strlen(d->library_dump), k;
    sel_gui_rect mn = region_rect(d, "main");
    int32_t h = si_row_h(d, mn, value), y = si_row_y(d, mn);
    char flat[2048];
    snprintf(flat, sizeof flat, "%s", value);
    for (k = 0; flat[k] != '\0'; k++) {
        flat[k] = flat[k] == '\n' ? ' ' : flat[k];
    }
    d->si_cy += h;
    si_composer_rect(d, mn, value, i, y);
    if (o + 2600u < sizeof d->library_dump) {
        snprintf(d->library_dump + o, sizeof d->library_dump - o, "sirow %u %s|%s\nsigeo %u %d %d %d\n", (unsigned)i, label, flat,
                 (unsigned)i, (int)y, (int)h, (int)si_lines(d, mn, value));
    }
}

static void si_walk_from_top(desk* d, const sel_state* st, void (*emit)(desk*, const char*, const char*, uint32_t)) {
    d->si_cy = 0;
    si_walk(d, st, emit);
}

static void si_count_row(desk* d, const char* label, const char* value, uint32_t i) {
    (void)label;
    (void)i;
    d->si_cy += si_row_h(d, region_rect(d, "main"), value);
}

/* Largest scroll offset (logical px, Q16): the whole list height less the room under the list top. */
static int32_t si_max_q16(desk* d) {
    sel_gui_rect mn = region_rect(d, "main");
    int32_t room = mn.y + mn.h - si_list_top(d, mn), over;
    si_walk_from_top(d, sel_engine_state(d->engine), si_count_row);
    over = px_to_logical_ceil(d, d->si_cy - room);
    return over > 0 ? over * 65536 : 0;
}

/* ---- Visualizer (DUI-P4-11) ----
 * The viz plugins (scope, levels, spectrum) render the engine's newest frame into an RGBA32 target the desktop owns; the
 * desktop blits it. Three places show it: the strip in Now Playing, the Visualizer page (one pane per plugin) and the
 * visual layout (full canvas with the signal path). viz_tick draws once per new engine frame. */
#define VIZ_MAX 3u
enum { VIZ_NONE = 0, VIZ_STRIP = 1, VIZ_PAGE = 2, VIZ_VISUAL = 3 };

static void viz_load(desk* d) {
    static const char* const ids[VIZ_MAX] = {"viz_scope", "viz_levels", "viz_spectrum"};
    uint32_t i;
    d->viz_n = 0u;
    for (i = 0; d->find_plugin != NULL && i < VIZ_MAX; i++) {
        const sel_tag* desc = d->find_plugin(SEL_PLUGIN_KIND_VIZ, ids[i]);
        const sel_tag* hooks = desc != NULL ? (const sel_tag*)sel_tag_find(desc, SEL_PLUGIN_TAGS, 0) : NULL;
        uintptr_t fn = hooks != NULL ? sel_tag_find(hooks, SEL_VIZ_FRAME, 0) : 0;
        if (fn == 0) {
            continue;
        }
        d->viz_fn[d->viz_n] = (sel_viz_render_fn)fn;
        d->viz_id[d->viz_n] = ids[i];
        d->viz_name[d->viz_n] = (const char*)sel_tag_find(hooks, SEL_VIZ_NAME, 0);
        d->viz_n++;
    }
}

/* The ui.desktop.viz pref: a plugin id picks the plugin the strip and the visual layout draw. */
static void viz_select(desk* d, const char* id) {
    uint32_t i;
    for (i = 0; i < d->viz_n; i++) {
        if (strcmp(d->viz_id[i], id) == 0) {
            d->viz_sel = i;
            d->uim.dirty |= SEL_UIM_DIRTY_ALL;
            d->repaint = 1; /* the switch shows the selected plugin */
        }
    }
}

/* Which place shows the visualizer now. */
static int viz_visible(const desk* d) {
    if (d->viz_n == 0u) {
        return VIZ_NONE;
    }
    if (d->uim.layout == SEL_UIM_LAYOUT_VISUAL) {
        return VIZ_VISUAL;
    }
    if (d->uim.layout != SEL_UIM_LAYOUT_FULL || d->overlay != 0) {
        return VIZ_NONE;
    }
    if (d->viz_page) {
        return VIZ_PAGE;
    }
    return d->np_viz.w > 0 ? VIZ_STRIP : VIZ_NONE;
}

/* Renders plugin `idx` for the newest frame into `r` and blits it. */
static void viz_draw_area(desk* d, sel_gui_rect r, uint32_t idx) {
    const sel_state* st = sel_engine_state(d->engine);
    sel_surface s;
    sel_viz_colors c;
    uint32_t need;
    if (r.w <= 0 || r.h <= 0 || idx >= d->viz_n) {
        return;
    }
    need = (uint32_t)r.w * (uint32_t)r.h;
    if (need > d->viz_cap) {
        if (d->viz_px != NULL) {
            d->host->mem_free(d->host, d->viz_px);
        }
        d->viz_px = (uint32_t*)d->host->mem_alloc(d->host, (size_t)need * sizeof(uint32_t), SEL_MEM_FAST);
        d->viz_cap = d->viz_px != NULL ? need : 0u;
        if (d->viz_px == NULL) {
            return;
        }
    }
    s.w = (uint32_t)r.w;
    s.h = (uint32_t)r.h;
    s.pitch = (uint32_t)r.w * (uint32_t)sizeof(uint32_t);
    s.fmt = SEL_PIX_RGBA32;
    s.pixels = d->viz_px;
    s.palette = NULL;
    c.background = ARGB(d->theme.token[SEL_GUI_TOKEN_SURFACE]);
    c.grid = ARGB(d->theme.token[SEL_GUI_TOKEN_SURFACE_RAISED]);
    c.primary = ARGB(d->theme.token[SEL_GUI_TOKEN_ACCENT]);
    c.secondary = ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT_MUTED]);
    c.accent = ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT]);
    d->viz_prims = d->viz_fn[idx](st->viz, &s, &c);
    sel_gui_blit_image(d->canvas, r, d->viz_px, r.w, r.h, r.w, 0);
    sel_gui_dirty_add(d->canvas, r);
}

/* F38: the signal path as text, from the engine state the song goes through: replayer, mixer or voice output, the
 * stages that are on, the output. */
static void viz_sigpath_text(desk* d, char* out, size_t n) {
    const sel_state* st = sel_engine_state(d->engine);
    sel_engine_stats es;
    size_t o;
    sel_engine_stats_get(d->engine, &es);
    o = (size_t)snprintf(out, n, "Signal Path: %s > %s", es.replayer != NULL ? es.replayer : "No Song",
                         st->mixed_path != 0u ? "Mixer" : "Voice Output");
    if (st->led_on != 0u && o < n) {
        o += (size_t)snprintf(out + o, n - o, " > LED Filter");
    }
    if (st->stereo_width < 100u && o < n) {
        o += (size_t)snprintf(out + o, n - o, " > Stereo Width %u Percent", (unsigned)st->stereo_width);
    }
    if (es.crossfeed_on != 0u && o < n) {
        o += (size_t)snprintf(out + o, n - o, " > Crossfeed");
    }
    if (o < n) {
        (void)snprintf(out + o, n - o, " > Output %s", st->output);
    }
}

static void viz_sigpath_draw(desk* d, sel_gui_rect r) {
    char line[200];
    viz_sigpath_text(d, line, sizeof line);
    draw_text(d, r, line, 0, 13, ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT_MUTED]));
    d->viz_sigpath++;
}

/* The visual layout: the whole canvas, the visualizer above the signal path. */
static void draw_visual(desk* d) {
    int32_t mg = lpx(d, HOME_MARGIN), th = lpx(d, 24);
    sel_gui_rect all = rect_of(0, 0, d->canvas->w, cvh(d));
    sel_gui_fill_rect(d->canvas, all, ARGB(d->theme.token[SEL_GUI_TOKEN_FRAME]));
    viz_draw_area(d, rect_of(mg, mg, all.w - 2 * mg, all.h - 3 * mg - th), d->viz_sel);
    viz_sigpath_draw(d, rect_of(mg, all.h - mg - th, all.w - 2 * mg, th));
    sel_gui_dirty_add(d->canvas, all);
    d->viz_visual_draws++;
}

/* The visualizer switch: one segment per plugin found, the selected one in the accent colour. Slot is 0 for the strip in
 * Now Playing and 1 for the Visualizer page; the rects the segments took are what a click is tested against. */
static void viz_switch_draw(desk* d, uint32_t slot, sel_gui_rect r) {
    uint32_t k;
    int32_t gap = lpx(d, 4), w;
    for (k = 0; k < 3u; k++) {
        d->viz_btn[slot][k] = rect_of(0, 0, 0, 0);
    }
    if (d->viz_n == 0u || r.w <= 0 || r.h <= 0) {
        return;
    }
    w = (r.w - ((int32_t)d->viz_n - 1) * gap) / (int32_t)d->viz_n;
    for (k = 0; k < d->viz_n; k++) {
        sel_gui_rect b = rect_of(r.x + (int32_t)k * (w + gap), r.y, w, r.h);
        int on = k == d->viz_sel;
        d->viz_btn[slot][k] = b;
        draw_button(d, b, ARGB(d->theme.token[on ? SEL_GUI_TOKEN_ACCENT : SEL_GUI_TOKEN_SURFACE_RAISED]), d->viz_name[k], 12,
                    ARGB(d->theme.token[on ? SEL_GUI_TOKEN_ACCENT_TEXT : SEL_GUI_TOKEN_TEXT_MUTED]));
    }
    sel_gui_dirty_add(d->canvas, r);
}

/* The switch button under a point as (slot, plugin index) packed slot * 3 + k, -1 for none. */
static int32_t viz_switch_at(const desk* d, int32_t x, int32_t y) {
    uint32_t s, k;
    for (s = 0; s < 2u; s++) {
        for (k = 0; k < d->viz_n; k++) {
            if (d->viz_btn[s][k].w > 0 && in_rect(d->viz_btn[s][k], x, y)) {
                return (int32_t)(s * 3u + k);
            }
        }
    }
    return -1;
}

/* A click on a switch button runs the plugin's row of the action table (the line the View/Visualizer menu and its key
 * run); the action id equals the plugin id. */
static int viz_switch_click(desk* d, int32_t hit) {
    const sel_uim_action* a = desk_action_by_id(d->viz_id[(uint32_t)hit % 3u]);
    if (a == NULL || !desk_send(d, a->line)) {
        return 0;
    }
    d->viz_clicks++;
    d->actions_fired++;
    return 1;
}

/* The Visualizer page: one pane per plugin, then the signal path. */
static void viz_panes_draw(desk* d) {
    uint32_t k;
    uint32_t text = ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT]);
    for (k = 0; k < d->viz_n; k++) {
        sel_gui_rect p = d->viz_pane[k];
        draw_text(d, rect_of(p.x, p.y - lpx(d, 20), p.w, lpx(d, 18)), d->viz_name[k], 1, 12, text);
        viz_draw_area(d, p, k);
    }
}

static void draw_vizpage(desk* d) {
    sel_gui_rect mn = draw_panel(d, "main");
    int32_t mg = lpx(d, HOME_MARGIN), top = mn.y + lpx(d, page_y(d, 64)), gap = lpx(d, 28);
    int32_t ph = (mn.h - lpx(d, page_y(d, 64)) - lpx(d, 40) - 3 * gap) / 3;
    uint32_t k;
    sel_gui_clip_push(d->canvas, mn);
    draw_text(d, rect_of(mn.x + mg, mn.y + lpx(d, 16), mn.w - 2 * mg, lpx(d, page_title_h(d))), "Visualizer", 1, 32,
              ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT]));
    /* I599: the switch ends left of the history arrows, which share its row (the arrows now sit on the search field row) */
    {
        int32_t sr = mn.x + mn.w - mg;
        if (nav_arrows_shown(d, mn) && sr > nav_arrow_rect(d, mn, 0).x - lpx(d, 8)) {
            sr = nav_arrow_rect(d, mn, 0).x - lpx(d, 8);
        }
        viz_switch_draw(d, 1u, rect_of(sr - lpx(d, 360), mn.y + lpx(d, 22), lpx(d, 360), lpx(d, 28)));
    }
    for (k = 0; k < VIZ_MAX; k++) {
        d->viz_pane[k] = rect_of(mn.x + mg, top + gap + (int32_t)k * (ph + gap), mn.w - 2 * mg, ph);
    }
    viz_panes_draw(d);
    d->viz_panes = d->viz_n;
    viz_sigpath_draw(d, rect_of(mn.x + mg, mn.y + mn.h - lpx(d, 36), mn.w - 2 * mg, lpx(d, 24)));
    sel_gui_clip_pop(d->canvas);
    sel_gui_dirty_add(d->canvas, mn);
}

/* One draw per new engine frame while a visualizer is on screen. Returns 1 when a frame was drawn. */
static int viz_tick(desk* d) {
    const sel_state* st = sel_engine_state(d->engine);
    int where = viz_visible(d);
    if (where == VIZ_NONE || st->viz == NULL || st->viz->seq == 0u || st->viz->seq == d->viz_seq || d->frames == 0u ||
        d->repaint) {
        return 0;
    }
    if (!d->live && (!d->ticks_given || (d->ticks == 0u && (d->input == NULL || d->input_done)))) {
        return 0; /* the mem backend pumps the engine without a clock: only a run with --ticks drives visualizer frames, so scripted runs without ticks keep their frame counts and hashes */
    }
    /* The mem backend has no song clock: it pumps one 20 ms block per turn, and the engine numbers its frames by block,
     * so the song time of a frame there is its sequence number times 20 ms. */
    desk_frame_begin(d);
    d->viz_frames++;
    d->viz_seq = st->viz->seq;
    d->viz_song_ms = st->song.position_ms != 0u ? st->song.position_ms : st->viz->seq * 20u;
    if (where == VIZ_VISUAL) {
        draw_visual(d);
    } else if (where == VIZ_PAGE) {
        viz_panes_draw(d);
    } else {
        viz_draw_area(d, d->np_viz, d->viz_sel);
    }
    d->dirty_pixels = desk_flush(d); /* a visualizer frame is counted in viz_frames, not in frames: frames count state changes */
    d->dirty_total += d->dirty_pixels;
    return 1;
}

/* ---- Tracker page (DUI-P4-12): pattern view and sample list with click-to-play ---- */
enum { TR_TAB_W = 120, TR_TAB_H = 32, TR_TAB_GAP = 8, TR_TAB_Y = 64, TR_HEAD_Y = 110, TR_ROW_H = 20, TR_NUM_W = 44, TR_CH_W = 150,
       TR_NOTE_X = 0, TR_INS_X = 44, TR_VOL_X = 72, TR_FX_X = 100, TR_TABS = 2, TR_NO_BAR = 0xFFFF };

static sel_gui_rect tr_tab_rect(desk* d, sel_gui_rect mn, uint32_t k) {
    return rect_of(mn.x + lpx(d, HOME_MARGIN) + (int32_t)k * lpx(d, TR_TAB_W + TR_TAB_GAP), mn.y + lpx(d, page_y(d, TR_TAB_Y)), lpx(d, TR_TAB_W),
                   lpx(d, TR_TAB_H));
}

/* Rows that fit under the header line of the main region, at most one pattern window. */
static uint32_t tr_rows_fit(desk* d, sel_gui_rect mn) {
    int32_t room = mn.h - lpx(d, page_y(d, TR_HEAD_Y) + 2 * TR_ROW_H + 8);
    uint32_t n = room > 0 ? (uint32_t)(room / lpx(d, TR_ROW_H)) : 0u;
    return n > SEL_PATTERN_ROWS ? SEL_PATTERN_ROWS : n;
}

/* Row i below the column header line (pattern tab) or below the header line (sample tab). */
static sel_gui_rect tr_row_rect(desk* d, sel_gui_rect mn, uint32_t i) {
    return rect_of(mn.x + lpx(d, HOME_MARGIN), mn.y + lpx(d, page_y(d, TR_HEAD_Y) + 2 * TR_ROW_H) + (int32_t)i * lpx(d, TR_ROW_H),
                   mn.w - 2 * lpx(d, HOME_MARGIN), lpx(d, TR_ROW_H));
}

static sel_gui_rect tr_chan_rect(desk* d, sel_gui_rect mn, uint32_t c) {
    return rect_of(mn.x + lpx(d, HOME_MARGIN + TR_NUM_W) + (int32_t)c * lpx(d, TR_CH_W), mn.y + lpx(d, page_y(d, TR_HEAD_Y) + TR_ROW_H),
                   lpx(d, TR_CH_W), lpx(d, TR_ROW_H));
}

static uint32_t tr_cols_fit(desk* d, sel_gui_rect mn) {
    int32_t room = mn.w - 2 * lpx(d, HOME_MARGIN) - lpx(d, TR_NUM_W);
    return room > 0 ? (uint32_t)(room / lpx(d, TR_CH_W)) : 0u;
}

static uint32_t tr_sample_count(desk* d) {
    sel_rp_sample s;
    memset(&s, 0, sizeof s);
    return sel_engine_sample_get(d->engine, 1u, &s) ? s.count : 0u;
}

static void draw_trackerpage(desk* d) {
    const sel_state* st = sel_engine_state(d->engine);
    const sel_pattern_view* v = st->pattern;
    sel_gui_rect mn = draw_panel(d, "main");
    int32_t mg = lpx(d, HOME_MARGIN), rad = lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_RADIUS]);
    uint32_t text = ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT]), muted = ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT_MUTED]);
    uint32_t surf = ARGB(d->theme.token[SEL_GUI_TOKEN_SURFACE]), acc = ARGB(d->theme.token[SEL_GUI_TOKEN_ACCENT]);
    uint32_t acct = ARGB(d->theme.token[SEL_GUI_TOKEN_ACCENT_TEXT]);
    static const char* const tab_name[TR_TABS] = {"Pattern", "Samples"};
    char buf[160];
    uint32_t k, r, c, fit = tr_rows_fit(d, mn);
    sel_gui_clip_push(d->canvas, mn);
    draw_text(d, rect_of(mn.x + mg, mn.y + lpx(d, 16), mn.w - 2 * mg, lpx(d, page_title_h(d))), "Tracker", 1, 32, text);
    for (k = 0; k < TR_TABS; k++) {
        sel_gui_rect b = tr_tab_rect(d, mn, k);
        draw_button(d, b, k == d->tr_tab ? acc : surf, tab_name[k], 13, k == d->tr_tab ? acct : text);
    }
    d->tr_rows = 0;
    d->tr_bar = TR_NO_BAR;
    if (d->tr_tab == 0u) {
        if (v == NULL || !v->valid) {
            draw_text(d, rect_of(mn.x + mg, mn.y + lpx(d, page_y(d, TR_HEAD_Y)), mn.w - 2 * mg, lpx(d, TR_ROW_H)), "No pattern data", 0, 13, muted);
        } else {
            uint32_t cols = v->channels < tr_cols_fit(d, mn) ? v->channels : tr_cols_fit(d, mn);
            snprintf(buf, sizeof buf, "Order %u of %u, Pattern %u, Row %u of %u", (unsigned)(st->song.pattern_pos.order + 1u),
                     (unsigned)v->info.orders, (unsigned)st->song.pattern_pos.pattern, (unsigned)(st->song.pattern_pos.row + 1u),
                     (unsigned)v->info.rows);
            draw_text(d, rect_of(mn.x + mg, mn.y + lpx(d, page_y(d, TR_HEAD_Y)), mn.w - 2 * mg, lpx(d, TR_ROW_H)), buf, 0, 13, muted);
            for (c = 0; c < cols; c++) {
                int off = (st->mute_mask >> c) & 1u;
                snprintf(buf, sizeof buf, off ? "Channel %u Muted" : "Channel %u", (unsigned)(c + 1u));
                draw_text(d, tr_chan_rect(d, mn, c), buf, 1, 13, off ? muted : text);
            }
            /* The playing row holds the middle slot and the pattern scrolls past it; slots before row 0 or past the last
             * row stay blank, and rows the answered window does not hold (yet) are skipped (I283). */
            for (r = 0; r < fit; r++) {
                int64_t srow = (int64_t)st->song.pattern_pos.row + (int64_t)r - (int64_t)(fit / 2u);
                uint32_t row = (uint32_t)srow;
                sel_gui_rect rr = tr_row_rect(d, mn, r);
                int at = r == fit / 2u;
                if (at) {
                    sel_gui_round_rect(d->canvas, rr, rad, acc);
                    d->tr_bar = r;
                }
                if (srow < 0 || row >= v->info.rows || v->order != st->song.pattern_pos.order || row < v->row_lo ||
                    row >= v->row_lo + v->row_n) {
                    continue;
                }
                snprintf(buf, sizeof buf, "%02X", (unsigned)row);
                draw_text(d, rect_of(rr.x, rr.y, lpx(d, TR_NUM_W), rr.h), buf, 0, 13, at ? acct : muted);
                for (c = 0; c < cols; c++) {
                    char note[4], ins[4], vol[4], fx[8];
                    int32_t x = mn.x + mg + lpx(d, TR_NUM_W) + (int32_t)c * lpx(d, TR_CH_W);
                    uint32_t col = at ? acct : ((st->mute_mask >> c) & 1u) ? muted : text;
                    sel_uim_cell_fields(&v->cell[row - v->row_lo][c], note, ins, vol, fx);
                    draw_text(d, rect_of(x + lpx(d, TR_NOTE_X), rr.y, lpx(d, 44), rr.h), note, 0, 13, col);
                    draw_text(d, rect_of(x + lpx(d, TR_INS_X), rr.y, lpx(d, 28), rr.h), ins, 0, 13, col);
                    draw_text(d, rect_of(x + lpx(d, TR_VOL_X), rr.y, lpx(d, 28), rr.h), vol, 0, 13, col);
                    draw_text(d, rect_of(x + lpx(d, TR_FX_X), rr.y, lpx(d, 44), rr.h), fx, 0, 13, col);
                }
                d->tr_rows++;
            }
        }
    } else {
        uint32_t n = tr_sample_count(d);
        draw_text(d, rect_of(mn.x + mg, mn.y + lpx(d, page_y(d, TR_HEAD_Y)), mn.w - 2 * mg, lpx(d, TR_ROW_H)),
                  n > 0u ? "Click a sample to play it" : "No samples", 0, 13, muted);
        for (k = 0; k < n && k < fit; k++) {
            sel_rp_sample s;
            sel_gui_rect rr = tr_row_rect(d, mn, k);
            memset(&s, 0, sizeof s);
            if (!sel_engine_sample_get(d->engine, k + 1u, &s)) {
                continue;
            }
            snprintf(buf, sizeof buf, "%02u", (unsigned)(k + 1u));
            draw_text(d, rect_of(rr.x, rr.y, lpx(d, 36), rr.h), buf, 0, 13, row_ink(d, rr, muted));
            draw_text(d, rect_of(rr.x + lpx(d, 40), rr.y, rr.w / 2, rr.h), s.name != NULL && s.name[0] != '\0' ? s.name : "(unnamed)", 0, 13,
                      row_ink(d, rr, text));
            snprintf(buf, sizeof buf, "%u frames  %u Hz", (unsigned)s.length, (unsigned)s.rate);
            draw_text(d, rect_of(rr.x + rr.w / 2, rr.y, rr.w / 2, rr.h), buf, 0, 13, row_ink(d, rr, muted));
            d->tr_rows++;
        }
    }
    if (d->tr_tab == 0u && v != NULL && v->valid) {
        uint32_t pos = (uint32_t)st->song.pattern_pos.order << 16 | st->song.pattern_pos.row;
        if (d->tr_draws > 0u && pos != d->tr_pos) {
            d->tr_moves++;
        }
        d->tr_pos = pos;
    }
    d->tr_draws++;
    sel_gui_clip_pop(d->canvas);
    sel_gui_dirty_add(d->canvas, mn);
}

/* Asks for the pattern window around the playing row when the position moved, and flags a redraw when what the page
 * shows changed (answered window, position, mute mask, sample count, tab). */
static void tr_tick(desk* d) {
    const sel_state* st = sel_engine_state(d->engine);
    sel_gui_rect mn = region_rect(d, "main");
    uint32_t n = tr_rows_fit(d, mn), row = st->song.pattern_pos.row, order = st->song.pattern_pos.order, sig;
    if (n > 0u && (!d->tr_asked || d->tr_order != order || d->tr_row != row || d->tr_qid != st->song.queue_id)) {
        uint32_t lo = row > n / 2u ? row - n / 2u : 0u;
        if (sel_engine_pattern_request(d->engine, order, lo, n) != 0) {
            d->tr_asked = 1;
            d->tr_order = order;
            d->tr_row = row;
            d->tr_qid = st->song.queue_id;
        }
    }
    sig = 2166136261u;
    sig = (sig ^ (st->pattern != NULL ? st->pattern->seq : 0u)) * 16777619u;
    sig = (sig ^ order) * 16777619u;
    sig = (sig ^ row) * 16777619u;
    sig = (sig ^ st->mute_mask) * 16777619u;
    sig = (sig ^ tr_sample_count(d)) * 16777619u;
    sig = (sig ^ d->tr_tab) * 16777619u;
    if (sig != d->tr_sig) {
        d->tr_sig = sig;
        d->uim.dirty |= SEL_UIM_DIRTY_ALL;
    }
}

static int tr_send(desk* d, const char* line) {
    d->tr_cmds++;
    return desk_send(d, line);
}

/* A click: a tab, a channel header (mutes the channel) or a sample row (plays the sample). */
static int tr_pointer(desk* d, const sel_gui_input* ev) {
    sel_gui_rect mn = region_rect(d, "main");
    const sel_state* st = sel_engine_state(d->engine);
    char line[48];
    uint32_t k, fit = tr_rows_fit(d, mn);
    if (ev->kind != SEL_GUI_IN_CLICK || ev->button != 0u) {
        return 0;
    }
    for (k = 0; k < TR_TABS; k++) {
        if (in_rect(tr_tab_rect(d, mn, k), ev->x, ev->y)) {
            d->tr_tab = k;
            d->repaint = 1;
            d->uim.dirty |= SEL_UIM_DIRTY_ALL;
            return 1;
        }
    }
    if (d->tr_tab == 0u && st->pattern != NULL) {
        for (k = 0; k < st->pattern->channels && k < tr_cols_fit(d, mn); k++) {
            if (in_rect(tr_chan_rect(d, mn, k), ev->x, ev->y)) {
                snprintf(line, sizeof line, "MUTE %u toggle", (unsigned)(k + 1u));
                return tr_send(d, line);
            }
        }
    } else if (d->tr_tab == 1u) {
        uint32_t n = tr_sample_count(d);
        for (k = 0; k < n && k < fit; k++) {
            if (in_rect(tr_row_rect(d, mn, k), ev->x, ev->y)) {
                snprintf(line, sizeof line, "SAMPLE_PLAY %u", (unsigned)(k + 1u));
                return tr_send(d, line);
            }
        }
    }
    return 0;
}

static void draw_songinfo(desk* d) {
    const sel_state* st = sel_engine_state(d->engine);
    sel_gui_rect mn = draw_panel(d, "main"), nm = pl_name_rect(d, mn);
    int32_t mg = lpx(d, HOME_MARGIN), rad = lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_RADIUS]);
    uint32_t text = ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT]), muted = ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT_MUTED]);
    uint32_t surf = ARGB(d->theme.token[SEL_GUI_TOKEN_SURFACE]);
    uint32_t i;
    d->empty_text[0] = '\0';
    sel_gui_clip_push(d->canvas, mn);
    draw_text(d, rect_of(mn.x + mg, mn.y + lpx(d, 16), mn.w - 2 * mg, lpx(d, page_title_h(d))), "Song Information", 1, 32, text);
    song_row_reg(d, rect_of(mn.x + mg, mn.y + lpx(d, 16), mn.w - 2 * mg, lpx(d, page_title_h(d))), st->song.path, 0, 0, 0); /* the open song (I372) */
    for (i = 0; i < SI_BUTTONS; i++) {
        sel_gui_rect b = queue_btn_rect(d, mn, i);
        draw_button(d, b, surf, si_button_label(i), 13, text);
    }
    sel_gui_round_rect(d->canvas, nm, rad, surf);
    draw_text(d, rect_of(nm.x + lpx(d, 12), nm.y, nm.w - lpx(d, 24), nm.h), d->si_nlen > 0u ? d->si_note : "Note", d->si_edit, 13,
              d->si_nlen > 0u ? text : muted);
    d->si_rows = 0;
    sel_gui_clip_push(d->canvas, rect_of(mn.x, si_list_top(d, mn), mn.w, mn.y + mn.h - si_list_top(d, mn)));
    memset(&d->si_composer, 0, sizeof d->si_composer);
    si_walk_from_top(d, st, si_draw_row);
    sel_gui_clip_pop(d->canvas);
    if (d->si_rows == 0u) {
        draw_empty_state(d, mn, si_list_top(d, mn), sel_uim_state_text(SEL_UIM_SV_NOWPLAYING, SEL_UIM_ST_EMPTY));
    }
    d->si_draws++;
    sel_gui_clip_pop(d->canvas);
    sel_gui_dirty_add(d->canvas, mn);
}

static int si_send(desk* d, const char* line) {
    d->si_cmds++;
    return desk_send(d, line);
}

/* A click: a flag button (the note field's text goes with Mark Wrong and Save Note) or the note field. */
static int si_pointer(desk* d, const sel_gui_input* ev) {
    sel_gui_rect mn = region_rect(d, "main");
    char line[160];
    uint32_t k;
    if (ev->kind != SEL_GUI_IN_CLICK || ev->button != 0u) {
        return 0;
    }
    if (d->si_composer.w > 0 && d->si_composer_key[0] != '\0' && in_rect(d->si_composer, ev->x, ev->y)) { /* I678h3: the composer opens the composer page */
        char canon[SEL_COMPOSER_MAX];
        snprintf(canon, sizeof canon, "%s", d->si_composer_key);
        desk_route(d, SEL_ROUTE_SONGINFO_COMPOSER, (uint32_t)SEL_LIB_ITEM_COMPOSER, 0u, canon, canon, "");
        return 1;
    }
    for (k = 0; k < SI_BUTTONS; k++) {
        if (in_rect(queue_btn_rect(d, mn, k), ev->x, ev->y)) {
            sel_uim_flag_line(k, d->si_note, line, sizeof line);
            d->si_edit = 0;
            return si_send(d, line);
        }
    }
    d->si_edit = in_rect(pl_name_rect(d, mn), ev->x, ev->y);
    d->repaint = 1;
    d->uim.dirty |= SEL_UIM_DIRTY_ALL;
    return 1;
}

/* A typed key while the note field is focused: printable keys append, Backspace deletes, Enter and Escape leave. */
static int si_key(desk* d, const sel_gui_input* ev) {
    if (ev->code == 13u || ev->code == 27u) {
        d->si_edit = 0;
    } else if (ev->code == 127u) {
        if (d->si_nlen > 0u) {
            d->si_note[--d->si_nlen] = '\0';
        }
    } else if (ev->code >= 32u && ev->code < 127u && ev->code != '"' && (ev->mods & (TXT_MOD_CTRL | TXT_MOD_ALT)) == 0u) {
        if (d->si_nlen + 1u < sizeof d->si_note) {
            d->si_note[d->si_nlen++] = (char)ev->code;
            d->si_note[d->si_nlen] = '\0';
        }
    } else {
        return 0;
    }
    d->repaint = 1;
    d->uim.dirty |= SEL_UIM_DIRTY_ALL;
    return desk_present(d);
}

static const char* lib_kind_name(uint32_t kind) {
    switch (kind) {
    case SEL_ROW_NODE: return "node";
    case SEL_ROW_DIR: return "dir";
    case SEL_ROW_FILE: return "file";
    case SEL_ROW_HEADING: return "heading";
    case SEL_ROW_HOME: return "play";
    default: return "song";
    }
}

/* I244: the action-bound controls besides the transport row (see desk_ctl_rect). */
enum { CTL_VIZ_N = 6, CTL_LIKE = 6, CTL_NP_ADD = 7, CTL_NP_PANEL = 8, CTL_COUNT = 9 }; /* I619: the add button and the panel toggle */
#define TIP_CTL_BASE ((int32_t)(TRANSPORT_COUNT + DOOR_COUNT + LIB_STYLES))
static sel_gui_rect desk_ctl_rect(const desk* d, uint32_t c); /* defined with the tooltip tracker */
static const char* desk_ctl_action(const desk* d, uint32_t c);

/* Appends one formatted line to library_dump at offset o and returns the new offset, never past the buffer: snprintf
 * returns the length it wanted, so a long Library (I303) would push o beyond the end and the next write below it. */
static size_t dump_put(desk* d, size_t o, const char* fmt, ...) {
    va_list ap;
    int n;
    if (o >= sizeof d->library_dump) return sizeof d->library_dump - 1u;
    va_start(ap, fmt);
    n = vsnprintf(d->library_dump + o, sizeof d->library_dump - o, fmt, ap);
    va_end(ap);
    if (n < 0) return o;
    o += (size_t)n;
    return o < sizeof d->library_dump ? o : sizeof d->library_dump - 1u;
}

/* Writes "composer I canon|display|songs", "row I x y w h" (the click rectangle), "drill display|songs" and "stat label|value" lines into library_dump. */
static const char* library_dump_build(desk* d) {
    const sel_state* st = sel_engine_state(d->engine);
    size_t o = 0;
    uint32_t i = 0;
    const char* p;
    d->library_dump[0] = '\0';
    for (p = st->composers; p != NULL && *p != '\0' && o + 400u < sizeof d->library_dump; i++) {
        const char* disp = lib_next(p);
        const char* cnt = lib_next(disp);
        o = dump_put(d, o, "composer %u %s|%s|%s\n", (unsigned)i, p, disp, cnt);
        {
            sel_gui_rect rr = lib_row_rect(d, panel_rect(d, "main"), i);
            o = dump_put(d, o, "row %u %d %d %d %d\n", (unsigned)i, (int)rr.x, (int)rr.y, (int)rr.w, (int)rr.h);
        }
        p = lib_next(cnt);
    }
    o = dump_put(d, o, "folders tabs_drawn=%u library_remaps=%u browse_folders_opens=%u\n", (unsigned)d->lib_tabs_drawn, (unsigned)d->library_remaps, (unsigned)d->browse_folders_opens);
    o = dump_put(d, o, "mode %s\nscroll %d\nlibempty %s\n", sel_uim_libmode_name(d->lib_mode), (int)(d->lib_q16 >> 16), d->empty_text);
    for (i = 0; i < LIB_TABS; i++) {
        sel_gui_rect tr = lib_tab_rect(d, panel_rect(d, "main"), i);
        o = dump_put(d, o, "tab %u %d %d %d %d\n", (unsigned)i, (int)tr.x, (int)tr.y, (int)tr.w, (int)tr.h);
    }
    {
        uint32_t k, n = d->nagdlg_n, idx;
        sel_gui_rect pm = panel_rect(d, "main");
        o = dump_put(d, o, "nag %u syncs %u\n", (unsigned)(d->dlg_open == DLG_SYNC ? d->nagdlg_n : 0u), (unsigned)d->nag_syncs);
        if (d->dlg_open == DLG_PLDEL) { /* I647: the Delete confirm's buttons, for a script to click */
            sel_gui_rect cb = dlg_button_rect(d, 1u);
            o = dump_put(d, o, "dlgconfirm %d %d %d %d\n", (int)cb.x, (int)cb.y, (int)cb.w, (int)cb.h);
        }
        o = dump_put(d, o, "nagdlg opens %u done %d\nliststart %d\n", (unsigned)d->nagdlg_opens, d->nag_done, (int)(pm.y + lib_list_y(d)));
        if (d->dlg_open == DLG_SYNC) {
            sel_gui_rect sy = nagdlg_button_rect(d, 2u), lt = nagdlg_button_rect(d, 1u), nv = nagdlg_button_rect(d, 0u), pn = dlg_panel_rect(d);
            o = dump_put(d, o, "nagpanel %d %d %d %d\nnagsync %d %d %d %d\nnaglater %d %d %d %d\nnagnever %d %d %d %d\n", (int)pn.x, (int)pn.y, (int)pn.w, (int)pn.h,
                         (int)sy.x, (int)sy.y, (int)sy.w, (int)sy.h, (int)lt.x, (int)lt.y, (int)lt.w, (int)lt.h, (int)nv.x, (int)nv.y, (int)nv.w, (int)nv.h);
            for (k = 0; k < n; k++) {
                sel_gui_rect row = nagdlg_row_rect(d, k), bx = nagdlg_box_rect(d, row);
                idx = d->nagdlg_idx[k];
                o = dump_put(d, o, "nagrow %u %s|%d %d %d %d %d\n", (unsigned)k, d->src[idx].key, !(d->nag_off & (1u << idx)), (int)bx.x, (int)bx.y, (int)bx.w, (int)bx.h);
            }
        }
    }
    {
        sel_gui_rect bt, bh;
        if (lib_bar_rects(d, &bt, &bh)) { /* I541: the Library scrollbar, and where the list is scrolled to */
            o = dump_put(d, o, "libbar %d %d %d %d handle %d %d %d %d q16 %d draws %u\n", (int)bt.x, (int)bt.y, (int)bt.w, (int)bt.h, (int)bh.x, (int)bh.y,
                         (int)bh.w, (int)bh.h, (int)d->lib_q16, (unsigned)d->lib_bar_draws);
        } else {
            o = dump_put(d, o, "libbar none\n");
        }
        o = dump_put(d, o, "libheld presents %u px_max %u\n", (unsigned)d->lib_held_n, (unsigned)d->lib_held_px_max);
    }
    {
        sel_gui_rect cr = lib_crumb_rect(d, panel_rect(d, "main"));
        o = dump_put(d, o, "crumb %d %d %d %d\n", (int)cr.x, (int)cr.y, (int)cr.w, (int)cr.h);
    }
    if (lib_view_on(d, st)) {
        const sel_view_state* v = st->view;
        for (i = 0; i < v->drill_n && i < 2u; i++) {
            o = dump_put(d, o, "picked %u|%s\n", (unsigned)i, v->drill[i]);
        }
        for (i = 0; i < v->row_n && o + 400u < sizeof d->library_dump; i++) {
            const sel_view_row* vr = &v->rows[i];
            sel_gui_rect rr = lib_row_rect(d, panel_rect(d, "main"), i);
            o = dump_put(d, o, "vrow %u %s|%u|%u|%u|%s\n", (unsigned)i,
                                  lib_kind_name(vr->kind), (unsigned)vr->depth, (unsigned)vr->count,
                                  (unsigned)vr->expanded, vr->text != NULL ? vr->text : "");
            o = dump_put(d, o, "vrect %u %d %d %d %d\n", (unsigned)i, (int)rr.x, (int)rr.y, (int)rr.w, (int)rr.h);
        }
    }
    if (st->view != NULL && st->view->style >= SEL_VIEW_STYLE_GRID) { /* grid, cover flow, compact: the rows and their artwork ids */
        const sel_view_state* v = st->view;
        o = dump_put(d, o, "vstyle %s\n", sel_view_style_label(v->style));
        for (i = 0; i < v->row_n && o + 400u < sizeof d->library_dump; i++) {
            o = dump_put(d, o, "srow %u %u|%s|%s\n", (unsigned)i,
                                  (unsigned)v->rows[i].art_id, v->rows[i].text != NULL ? v->rows[i].text : "",
                                  v->rows[i].artist != NULL ? v->rows[i].artist : "");
        }
    }
    for (i = 0; i < (uint32_t)LIB_STYLES; i++) {
        sel_gui_rect sr = lib_style_rect(d, panel_rect(d, "main"), i);
        o = dump_put(d, o, "style_chooser %u %s %d %d %d %d selected=%d\n", (unsigned)i, lib_style_name[i], (int)sr.x, (int)sr.y,
                     (int)sr.w, (int)sr.h, lib_style_selected(st) == i);
    }
    o = dump_put(d, o, "grid draws %u tiles %u cols %u\nflow draws %u centre %u\n",
                          (unsigned)d->grid_draws, (unsigned)d->grid_tiles, (unsigned)d->grid_cols, (unsigned)d->flow_draws,
                          (unsigned)d->flow_centre);
    o = dump_put(d, o, "flow cover %d %d %d %d area %d %d %d %d\n", (int)d->flow_cover.x, (int)d->flow_cover.y, (int)d->flow_cover.w,
                 (int)d->flow_cover.h, (int)d->flow_area.x, (int)d->flow_area.y, (int)d->flow_area.w, (int)d->flow_area.h);
    o = dump_put(d, o, "flow covers %u", (unsigned)d->flow_rec_n);
    for (i = 0; i < d->flow_rec_n; i++) { /* index:x:size:|p| in 16.16, in draw order (far to near) */
        o = dump_put(d, o, " %d:%d:%d:%d", (int)d->flow_rec[i].g, (int)d->flow_rec[i].x, (int)d->flow_rec[i].w, (int)d->flow_rec[i].a);
    }
    o = dump_put(d, o, "\n%s", d->flow_snaps);
    o = dump_put(d, o, "grid box %d %d %d %d\n", (int)d->grid_box.x, (int)d->grid_box.y, (int)d->grid_box.w, (int)d->grid_box.h);
    {
        /* I449: the first section heading's bottom edge and the top of the first tile's shadow (the cover top less the shadow
         * radius), plus the spacing token that gap must reach; shelf mode and the flat tile grid lay out differently. */
        sel_gui_rect mn = region_rect(d, "main");
        int shelves = home_shelf_mode(sel_engine_state(d->engine));
        sel_gui_rect t0 = shelves ? shelf_tile_rect(d, 0u, 0u) : tile_rect(d, 0u);
        int32_t pad0 = lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_TILE_PAD]);
        int32_t head_bottom = (shelves ? shelf_top(d, 0u) : mn.y + lpx(d, HOME_SECTION_Y)) + lpx(d, HOME_SECTION_H);
        o = dump_put(d, o, "home head bottom %d shadow top %d token %d\n", (int)head_bottom, (int)(t0.y + pad0 - lpx(d, HOME_SHADOW_R)),
                     (int)lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_SPACING]));
    }
    if (st->stats != NULL) {
        sel_gui_rect sr = lib_stats_rect(d, panel_rect(d, "main"), st);
        sel_gui_rect mr = lib_stats_more_rect(d, sr), pr = lib_stats_panel_rect(d, sr, st);
        o = dump_put(d, o, "stats rect %d %d %d %d\n", (int)sr.x, (int)sr.y, (int)sr.w, (int)sr.h);
        {
            sel_gui_rect ar = lib_add_rect(d, panel_rect(d, "main")), cr = lib_clear_rect(d, panel_rect(d, "main"));
            const sel_gui_atlas* ta = desk_text_atlas(d, 1, 32, lpx(d, page_title_h(d)));
            o = dump_put(d, o, "lib_header title %d %d %d add %d %d %d %d clear %d %d %d %d\n", (int)(panel_rect(d, "main").x + lpx(d, LIB_MARGIN)),
                         (int)(panel_rect(d, "main").y + lpx(d, LIB_TITLE_Y)), (int)(ta != NULL ? sel_gui_text_width(ta, "Library", 7u) : 0),
                         (int)ar.x, (int)ar.y, (int)ar.w, (int)ar.h, (int)cr.x, (int)cr.y, (int)cr.w, (int)cr.h);
        }
        o = dump_put(d, o, "stats_more %d %d %d %d open=%d\nstats_panel draws=%u rows=%u rect %d %d %d %d\n", (int)mr.x, (int)mr.y, (int)mr.w, (int)mr.h,
                     d->stats_open, (unsigned)d->stats_panel_draws, (unsigned)d->stats_panel_rows, (int)pr.x, (int)pr.y, (int)pr.w, (int)pr.h);
    }
    o = dump_put(d, o, "nowplaying inset left %d right %d token %d\nnowplaying draws %u pairs %u genart %u\nlyrics line %d at %u\nmessage lines %u\n",
                          (int)d->np_inset_l, (int)d->np_inset_r, (int)lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_SPACING]),
                          (unsigned)d->np_draws, (unsigned)d->np_rows, (unsigned)d->np_genart, (int)st->lyric_line, (unsigned)st->song.position_ms, (unsigned)d->np_msg_lines);
    o = dump_put(d, o, "nowplaying art %d %d %d %d panel %d %d %d %d\n", (int)d->np_art.x, (int)d->np_art.y, (int)d->np_art.w,
                          (int)d->np_art.h, (int)d->np_panel.x, (int)d->np_panel.y, (int)d->np_panel.w, (int)d->np_panel.h);
    o = dump_put(d, o, "nowplaying title %d %d like %d artist %u\n", (int)d->np_title.y, (int)d->np_title.h, (int)d->np_like.y,
                 (unsigned)(d->np_text_artist[0] != '\0'));
    o = dump_put(d, o, "cover_tiles np=%u bar=%u side=%u\n", (unsigned)d->cover_np, (unsigned)d->cover_bar, (unsigned)d->cover_side);
    o = dump_put(d, o, "nowplaying fade %u\n", (unsigned)d->np_fade);
    o = dump_put(d, o, "platform_covers np=%s bar=%s page=%s tile0=%s draws=%u shelf=%s grid=%s flow=%s side=%s header=%s\n", pc_name(d->pc_np), pc_name(d->pc_bar),
                 pc_name(d->pc_page), pc_name(d->pc_tile0), (unsigned)d->pc_draws, pc_name(d->pc_shelf), pc_name(d->pc_grid), pc_name(d->pc_flow),
                 pc_name(d->pc_side), pc_name(d->pc_header));
    o = dump_put(d, o, "platform_cover_words np=%u bar=%u page=%u tile0=%u\n", (unsigned)pc_word(d->pc_np), (unsigned)pc_word(d->pc_bar),
                 (unsigned)pc_word(d->pc_page), (unsigned)pc_word(d->pc_tile0));
    o = dump_put(d, o, "nowplaying keys end %d value %d\n",(int)d->np_key_end, (int)d->np_val_x);
    for (uint32_t tg = 0; tg <= SONG_PLAYLIST_TARGETS; tg += SONG_PLAYLIST_TARGETS) { /* I448: first and last drop target, then the rail */
        sel_gui_rect tr = songs_target_rect(d, tg);
        o = dump_put(d, o, "songs_target %u %d %d %d %d\n", (unsigned)tg, (int)tr.x, (int)tr.y, (int)tr.w, (int)tr.h);
    }
    {
        sel_gui_rect rl = region_rect(d, "sidebar");
        o = dump_put(d, o, "songs_focus row %d fill %06x activates %u plays %u playing %d\n", (int)d->row_focus,
                     (unsigned)(ARGB(d->theme.token[SEL_GUI_TOKEN_SURFACE_RAISED]) & 0xFFFFFFu), (unsigned)d->kb_activates, (unsigned)d->row_plays,
                     d->row_focus >= 0 && (uint32_t)d->row_focus < d->order_n && st->song.path != NULL &&
                         strcmp(st->song.path, st->queue_text + st->queue[d->order[d->row_focus]].path_offset) == 0);
        {
            const sel_view_state* pv = pl_view(st);
            int32_t row = d->queue_page ? d->q_focus : d->pl_page && d->pls_id != 0u && d->col_kind != 0u ? d->col_focus : d->pl_page ? pl_sel_row(d, pv) : d->page == PG_FOLDERS && lib_ui_list(d, st) && !lib_grid_on(st) ? d->lib_focus
                          : d->kb_list && st->view != NULL ? (int32_t)st->view->cursor : -1;
            o = dump_put(d, o, "list_focus page %d row %d activates %u plays %u song=%s\n", (int)d->page, (int)row, (unsigned)d->kb_activates, (unsigned)d->row_plays,
                         st->song.path != NULL ? path_base(st->song.path) : "-");
        }
        o = dump_put(d, o, "rail %d %d %d %d\n", (int)rl.x, (int)rl.y, (int)rl.w, (int)rl.h);
    }
    o = dump_put(d, o, "rating %d liked %d mark %d %d %d %d step %d\n", (int)d->np_rating, (int)d->np_liked,
                          (int)d->np_rate[0].x, (int)d->np_rate[0].y, (int)d->np_rate[0].w, (int)d->np_rate[0].h,
                          (int)(d->np_rate[1].x - d->np_rate[0].x));
    o = dump_put(d, o,
                          "viz frames %u song_ms %u plugin %s plugins %u layout %u panes %u sigpath %u prims %u visual %u strip %d\n",
                          (unsigned)d->viz_frames, (unsigned)d->viz_song_ms, d->viz_n > 0u ? d->viz_id[d->viz_sel] : "none",
                          (unsigned)d->viz_n, (unsigned)d->uim.layout, (unsigned)d->viz_panes, (unsigned)d->viz_sigpath,
                          (unsigned)d->viz_prims, (unsigned)d->viz_visual_draws, d->np_viz.w > 0);
    o = dump_put(d, o, "viz_switch clicks %u strip %d %d %d %d page %d %d %d %d\n",
                          (unsigned)d->viz_clicks, (int)d->viz_btn[0][0].x, (int)d->viz_btn[0][0].y, (int)d->viz_btn[0][0].w,
                          (int)d->viz_btn[0][0].h, (int)d->viz_btn[1][0].x, (int)d->viz_btn[1][0].y, (int)d->viz_btn[1][0].w,
                          (int)d->viz_btn[1][0].h);
    for (p = st->panel, i = 0; p != NULL && *p != '\0' && i < 16u && o + 400u < sizeof d->library_dump; i++) {
        const char* v = p + strlen(p) + 1;
        o = dump_put(d, o, "meta %u %s|%s\n", (unsigned)i, p, v);
        p = v + strlen(v) + 1;
    }
    {
        sel_gui_rect ab = lib_add_rect(d, panel_rect(d, "main"));
        sel_scan_info si;
        sel_engine_scan_info(d->engine, &si);
        o = dump_put(d, o, "addfolder %d %d %d %d\nscan probed %u files %u scans %u\n",
                              (int)ab.x, (int)ab.y, (int)ab.w, (int)ab.h, (unsigned)si.probed, (unsigned)si.scanned_files, (unsigned)d->scans);
    }
    {
        sel_gui_rect pm = panel_rect(d, "main"), tr = toast_rect(d), sr = lib_scan_rect(d, pm), ab = lib_add_rect(d, pm);
        o = dump_put(d, o, "scan_ui page=%d done=%u total=%u file=%s draws=%u pending=%d toast_rect %d %d %d %d add_rect %d %d %d %d scan_rect %d %d %d %d\n",
                              (int)d->page, (unsigned)d->scan_done, (unsigned)d->scan_total, path_base(d->scan_file), (unsigned)d->scan_draws,
                              d->scan_pending, (int)tr.x, (int)tr.y, (int)tr.w, (int)tr.h, (int)ab.x, (int)ab.y, (int)ab.w, (int)ab.h,
                              (int)sr.x, (int)sr.y, (int)sr.w, (int)sr.h);
    }
    o = dump_put(d, o, "toast_text %s\n", d->toast_text[0] != '\0' ? d->toast_text : "-");
    o = dump_put(d, o, "scan_mid %u\n", (unsigned)d->scan_mid);
    o = dump_put(d, o, "search draws %u rows %u query %s\n",
                          (unsigned)d->search_draws, (unsigned)d->search_rows, d->query);
    o = dump_put(d, o, "browse draws=%u tiles=%u opens=%u filter=%d\n", (unsigned)d->browse_draws, (unsigned)d->browse_n, (unsigned)d->browse_opens, d->browse_filter);
    {
        browse_tile bt[BROWSE_MAX];
        uint32_t bn = browse_tiles(d, bt, BROWSE_MAX), bi;
        for (bi = 0; d->page == PG_BROWSE && bi < bn; bi++) {
            o = dump_put(d, o, "browse tile %u %u %s\n", (unsigned)bi, (unsigned)bt[bi].kind, bt[bi].label);
        }
    }
    o = dump_put(d, o, "search view grid=%u flow=%u\n", (unsigned)d->search_grid_draws, (unsigned)d->search_flow_draws);
    o = dump_put(d, o, "search groups songs=%u albums=%u artists=%u facet=%u drawn=%u\n",
                          (unsigned)d->search_groups[0], (unsigned)d->search_groups[1], (unsigned)d->search_groups[2],
                          (unsigned)d->search_facet, (unsigned)d->search_drawn);
    if (d->pl_page) { /* the Playlists page geometry and tree rows (DUI-P4-9) */
        const sel_view_state* pv = pl_view(st);
        sel_gui_rect pmn = region_rect(d, "main"), pn = pl_name_rect(d, pmn);
        o = dump_put(d, o,
                              "pl draws %u rows %u cmds %u opens %u count %u hash %08x sel %u queue %u\n", (unsigned)d->pl_draws,
                              (unsigned)d->pl_rows, (unsigned)d->pl_cmds, (unsigned)d->folder_opens, pv != NULL ? (unsigned)pv->row_n : 0u,
                              (unsigned)pl_hash(pv), (unsigned)d->pl_sel_id, (unsigned)st->queue_count);
        for (i = 0; i < PL_BUTTONS; i++) {
            sel_gui_rect b = queue_btn_rect(d, pmn, i);
            o = dump_put(d, o, "plbtn %u %d %d %d %d\n", (unsigned)i, (int)b.x,
                                  (int)b.y, (int)b.w, (int)b.h);
        }
        o = dump_put(d, o, "empty %s\n", d->empty_text);
        o = dump_put(d, o, "plname 0 %d %d %d %d\n", (int)pn.x, (int)pn.y, (int)pn.w,
                              (int)pn.h);
        for (i = 0; i < PL_SLOTS; i++) { /* the first row slots, rows or not: the geometry a script clicks before a row exists */
            sel_gui_rect r = pl_row_rect(d, pmn, i);
            o = dump_put(d, o, "plslot %u %d %d %d %d\n", (unsigned)i, (int)r.x,
                                  (int)r.y, (int)r.w, (int)r.h);
        }
        for (i = 0; pv != NULL && i < pv->row_n && pl_row_visible(d, pmn, i) && o + 400u < sizeof d->library_dump; i++) {
            o = dump_put(d, o, "plrow %u %s %u %u %s\n", (unsigned)i,
                                  pv->rows[i].kind == (uint32_t)SEL_ROW_FOLDER ? "folder" : "playlist", (unsigned)pv->rows[i].id,
                                  (unsigned)pv->rows[i].depth, pv->rows[i].text);
        }
    }
    if (add_mode(d)) { /* I652b: the Library page in add mode: its playlist, the way back, and the Add controls drawn on the rows */
        sel_gui_rect ab = add_back_rect(d, region_rect(d, "main"));
        uint32_t j;
        o = dump_put(d, o, "libadd id=%u name=[%s] adds=%u sent=%u\nlibaddback %d %d %d %d\n", (unsigned)d->add_id, d->add_name,
                     (unsigned)d->add_drawn, (unsigned)d->pls_adds, (int)ab.x, (int)ab.y, (int)ab.w, (int)ab.h);
        for (j = 0; j < d->sm_n && o + 400u < sizeof d->library_dump; j++) {
            sel_gui_rect ar = add_btn_rect(d, d->sm_rows[j].r);
            if (d->sm_rows[j].more.w > 0) {
                o = dump_put(d, o, "libaddrow %u %d %d %d %d key=%s\n", (unsigned)j, (int)ar.x, (int)ar.y, (int)ar.w, (int)ar.h, d->sm_rows[j].ref.path);
            }
        }
    }
    if (d->pl_page && d->pls_id != 0u) { /* I633: the open playlist page: its songs in order and the geometry a script clicks */
        sel_gui_rect pmn = region_rect(d, "main"), bk = pls_back_rect(d, pmn), pp = pls_play_rect(d, pmn), pm = pls_more_rect(d, pmn);
        uint32_t j;
        o = dump_put(d, o, "plpage id=%u name=[%s] songs=%u rows=%u draws=%u back=%d,%d,%d,%d play=%d,%d,%d,%d edit=%d\n", (unsigned)d->pls_id, d->pls_name,
                     (unsigned)d->pls_n, (unsigned)d->pls_rows, (unsigned)d->pls_draws, (int)bk.x, (int)bk.y, (int)bk.w, (int)bk.h,
                     (int)pp.x, (int)pp.y, (int)pp.w, (int)pp.h, d->pl_edit);
        o = dump_put(d, o, "plmore %d %d %d %d\n", (int)pm.x, (int)pm.y, (int)pm.w, (int)pm.h);
        { /* I678e: the collection header, action row and the engine's open sentinel */
            sel_gui_rect cs = col_sort_rect(d, pmn), cf = col_find_rect(d, pmn);
            sel_engine_stats cst;
            sel_engine_stats_get(d->engine, &cst);
            o = dump_put(d, o, "collection kind=%s opens=%u dominants=%u cover=%d color=%08x top=%08x sort=%u q=[%s] shown=%u of=%u search=%d sortbtn=%d,%d,%d,%d find=%d,%d,%d,%d\n",
                         d->col_kind == 0u ? "playlist" : d->col_kind == (uint32_t)SEL_LIB_ITEM_ALBUM ? "album" : d->col_kind == (uint32_t)SEL_LIB_ITEM_ARTIST ? "artist" : d->col_kind == (uint32_t)SEL_LIB_ITEM_COMPOSER ? "composer" : d->col_kind == COL_LIKED ? "liked" : d->col_kind == (uint32_t)SEL_LIB_ITEM_RECENT ? "recent" : d->col_kind == (uint32_t)SEL_LIB_ITEM_LOCAL ? "local" : "source",
                         (unsigned)cst.collection_opens, (unsigned)art_dominants(d->art), d->col_cover, (unsigned)d->col_color, (unsigned)d->col_top,
                         (unsigned)d->col_sort, d->col_q, (unsigned)d->pls_vn, (unsigned)d->pls_n, d->col_search, (int)cs.x, (int)cs.y, (int)cs.w, (int)cs.h,
                         (int)cf.x, (int)cf.y, (int)cf.w, (int)cf.h);
            if (d->col_kind != 0u) { /* I678f: the kind page's shelves, the Popular limit and the geometry a script clicks */
                sel_gui_rect mr = col_more_rect(d, pmn), c0 = col_card_rect(d, pmn, 0u, 0u), c2 = col_card_rect(d, pmn, 2u, 0u), sh = pls_more_rect(d, pmn);
                o = dump_put(d, o, "kindpage label=%s key=[%s] sub=[%s] queries=%u popular=%u rows=%u shelf albums=%u appears=%u more=%u spellings=%u showmore=%d,%d,%d,%d albumcard=%d,%d,%d,%d morecard=%d,%d,%d,%d name=[%s] year=%u shuffle=%d,%d,%d,%d\n",
                             col_kind_label(d), d->col_key, d->col_sub, (unsigned)cst.collection_queries, (unsigned)d->col_popular, (unsigned)col_song_rows(d),
                             (unsigned)d->col_shelf_total[0], (unsigned)d->col_shelf_total[1], (unsigned)d->col_shelf_total[2], (unsigned)d->col_shelf_total[3],
                             (int)mr.x, (int)mr.y, (int)mr.w, (int)mr.h, (int)c0.x, (int)c0.y, (int)c0.w, (int)c0.h, (int)c2.x, (int)c2.y, (int)c2.w, (int)c2.h,
                             d->pls_name, (unsigned)d->col_year, (int)sh.x, (int)sh.y, (int)sh.w, (int)sh.h);
                {
                    char khst[64];
                    sel_gui_rect hrow = col_src_row(d, pmn), stg = src_toggle_rect(d, hrow), ssy = src_sync_rect(d, hrow);
                    uint32_t si = col_src_index(d);
                    col_src_status(d, si, khst, sizeof khst);
                    o = dump_put(d, o, "kindhead wordmark=%d played=%d source=%d status=[%s] toggle=%d,%d,%d,%d sync=%d,%d,%d,%d first=[%s]\n", col_wordmark(d),
                                 d->col_kind == (uint32_t)SEL_LIB_ITEM_RECENT, d->col_kind == (uint32_t)SEL_LIB_ITEM_SOURCE, d->col_kind == (uint32_t)SEL_LIB_ITEM_SOURCE ? khst : "",
                                 (int)stg.x, (int)stg.y, (int)stg.w, (int)stg.h, (int)ssy.x, (int)ssy.y, (int)ssy.w, (int)ssy.h,
                                 d->pls_n > 0u && d->pls_ord != NULL ? pls_key(d, d->pls_ord[0]) : "");
                }
            }
        }
        if (d->pls_n == 0u) {
            sel_gui_rect fb = pls_find_btn_rect(d, pmn, 0u);
            sel_gui_rect ib = pls_find_btn_rect(d, pmn, 1u);
            sel_gui_rect ab = pls_find_btn_rect(d, pmn, 2u);
            o = dump_put(d, o, "plfindbtn 0 %d %d %d %d label=Add from Library\n", (int)fb.x, (int)fb.y, (int)fb.w, (int)fb.h);
            o = dump_put(d, o, "plfindbtn 1 %d %d %d %d label=Import playlist file\n", (int)ib.x, (int)ib.y, (int)ib.w, (int)ib.h);
            o = dump_put(d, o, "plfindbtn 2 %d %d %d %d label=Add Folder\n", (int)ab.x, (int)ab.y, (int)ab.w, (int)ab.h);
        }
        { /* I652c: the import sentinel; printed on every playlist page, the page the import fills is no longer empty */
            sel_engine_stats is;
            sel_engine_stats_get(d->engine, &is);
            o = dump_put(d, o, "plimport sent=%u runs=%u added=%u missing=%u\n", (unsigned)d->imp_sent, (unsigned)is.import_runs,
                         (unsigned)is.import_added, (unsigned)is.import_missing);
            o = dump_put(d, o, "pladdfolder sent=%u runs=%u added=%u\n", (unsigned)d->addf_sent, (unsigned)is.addf_runs, (unsigned)is.addf_added);
        }
        if (d->pls_n == 0u) { /* I652a: the empty playlist's search field, its results and their Add buttons */
            sel_gui_rect pf = pls_find_field_rect(d, pmn);
            o = dump_put(d, o, "plfind field %d %d %d %d results=%d adds=%u searches=%u on=%d vsrc=%d vrows=%u live=%d\n", (int)pf.x, (int)pf.y, (int)pf.w, (int)pf.h, d->pls_find_shown,
                         (unsigned)d->pls_adds, (unsigned)d->lib_searches, d->search_on, sel_engine_state(d->engine)->view != NULL ? (int)sel_engine_state(d->engine)->view->source : -1,
                         sel_engine_state(d->engine)->view != NULL ? (unsigned)sel_engine_state(d->engine)->view->row_n : 0u, pls_find_result(d, 0u) != NULL);
            for (j = 0; (int)j < d->pls_find_shown; j++) {
                sel_gui_rect fa = pls_find_add_rect(d, pmn, j);
                const sel_view_row* fr = pls_find_result(d, j);
                o = dump_put(d, o, "plfindadd %u %d %d %d %d key=%s\n", (unsigned)j, (int)fa.x, (int)fa.y, (int)fa.w, (int)fa.h, fr != NULL ? fr->key : "");
            }
        }
        for (j = 0; d->pls_scroll + j < d->pls_vn && pls_row_visible(d, pmn, j) && o + 400u < sizeof d->library_dump; j++) {
            sel_gui_rect r = pls_row_rect(d, pmn, j);
            uint32_t real = d->pls_ord[d->pls_scroll + j];
            o = dump_put(d, o, "plsong %u %d %d %d %d file=%s title=%s\n", (unsigned)j, (int)r.x, (int)r.y, (int)r.w, (int)r.h, path_base(pls_key(d, real)),
                         d->pls_title[real]);
        }
        o = dump_put(d, o, "plplaying %d\n", (int)d->pls_playing); /* I690: the position of the playing song's row, -1 when none */
        for (j = 0; d->pls_meta != NULL && d->pls_scroll + j < d->pls_vn && pls_row_visible(d, pmn, j) && o + 400u < sizeof d->library_dump; j++) {
            sel_gui_rect r = pls_row_rect(d, pmn, j), la, lb; /* I678h3: the link rects and the db row ids behind them, after the plsong lines */
            uint32_t real = d->pls_ord[d->pls_scroll + j];
            pls_link_rects(d, r, real, &la, &lb);
            o = dump_put(d, o, "plsonglink %u artist %d %d %d %d album %d %d %d %d album_id=%u\n", (unsigned)j, (int)la.x, (int)la.y, (int)la.w, (int)la.h,
                         (int)lb.x, (int)lb.y, (int)lb.w, (int)lb.h, (unsigned)d->pls_meta[real].album_id);
        }
    }
    if (d->si_page) { /* the Song Information page geometry and rows (DUI-P4-10) */
        sel_gui_rect smn = region_rect(d, "main"), sn = pl_name_rect(d, smn);
        o = dump_put(d, o, "si draws %u rows %u cmds %u\n", (unsigned)d->si_draws,
                              (unsigned)d->si_rows, (unsigned)d->si_cmds);
        o = dump_put(d, o, "empty %s\n", d->empty_text);
        for (i = 0; i < SI_BUTTONS; i++) {
            sel_gui_rect b = queue_btn_rect(d, smn, i);
            o = dump_put(d, o, "sibtn %u %d %d %d %d\n", (unsigned)i, (int)b.x,
                                  (int)b.y, (int)b.w, (int)b.h);
        }
        o = dump_put(d, o, "sinote 0 %d %d %d %d\n", (int)sn.x, (int)sn.y, (int)sn.w,
                              (int)sn.h);
        o = dump_put(d, o, "sipanel %d %d\n", (int)si_list_top(d, smn),
                              (int)(smn.y + smn.h));
        d->si_cy = 0;
        memset(&d->si_composer, 0, sizeof d->si_composer);
        si_walk(d, st, si_dump_row);
        o = strlen(d->library_dump);
        o = dump_put(d, o, "sicomposer %d %d %d %d key=%s\n", (int)d->si_composer.x, (int)d->si_composer.y, (int)d->si_composer.w, (int)d->si_composer.h,
                     d->si_composer_key);
    }
    if (d->tr_page) { /* the Tracker page geometry, position and playing row (DUI-P4-12) */
        sel_gui_rect tmn = region_rect(d, "main");
        const sel_pattern_view* tv = st->pattern;
        sel_engine_stats ts;
        uint32_t tn = tr_sample_count(d), tfit = tr_rows_fit(d, tmn), shown = 0u;
        sel_engine_stats_get(d->engine, &ts);
        if (tv != NULL && tv->valid && tv->order == st->song.pattern_pos.order && st->song.pattern_pos.row >= tv->row_lo &&
            st->song.pattern_pos.row < tv->row_lo + tv->row_n) {
            shown = 1u;
        }
        o = dump_put(d, o,
                              "tracker draws %u tab %u order %u row %u rows %u channels %u muted %u samples %u plays %u frames %u cmds %u shown %u bar %u fit %u moves %u\n",
                              (unsigned)d->tr_draws, (unsigned)d->tr_tab, (unsigned)st->song.pattern_pos.order,
                              (unsigned)st->song.pattern_pos.row, (unsigned)d->tr_rows, (unsigned)(tv != NULL ? tv->channels : 0u),
                              (unsigned)st->mute_mask, (unsigned)tn, (unsigned)ts.sample_plays, (unsigned)ts.sample_frames,
                              (unsigned)d->tr_cmds, (unsigned)shown, (unsigned)d->tr_bar, (unsigned)tfit, (unsigned)d->tr_moves);
        for (i = 0; i < TR_TABS; i++) {
            sel_gui_rect b = tr_tab_rect(d, tmn, i);
            o = dump_put(d, o, "trtab %u %d %d %d %d\n", (unsigned)i, (int)b.x,
                                  (int)b.y, (int)b.w, (int)b.h);
        }
        for (i = 0; tv != NULL && i < tv->channels && i < tr_cols_fit(d, tmn); i++) {
            sel_gui_rect b = tr_chan_rect(d, tmn, i);
            o = dump_put(d, o, "trchan %u %d %d %d %d\n", (unsigned)i, (int)b.x,
                                  (int)b.y, (int)b.w, (int)b.h);
        }
        for (i = 0; i < tn && i < tfit; i++) {
            sel_gui_rect b = tr_row_rect(d, tmn, i);
            o = dump_put(d, o, "trsample %u %d %d %d %d\n", (unsigned)(i + 1u),
                                  (int)b.x, (int)b.y, (int)b.w, (int)b.h);
        }
        for (i = 0; shown && i < tv->channels; i++) {
            char tnote[4], tins[4], tvol[4], tfx[8];
            sel_uim_cell_fields(&tv->cell[st->song.pattern_pos.row - tv->row_lo][i], tnote, tins, tvol, tfx);
            o = dump_put(d, o, "trcell %u %s %s %s %s\n", (unsigned)i, tnote, tins,
                                  tvol, tfx);
        }
        o = strlen(d->library_dump);
    }
    if (d->queue_page) { /* the Queue page geometry and queue state (DUI-P4-8) */
        sel_engine_stats qs;
        sel_gui_rect qmn = region_rect(d, "main");
        sel_engine_stats_get(d->engine, &qs);
        queue_groups_sync(d);
        o = dump_put(d, o,
                              "queue draws %u rows %u cmds %u hash %08x count %u undo %u redo %u scope %s\n", (unsigned)d->queue_draws,
                              (unsigned)d->queue_rows, (unsigned)d->queue_cmds, (unsigned)queue_hash(st), (unsigned)st->queue_count,
                              (unsigned)qs.undo_depth, (unsigned)qs.redo_depth, sel_uim_scope_name(queue_scope_index(st)));
        o = dump_put(d, o, "qdrag from %d gap %d paints %u\n", (int)d->qdrag_from, (int)d->qdrag_gap, (unsigned)d->qdrag_paints);
        for (i = 0; i < QUEUE_BUTTONS; i++) {
            sel_gui_rect b = queue_btn_rect(d, qmn, i);
            o = dump_put(d, o, "qbtn %u %d %d %d %d\n", (unsigned)i, (int)b.x,
                                  (int)b.y, (int)b.w, (int)b.h);
        }
        o = dump_put(d, o, "empty %s\n", d->empty_text);
        o = dump_put(d, o, "qgroups %u\n", (unsigned)(d->qg.valid ? d->qg.headers : 0u));
        for (i = 0; d->sm_open && d->sm_step == 2 && i < 3u; i++) { /* the open Scope menu's items */
            sel_gui_rect m = song_menu_item_rect(d, (int32_t)i);
            o = dump_put(d, o, "qscope %u %d %d %d %d\n", (unsigned)i, (int)m.x, (int)m.y, (int)m.w, (int)m.h);
        }
        for (i = 0; i < st->queue_count && o + 400u < sizeof d->library_dump; i++) {
            if (!queue_row_visible(d, qmn, i)) {
                continue;
            }
            sel_gui_rect r = queue_row_rect(d, qmn, i), nx = queue_next_rect(d, qmn, i), dl = queue_del_rect(d, qmn, i);
            o = dump_put(d, o,
                                  "qrow %u %d %d %d %d %s\nqnext %u %d %d %d %d\nqdel %u %d %d %d %d\nqpath %u %s\n", (unsigned)i,
                                  (int)r.x, (int)r.y, (int)r.w, (int)r.h,
                                  (st->queue[i].flags & SEL_QUEUE_FLAG_DIVIDER) != 0u ? "divider" : "song", (unsigned)i, (int)nx.x,
                                  (int)nx.y, (int)nx.w, (int)nx.h, (unsigned)i, (int)dl.x, (int)dl.y, (int)dl.w, (int)dl.h,
                                  (unsigned)i, st->queue_text + st->queue[i].path_offset);
        }
    }
    if (o + 400u < sizeof d->library_dump) { /* the page navigation (I262) */
        o = dump_put(d, o, "nav page=%s nav_clicks=%u nav_keys=%u doors=%u hist=%u pos=%u np_page_draws=%u arrow_clicks=%u lit=%u lib_off_page=%u\nfocus_ring %d %d %d %d\nsearch_field %d %d %d %d caret_draws=%u caret_rect %d %d %d %d\nnav_arrows %d %d %d %d %d %d %d %d\nnav_icons %u\n",
                              g_page_name[d->page], (unsigned)d->nav_clicks, (unsigned)d->nav_keys, (unsigned)DOOR_COUNT,
                              (unsigned)d->rh.n, (unsigned)d->rh.pos, (unsigned)d->np_page_draws, (unsigned)d->nav_arrow_clicks, (unsigned)d->lit_doors, (unsigned)d->lib_off_page, (int)d->ring.x, (int)d->ring.y, (int)d->ring.w, (int)d->ring.h,
                              (int)d->field.x, (int)d->field.y, (int)d->field.w, (int)d->field.h, (unsigned)d->caret_draws, (int)d->caret.x, (int)d->caret.y, (int)d->caret.w, (int)d->caret.h, (int)d->nav_back.x, (int)d->nav_back.y, (int)d->nav_back.w,
                              (int)d->nav_back.h, (int)d->nav_fwd.x, (int)d->nav_fwd.y, (int)d->nav_fwd.w, (int)d->nav_fwd.h, (unsigned)d->nav_icon_draws);
    }
    if (o + 300u < sizeof d->library_dump) { /* the route table (I678h): clicks routed, the page the window is on, the history */
        char cur[48];
        if (d->page == PG_COLLECTION) {
            snprintf(cur, sizeof cur, "collection:%s", sel_route_kind_name(d->col_kind != 0u ? col_eng_kind(d) : (uint32_t)SEL_LIB_ITEM_PLAYLIST));
        } else {
            snprintf(cur, sizeof cur, "%s", g_page_name[d->page]);
        }
        o = dump_put(d, o, "route route_calls=%u cur=%s n=%u pos=%u scroll=%u artist_rect %d %d %d %d\n", (unsigned)d->rh.calls, cur, (unsigned)d->rh.n,
                     (unsigned)d->rh.pos, (unsigned)d->pls_scroll, (int)d->col_artist.x, (int)d->col_artist.y, (int)d->col_artist.w, (int)d->col_artist.h);
    }
    if (o + 1800u < sizeof d->library_dump) { /* the sidebar library list (I608): counters and the rectangle of each drawn row */
        uint32_t k;
        {   /* I611: the tools row and the sort/view menu */
            sel_gui_rect mg = side_mag_rect(d), sr = side_sort_rect(d), fr = side_field_rect(d);
            o = dump_put(d, o, "sidetools sort=%u view=%u filter=[%s] open=%u edit=%u cols=%u sorts=%u sort_picks=%u view_picks=%u menu_opens=%u filter_keys=%u\n",
                         (unsigned)d->side_sort, (unsigned)d->side_view, d->lf_text, (unsigned)d->lf_open, (unsigned)d->lf_edit, (unsigned)side_lib_cols(d),
                         (unsigned)d->side_sorts, (unsigned)d->side_sort_picks, (unsigned)d->side_view_picks, (unsigned)d->side_menu_opens,
                         (unsigned)d->side_filter_keys);
            {
                sel_gui_rect lt = d->page == PG_FOLDERS ? lib_tools_rect(d, panel_rect(d, "main")) : rect_of(0, 0, 0, 0);
                sel_gui_rect lm = tl_mag_rect(d, lt), ls = tl_sort_rect(d, lt);
                o = dump_put(d, o, "libtools draws=%u clicks=%u sort_keys=%u pref_sets=%u mag %d %d %d %d sort %d %d %d %d\n", (unsigned)d->lib_tools_draws,
                             (unsigned)d->lib_tools_clicks, (unsigned)d->lib_sort_keys, (unsigned)d->side_pref_sets, (int)lm.x, (int)lm.y, (int)lm.w, (int)lm.h,
                             (int)ls.x, (int)ls.y, (int)ls.w, (int)ls.h);
            }
            {
                const sel_state* ps = sel_engine_state(d->engine);
                const char* r0 = ps->composers != NULL ? lib_rows_first(ps) : NULL;
                o = dump_put(d, o, "libpage rows=%u first=[%s] view_sends=%u\n", (unsigned)(ps->composers != NULL ? lib_list_n(ps) : 0u),
                             r0 != NULL && *r0 != '\0' ? lib_next(r0) : "", (unsigned)d->lib_view_sends);
            }
            o = dump_put(d, o, "sidetools_rect mag %d %d %d %d sort %d %d %d %d field %d %d %d %d\n", (int)mg.x, (int)mg.y, (int)mg.w, (int)mg.h,
                         (int)sr.x, (int)sr.y, (int)sr.w, (int)sr.h, (int)fr.x, (int)fr.y, (int)fr.w, (int)fr.h);
            if (d->sm_open && d->sm_step == 4) {
                uint32_t g;
                for (g = 0; g < 8u; g++) {
                    sel_gui_rect mr = song_menu_item_rect(d, (int32_t)g);
                    o = dump_put(d, o, "lmenu %u %d %d %d %d\n", (unsigned)g, (int)mr.x, (int)mr.y, (int)mr.w, (int)mr.h);
                }
                for (g = 0; g < SEL_UIM_LVIEWS; g++) {
                    sel_gui_rect mr = lmenu_strip_rect(d, g);
                    o = dump_put(d, o, "lmenu_view %u %d %d %d %d\n", (unsigned)g, (int)mr.x, (int)mr.y, (int)mr.w, (int)mr.h);
                }
            }
        }
        if (d->sm_open) { /* I649: the open menu's panel, in window coordinates */
            sel_gui_rect m0 = song_menu_item_rect(d, 0);
            o = dump_put(d, o, "smenu %d %d %d %d\n", (int)m0.x, (int)m0.y, (int)m0.w, (int)(m0.h * song_menu_n(d)));
            o = dump_put(d, o, "smenu_pl step=%d playlists=%u sidebar_rows=%u\n", d->sm_step, (unsigned)d->sm_pl_n, (unsigned)d->side_n);
        }
        o = dump_put(d, o, "sidelib draws=%u rows=%u items=%u opens=%u y=%d max=%d item_calls=%u lib_touch_calls=%u\n", (unsigned)d->side_draws, (unsigned)d->side_rows,
                     (unsigned)d->side_n, (unsigned)d->side_opens, (int)d->side_y_q16, (int)side_lib_max_q16(d), (unsigned)d->side_item_calls,
                     (unsigned)lib_touch_calls(d));
        {
            sel_gui_rect th = topbar_part(d, TOPBAR_HOME), tf = topbar_part(d, TOPBAR_FIELD), tb = topbar_part(d, TOPBAR_BROWSE);
            o = dump_put(d, o, "sidebar_doors drawn=%u\n", (unsigned)d->doors_drawn);
            o = dump_put(d, o, "topbar draws=%u shown=%d home %d %d %d %d field %d %d %d %d browse %d %d %d %d home_clicks=%u browse_clicks=%u\n",
                         (unsigned)d->topbar_draws, topbar_shown(d), (int)th.x, (int)th.y, (int)th.w, (int)th.h, (int)tf.x, (int)tf.y,
                         (int)tf.w, (int)tf.h, (int)tb.x, (int)tb.y, (int)tb.w, (int)tb.h, (unsigned)d->topbar_home_clicks,
                         (unsigned)d->topbar_browse_clicks);
        }
        {
            sel_gui_rect ta = d->np_add, tt = np_toggle_rect(d), md = region_rect(d, "main");
            o = dump_put(d, o, "np_panel context=[%s] add %d %d %d %d add_clicks=%u toggle %d %d %d %d toggle_clicks=%u hidden=%u detail_w=%d main_w=%d\n",
                         d->np_ctx_shown, (int)ta.x, (int)ta.y, (int)ta.w, (int)ta.h, (unsigned)d->np_add_clicks, (int)tt.x, (int)tt.y,
                         (int)tt.w, (int)tt.h, (unsigned)d->np_toggle_clicks, (unsigned)d->detail_hidden, (int)region_rect(d, "detail").w, (int)md.w);
        }
        o = dump_put(d, o, "showall draws=%u clicks=%u page_draws=%u opens=%u open=%u tiles=%u at %d %d %d %d\n", (unsigned)d->show_all_draws,
                     (unsigned)d->show_all_clicks, (unsigned)d->shelf_page_draws, (unsigned)d->shelf_page_opens, (unsigned)d->shelf_open,
                     (unsigned)d->shelf_page_tiles, (int)d->show_all_rect[0].x, (int)d->show_all_rect[0].y, (int)d->show_all_rect[0].w, (int)d->show_all_rect[0].h);
        o = dump_put(d, o, "sidelib_density view=%u row_h=%d cols=%u table=%u table_head=%d sidebar_w=%d canvas_w=%d card_h=%d pinned=%u sub=%u sub_clicks=%u sub_btn %d %d %d %d\n", (unsigned)d->side_view,
                     (int)side_lib_row_h(d), (unsigned)side_lib_cols(d), (unsigned)side_table(d), (int)side_table_head_h(d), (int)region_rect(d, "sidebar").w,
                     (int)d->canvas->w, (int)side_card_h(d), (unsigned)side_pinned_n(d), (unsigned)d->side_sub, (unsigned)d->side_sub_clicks, (int)d->side_sub_btn.x,
                     (int)d->side_sub_btn.y, (int)d->side_sub_btn.w, (int)d->side_sub_btn.h);
        o = dump_put(d, o, "sideempty draws=%u clicks=%u btn %d %d %d %d\n", (unsigned)d->side_empty_draws, (unsigned)d->side_create_clicks,
                     (int)d->side_empty_btn.x, (int)d->side_empty_btn.y, (int)d->side_empty_btn.w, (int)d->side_empty_btn.h);
        for (k = 1u; k <= (uint32_t)SIDE_HDR_COUNT; k++) { /* I609: header controls and their counters */
            sel_gui_rect r = side_hdr_rect(d, k);
            o = dump_put(d, o, "sidehdr %s plus_clicks=%u wide_clicks=%u wide=%u sidebar_w=%d rect %d %d %d %d tween_frames=%u\n", k == SIDE_HDR_PLUS ? "plus" : "wide",
                         (unsigned)d->side_plus_clicks, (unsigned)d->side_wide_clicks, (unsigned)d->side_wide, (int)region_rect(d, "sidebar").w,
                         (int)r.x, (int)r.y, (int)r.w, (int)r.h, (unsigned)d->side_tween_frames);
        }
        {
            sel_gui_rect tr = side_hdr_title_rect(d);
            o = dump_put(d, o, "sidehdr title %d %d %d %d\n", (int)tr.x, (int)tr.y, (int)tr.w, (int)tr.h); /* I673 */
        }
        o = dump_put(d, o, "sidechips chip=%u filters=%u scrolls=%u single=%d\n", (unsigned)d->side_chip, (unsigned)d->side_filters,
                     (unsigned)d->side_chip_scrolls, side_chip_single(d));
        {
            sel_gui_rect xr = side_chip_chevron(d, 0);
            o = dump_put(d, o, "sidechip_x %d %d %d %d %d\n", side_chip_has_x(d), (int)xr.x, (int)xr.y, (int)xr.w, (int)xr.h);
        }
        for (k = 0; k < d->side_n && k < 64u; k++) {
            if (side_lib_open_item(d, k)) {
                o = dump_put(d, o, "sidelib_open kind=%u row=%u\n", (unsigned)d->side_items[k].kind, (unsigned)k);
            }
        }
        for (k = 0; k < (uint32_t)SIDE_CHIPS && side_chips_h(d) > 0; k++) {
            sel_gui_rect r = side_chip_rect(d, k);
            o = dump_put(d, o, "sidechip %u %d %d %d %d\n", (unsigned)k, (int)r.x, (int)r.y, (int)r.w, (int)r.h);
        }
        {
            sel_gui_rect lc = side_chip_chevron(d, 0), rc = side_chip_chevron(d, 1);
            o = dump_put(d, o, "sidechev L %d %d %d %d R %d %d %d %d\n", (int)lc.x, (int)lc.y, (int)lc.w, (int)lc.h, (int)rc.x, (int)rc.y, (int)rc.w, (int)rc.h);
        }
        for (k = 0; k < d->side_n && k < 16u; k++) {
            sel_gui_rect r = side_lib_row_rect(d, k);
            o = dump_put(d, o, "sidelib_row %u %d %d %d %d pinned=%d open=%d kind=%u %s\n", (unsigned)k, (int)r.x, (int)r.y, (int)r.w, (int)r.h,
                         d->side_items[k].pinned, side_lib_open_item(d, k), (unsigned)d->side_items[k].kind, d->side_items[k].name);
        }
    }
    if (o + 300u < sizeof d->library_dump) { /* the Now Playing page text (I361) */
        o = dump_put(d, o, "np_page_text title=[%s] artist=[%s] font_misses=%u\n", d->np_text_title, d->np_text_artist, (unsigned)d->font_misses);
    }
    if (o + 300u < sizeof d->library_dump) { /* the page title box and what sits below it, every page (I545) */
        sel_gui_rect pm = panel_rect(d, "main");
        o = dump_put(d, o, "page_tops title_h=%d queue_btn_y=%d queue_list_y=%d playlists_name_y=%d songinfo_list_y=%d tracker_tab_y=%d\n",
                     (int)page_title_h(d), (int)(queue_btn_rect(d, pm, 0u).y - pm.y), (int)(queue_list_rect(d, pm).y - pm.y),
                     (int)(pl_name_rect(d, pm).y - pm.y), (int)(si_list_top(d, pm) - pm.y), (int)(tr_tab_rect(d, pm, 0u).y - pm.y));
    }
    if (o + 200u < sizeof d->library_dump) { /* the footer bars (I312) */
        sel_gui_rect vb = vol_bar_rect(d), skb = seek_bar_rect(d);
        o = dump_put(d, o, "footer vol_bar %d %d %d %d seek_bar %d %d %d %d vol_clicks=%u seek_clicks=%u seek_fill=%d\n", (int)vb.x,
                     (int)vb.y, (int)vb.w, (int)vb.h, (int)skb.x, (int)skb.y, (int)skb.w, (int)skb.h,
                     (unsigned)d->vol_clicks, (unsigned)d->seek_clicks, (int)d->seek_fill);
    }
    if (o + 200u < sizeof d->library_dump) { /* the hover state (I272) */
        o = dump_put(d, o,
                              "hover_box %d %d %d %d fill=%08x token=%08x paints=%u tile=%d\n", (int)d->hover_box.x, (int)d->hover_box.y,
                              (int)d->hover_box.w, (int)d->hover_box.h, (unsigned)d->hover_fill,
                              (unsigned)ARGB(d->theme.token[SEL_GUI_TOKEN_SURFACE_HOVER]), (unsigned)d->hover_paints, (int)d->hover_tile);
        o = dump_put(d, o, "hover_ink=%d\n", (int)d->ink_in);
        o = dump_put(d, o, "turns idle=%u present=%u scan=%u input=%u viz=%u bits song=%u queue=%u transport=%u progress=%u volume=%u status=%u\n",
                     (unsigned)d->turn_why[0], (unsigned)d->turn_why[1], (unsigned)d->turn_why[2], (unsigned)d->turn_why[3],
                     (unsigned)d->turn_why[4], (unsigned)d->turn_bits[0], (unsigned)d->turn_bits[1], (unsigned)d->turn_bits[2],
                     (unsigned)d->turn_bits[3], (unsigned)d->turn_bits[4], (unsigned)d->turn_bits[5]);
#if defined(SELECTA_DESKTOP_SDL) && !defined(SELECTA_DESKTOP_RTG) && !defined(SELECTA_DESKTOP_AGA)
        if (d->sdl != NULL) { /* I291: input to present, and the cost of a wheel notch, in microseconds */
            desk_sdl_lat lt;
            desk_sdl_latency(d->sdl, &lt);
            o = dump_put(d, o, "input_latency n=%u median_us=%u p95_us=%u max_us=%u wheel_n=%u wheel_median_us=%u wheel_p95_us=%u wheel_max_us=%u\n",
                         (unsigned)lt.n, (unsigned)lt.med_us, (unsigned)lt.p95_us, (unsigned)lt.max_us, (unsigned)lt.cost_n,
                         (unsigned)lt.cost_med_us, (unsigned)lt.cost_p95_us, (unsigned)lt.cost_max_us);
        }
#endif
        o = dump_put(d, o, "library_serial=%u library_reloads=%u\n", (unsigned)sel_engine_state(d->engine)->library_serial, (unsigned)d->lib_reloads);
        {
            static const char* const rn[] = {"", "sidebar", "main", "detail", "player"};
            int hr = hover_region_of(d, d->hover_box);
            sel_gui_rect rr = hr != HR_NONE ? region_rect(d, rn[hr]) : rect_of(0, 0, 0, 0);
            o = dump_put(d, o, "hover_dirty=%u region=%s %d %d %d %d\n", (unsigned)d->hover_dirty_max, rn[hr], (int)rr.x, (int)rr.y,
                         (int)rr.w, (int)rr.h);
        }
        if (o + 300u < sizeof d->library_dump) { /* one control per layout region, where dui-hover-regions moves the pointer (DUI-P3-8) */
            sel_gui_rect dr = sidebar_door_rect(d, 3u), pb = desk_button_rect(d, SLOT_PLAY), tl;
            tl = home_shelf_mode(sel_engine_state(d->engine)) ? shelf_tile_rect(d, 0u, 0u) : tile_rect(d, 0u);
            o = dump_put(d, o, "hover_ctl sidebar %d %d %d %d main %d %d %d %d player %d %d %d %d detail %d %d %d %d nav %d %d %d %d\n",
                         (int)dr.x, (int)dr.y, (int)dr.w, (int)dr.h, (int)tl.x, (int)tl.y, (int)tl.w, (int)tl.h, (int)pb.x, (int)pb.y,
                         (int)pb.w, (int)pb.h, (int)d->np_like.x, (int)d->np_like.y, (int)d->np_like.w, (int)d->np_like.h,
                         (int)d->nav_back.x, (int)d->nav_back.y, (int)d->nav_back.w, (int)d->nav_back.h);
        }
        if (o + 300u < sizeof d->library_dump) { /* the region rectangles the hover repaint may touch */
            sel_gui_rect rs = region_rect(d, "sidebar"), rm = region_rect(d, "main"), rd = region_rect(d, "detail"),
                         rp = region_rect(d, "player");
            o = dump_put(d, o, "hover_regions sidebar %d %d %d %d main %d %d %d %d detail %d %d %d %d player %d %d %d %d\n", (int)rs.x,
                         (int)rs.y, (int)rs.w, (int)rs.h, (int)rm.x, (int)rm.y, (int)rm.w, (int)rm.h, (int)rd.x, (int)rd.y,
                         (int)rd.w, (int)rd.h, (int)rp.x, (int)rp.y, (int)rp.w, (int)rp.h);
        }
        for (i = 0; i < (uint32_t)CTL_COUNT && o + 200u < sizeof d->library_dump; i++) { /* I244: the action-bound controls drawn now */
            sel_gui_rect cr = desk_ctl_rect(d, i);
            if (cr.w > 0) {
                o = dump_put(d, o, "tipctl id=%s %d %d %d %d\n", desk_ctl_action(d, i), (int)cr.x, (int)cr.y, (int)cr.w, (int)cr.h);
            }
        }
        for (i = 0; i < DOOR_COUNT && o + 200u < sizeof d->library_dump; i++) { /* the door rectangles, where a test moves the pointer */
            sel_gui_rect dr = sidebar_door_rect(d, i);
            o = dump_put(d, o, "door %u %d %d %d %d\n", (unsigned)i, (int)dr.x, (int)dr.y, (int)dr.w, (int)dr.h);
        }
    }
    for (i = 0; i < d->label_n && o + 200u < sizeof d->library_dump; i++) { /* centred button and chip labels (I270) */
        o = dump_put(d, o, "label %d %d %d %d ink %d %d text %s\n",
                              (int)d->labels[i].box.x, (int)d->labels[i].box.y, (int)d->labels[i].box.w, (int)d->labels[i].box.h,
                              (int)d->labels[i].ink_x, (int)d->labels[i].ink_w, d->labels[i].text);
    }
    if (o + 900u < sizeof d->library_dump) { /* the overlays (DUI-P4-13) */
        uint32_t k;
        o = dump_put(d, o,
                              "overlay kind=%d palette_opens=%u palette_runs=%u help_opens=%u cmdline_opens=%u cmdline_runs=%u sends_ok=%u "
                              "toasts=%u toast_on=%d menu_picks=%u,%u,%u,%u last_verb=%s pal_count=%u queue=%u status=%s\n"
                              "song_menu open=%d items=%d opens=%u picks=%u playlist_adds=%u rows=%u accent=%06x raised=%06x\n"
                              "dialog quits=%d open=%d opens=%u confirms=%u cancels=%u songs=%u playlists=%u cancel=%d,%d,%d,%d confirm=%d,%d,%d,%d clear=%d,%d,%d,%d\n",
                              d->overlay, (unsigned)d->palette_opens, (unsigned)d->palette_runs, (unsigned)d->help_opens,
                              (unsigned)d->cmdline_opens, (unsigned)d->cmdline_runs, (unsigned)d->overlay_sends_ok, (unsigned)d->toasts,
                              d->toast_on, (unsigned)d->menu_picks[0], (unsigned)d->menu_picks[1], (unsigned)d->menu_picks[2],
                              (unsigned)d->menu_picks[3], d->last_verb[0] != '\0' ? d->last_verb : "-", (unsigned)d->pal.count,
                              (unsigned)st->queue_count, d->cmd.status[0] != '\0' ? d->cmd.status : "-", d->sm_open, d->sm_open ? song_menu_count(d) : 0,
                              (unsigned)d->sm_opens, (unsigned)d->sm_picks, (unsigned)d->sm_pl_adds, (unsigned)d->sm_n,
                              (unsigned)(ARGB(d->theme.token[SEL_GUI_TOKEN_ACCENT]) & 0xFFFFFFu),
                              (unsigned)(ARGB(d->theme.token[SEL_GUI_TOKEN_SURFACE_RAISED]) & 0xFFFFFFu),
                              d->quit_req, d->dlg_open, (unsigned)d->dlg_opens, (unsigned)d->dlg_confirms, (unsigned)d->dlg_cancels,
                              (unsigned)st->library_songs, (unsigned)d->dlg_playlists, dlg_button_rect(d, 0u).x, dlg_button_rect(d, 0u).y,
                              dlg_button_rect(d, 0u).w, dlg_button_rect(d, 0u).h, dlg_button_rect(d, 1u).x,
                              dlg_button_rect(d, 1u).y, dlg_button_rect(d, 1u).w, dlg_button_rect(d, 1u).h,
                              lib_clear_rect(d, panel_rect(d, "main")).x, lib_clear_rect(d, panel_rect(d, "main")).y,
                              lib_clear_rect(d, panel_rect(d, "main")).w, lib_clear_rect(d, panel_rect(d, "main")).h);
#ifdef SELECTA_DESKTOP_SDL
        o = dump_put(d, o, "window raise_count=%u\n", d->sdl != NULL ? (unsigned)desk_sdl_raises(d->sdl) : 0u); /* I501 */
#endif
        for (k = 0; k < 5u && k < d->pal.count; k++) {
            o = dump_put(d, o, "palitem %u %s\n", (unsigned)k, d->pal.items[k].name);
        }
    }
    if (o + 300u < sizeof d->library_dump) { /* I632d: the selection of the list on screen */
        char ranges[96];
        sel_uim_selset_format(&d->sel, ranges, sizeof ranges);
        o = dump_put(d, o, "select count=%u runs=%u list=%u,%u,%u ranges=%s clicks=%u keys=%u\n", (unsigned)sel_uim_selset_count(&d->sel),
                     (unsigned)sel_uim_selset_run_count(&d->sel), (unsigned)d->sel.list.kind, (unsigned)d->sel.list.id,
                     (unsigned)d->sel.list.version, ranges[0] != '\0' ? ranges : "-", (unsigned)d->sel_clicks, (unsigned)d->sel_keys);
    }
    for (p = st->stats; p != NULL && *p != '\0' && o + 400u < sizeof d->library_dump;) {
        const char* val = lib_next(p);
        o = dump_put(d, o, "stat %s|%s\n", p, val);
        p = lib_next(val);
    }
    return d->library_dump;
}

/* ---- Library grid and cover flow (DUI-P4-5): the engine's flat rows (VIEW_STYLE grid / coverflow) as artwork tiles.
 * Integer geometry only. Sizes are logical units on the 8-unit grid (the artwork size tokens of plan 2.2 are not in the
 * token table yet: tile and cover flow art are these constants). The art is the generated tile of the Home shelves. */
enum { GRID_ART = 160, GRID_GAP = 16, FLOW_ART = 240 };

/* Cover flow stack constants (I391): FLOW_SIDES neighbours per side; neighbour k sits k/FLOW_STEP_DIV of the centre size from the
 * centre line and is k/FLOW_SHRINK_DIV smaller. Each neighbour then shows (1/FLOW_STEP_DIV - 1/(2*FLOW_SHRINK_DIV)) of the centre
 * size of its own art beside the cover in front of it (0.15: 51 px at a 340 px centre). The stack width follows from the same
 * two functions, so the width fit is exact. */
enum { FLOW_SIDES = 3, FLOW_STEP_DIV = 4, FLOW_SHRINK_DIV = 5 };
static int32_t flow_off(int32_t big, int32_t k) { return k * big / FLOW_STEP_DIV; }
static int32_t flow_size(int32_t big, int32_t k) { return big - k * big / FLOW_SHRINK_DIV; }
/* Width of the whole stack at centre size big: the two outermost covers' outer edges. */
static int32_t flow_stack_w(int32_t big) { return 2 * (flow_off(big, FLOW_SIDES) + flow_size(big, FLOW_SIDES) / 2); }

/* Smallest rectangle holding a and b; a zero-width a is "nothing yet" (the tile boxes of the grid and the cover flow, I379, I382). */
static sel_gui_rect rect_union(sel_gui_rect a, sel_gui_rect b) {
    sel_gui_rect u;
    int32_t x1, y1;
    if (a.w == 0) {
        return b;
    }
    x1 = a.x + a.w > b.x + b.w ? a.x + a.w : b.x + b.w;
    y1 = a.y + a.h > b.y + b.h ? a.y + a.h : b.y + b.h;
    u = rect_of(a.x < b.x ? a.x : b.x, a.y < b.y ? a.y : b.y, 0, 0);
    u.w = x1 - u.x;
    u.h = y1 - u.y;
    return u;
}

/* The chooser segment that shows the engine's current style: Grid, Cover Flow, or List for every row style. */
static uint32_t lib_style_selected(const sel_state* st) {
    if (st->view != NULL && st->view->style == (uint32_t)SEL_VIEW_STYLE_GRID) {
        return 0u;
    }
    return st->view != NULL && st->view->style == (uint32_t)SEL_VIEW_STYLE_COVERFLOW ? 1u : 2u;
}

static void draw_lib_styles(desk* d, sel_gui_rect mn, const sel_state* st) {
    uint32_t i, sel = lib_style_selected(st);
    uint32_t text = ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT]), muted = ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT_MUTED]);
    for (i = 0; i < (uint32_t)LIB_STYLES; i++) {
        sel_gui_rect r = lib_style_rect(d, mn, i);
        uint32_t ink;
        if (i == sel) { /* the selected look is the filled surface of the selected sidebar door */
            hover_reg(d, r);
            sel_gui_round_rect(d->canvas, r, lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_RADIUS]), d->theme.token[SEL_GUI_TOKEN_SURFACE]);
            ink = text;
        } else {
            ink = hover_ink_of(d, r, muted, text);
        }
        sel_gui_icon_draw(d->canvas, &d->icons, (uint32_t)lib_style_icon[i], rect_of(r.x + lpx(d, LIB_STYLE_PAD), r.y, lpx(d, LIB_STYLE_ICON), r.h),
                          lpx(d, LIB_STYLE_ICON), ink);
        if (lib_style_text_w(d, mn, i) > 0) {
            draw_text_centered(d, rect_of(r.x + r.w - lib_style_text_w(d, mn, i), r.y, lib_style_text_w(d, mn, i), r.h), lib_style_name[i], 1,
                               LIB_STYLE_LABEL_PX, ink);
        }
        sel_gui_dirty_add(d->canvas, r);
    }
}

static int lib_grid_on(const sel_state* st) {
    return st->composers != NULL && st->view != NULL &&
           (st->view->style == (uint32_t)SEL_VIEW_STYLE_GRID || st->view->style == (uint32_t)SEL_VIEW_STYLE_COVERFLOW);
}

/* Tiles per row for the Library main region: the tile size plus gap against the width. */
static uint32_t lib_grid_cols(const desk* d, sel_gui_rect mn) {
    int32_t pad = lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_TILE_PAD]);
    int32_t tw = lpx(d, GRID_ART) + 2 * pad, gap = lpx(d, GRID_GAP);
    int32_t avail = mn.w - 2 * lpx(d, LIB_MARGIN) + gap;
    return avail >= tw + gap ? (uint32_t)(avail / (tw + gap)) : 1u;
}

/* Registers view row k of the Library page with the song menu when it is a song (I371, DUI-P4-16): a tree leaf or a column
 * song (queue kind, path from the engine), a Files row that is a file, a History row naming a song (home kind, album 0).
 * Folders, nodes, headings and albums are not songs. Only rows whose top is in the list are registered, so the registry
 * and the hover list hold the visible rows: the view window is at most 64 rows (SEARCH_WINDOW_ROWS), under SM_ROWS. */
static void lib_view_row_reg(desk* d, sel_gui_rect mn, const sel_state* st, uint32_t k, sel_gui_rect r) {
    const sel_view_row* vr = &st->view->rows[k];
    char path[sizeof((song_ref*)0)->path];
    if (r.y < lib_list_top(d, mn, st) || r.y >= mn.y + mn.h) {
        return;
    }
    if (vr->kind == (uint32_t)SEL_ROW_HOME) {
        if (vr->album == 0u && vr->key != NULL) {
            song_row_set_list(d, song_row_reg(d, r, vr->key, 0, 0, 0), SEL_UIM_LIST_VIEW, (uint32_t)d->page, st->view->version, st->view->window_start + k);
        }
    } else if (sel_engine_view_row_path(d->engine, k, path, sizeof path)) {
        song_row_set_list(d, song_row_reg(d, r, path, 0, 0, 0), SEL_UIM_LIST_VIEW, (uint32_t)d->page, st->view->version, st->view->window_start + k);
    }
}

/* The cover of a grid or cover flow tile (I691d): the Library's rows get the song path from the engine (window slot `k`), a
 * filtered copy (Search, k = UINT32_MAX) and Home rows carry their own key. */
static void grid_row_cover(desk* d, sel_gui_rect ar, const sel_view_row* r, uint32_t k, uint32_t* slot) {
    char path[sizeof((song_ref*)0)->path];
    const char* p = NULL;
    if (r->kind != (uint32_t)SEL_ROW_HOME && k != UINT32_MAX && sel_engine_view_row_path(d->engine, k, path, sizeof path)) {
        p = path;
    }
    row_cover(d, ar, r, p, slot);
}

/* The caption under a tile: left-aligned for the grid, centred under the cover for the cover flow (I391). */
static void lib_tile_text(desk* d, sel_gui_rect ar, int32_t y, const sel_view_row* r, uint32_t text, uint32_t muted, int centre) {
    const char* title = r->text != NULL ? r->text : "";
    if (centre) {
        draw_text_centered(d, rect_of(ar.x, y, ar.w, lpx(d, HOME_TITLE_H)), title, 1, 14, text);
        if (r->artist != NULL) {
            draw_text_centered(d, rect_of(ar.x, y + lpx(d, HOME_TITLE_H), ar.w, lpx(d, HOME_META_H)), r->artist, 0, 13, muted);
        }
        return;
    }
    draw_text(d, rect_of(ar.x, y, ar.w, lpx(d, HOME_TITLE_H)), title, 1, 14, text);
    if (r->artist != NULL) {
        draw_text(d, rect_of(ar.x, y + lpx(d, HOME_TITLE_H), ar.w, lpx(d, HOME_META_H)), r->artist, 0, 13, muted);
    }
}

/* The tile grid over the rows of v from the absolute y y0 down. The Library draws the engine's view and registers its rows with
 * lib_view_row_reg; the Search page (I615b) draws a filtered copy of its rows and registers the songs with the song menu. */
static void draw_grid_rows(desk* d, sel_gui_rect mn, const sel_view_state* v, int32_t y0, int lib_reg) {
    int32_t pad = lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_TILE_PAD]), gap = lpx(d, GRID_GAP), art = lpx(d, GRID_ART);
    int32_t tw = art + 2 * pad, th = pad + art + lpx(d, HOME_TILE_TEXT_GAP) + lpx(d, HOME_TITLE_H) + lpx(d, HOME_META_H) + pad;
    int32_t vis = (mn.y + mn.h - y0 + gap) / (th + gap);
    uint32_t cols = lib_grid_cols(d, mn), i, first, panel = ARGB(d->theme.token[SEL_GUI_TOKEN_PANEL]);
    uint32_t cur_row = v->cursor / cols;
    first = vis > 0 && cur_row >= (uint32_t)vis ? cur_row - (uint32_t)vis + 1u : 0u;
    d->grid_draws++;
    d->grid_cols = cols;
    d->grid_tiles = 0u;
    d->grid_box = rect_of(0, 0, 0, 0);
    for (i = 0; i < v->row_n; i++) {
        uint32_t g = v->window_start + i, row = g / cols, col = g % cols;
        const sel_view_row* r = &v->rows[i];
        sel_gui_rect t, ar;
        int sel = g == v->cursor;
        if (row < first || (int32_t)(row - first) >= vis) {
            continue;
        }
        t = rect_of(mn.x + lpx(d, LIB_MARGIN) + (int32_t)col * (tw + gap), y0 + (int32_t)(row - first) * (th + gap), tw, th);
        ar = rect_of(t.x + pad, t.y + pad, art, art);
        if (sel) {
            sel_gui_round_rect(d->canvas, rect_of(t.x - 2, t.y - 2, t.w + 4, t.h + 4),
                               lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_RADIUS_LARGE]) + 2, ARGB(d->theme.token[SEL_GUI_TOKEN_ACCENT]));
        }
        sel_gui_round_rect(d->canvas, t, lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_RADIUS_LARGE]),
                           sel ? ARGB(d->theme.token[SEL_GUI_TOKEN_SURFACE]) : panel);
        grid_row_cover(d, ar, r, lib_reg ? i : UINT32_MAX, &d->pc_grid);
        lib_tile_text(d, ar, ar.y + ar.h + lpx(d, HOME_TILE_TEXT_GAP), r, ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT]),
                      ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT_MUTED]), 0);
        if (lib_reg) {
            lib_view_row_reg(d, mn, sel_engine_state(d->engine), i, t);
        } else if (r->key != NULL) {
            song_row_reg(d, t, r->key, 0, 0, 0);
        }
        sel_gui_dirty_add(d->canvas, t);
        d->grid_tiles++;
        d->grid_box = rect_union(d->grid_box, t);
    }
}

static void draw_lib_grid(desk* d, sel_gui_rect mn) {
    draw_grid_rows(d, mn, sel_engine_state(d->engine)->view, mn.y + lib_body_y(d, sel_engine_state(d->engine)), 1);
}

/* Fractional centre of the cover flow in 16.16 listing indices: the cursor at rest, gliding from flow_from to it as gl runs 0..65536. */
static int64_t flow_centre_fx(const desk* d, uint32_t cursor, int32_t gl) {
    int64_t to = (int64_t)cursor * 65536;
    return gl < 65536 ? to + (((d->flow_from - to) * (65536 - gl)) >> 16) : to;
}

/* Scrim alpha of slot k (the centre has none); slots past FLOW_SIDES keep the last one. */
static const uint8_t flow_scrim[FLOW_SIDES + 1] = {0u, 90u, 150u, 150u};

/* One cover at the fractional slot distance a (16.16, >= 0): size, offset from the centre line and scrim alpha interpolated
 * between the integer slots floor(a) and floor(a)+1. Past the last slot the cover shrinks to nothing behind its neighbour, so a
 * cover entering or leaving the stack never pops. At whole slots this is exactly flow_size / flow_off / flow_scrim. */
static void flow_slot(int32_t big, int32_t a, int32_t* sz, int32_t* off, int32_t* alpha) {
    int32_t k = a >> 16, f = a & 0xFFFF;
    if (k >= FLOW_SIDES) {
        *sz = (int32_t)((int64_t)flow_size(big, FLOW_SIDES) * (65536 - f) >> 16);
        *off = flow_off(big, FLOW_SIDES);
        *alpha = flow_scrim[FLOW_SIDES];
        return;
    }
    *sz = flow_size(big, k) + (int32_t)((int64_t)(flow_size(big, k + 1) - flow_size(big, k)) * f >> 16);
    *off = flow_off(big, k) + (int32_t)((int64_t)(flow_off(big, k + 1) - flow_off(big, k)) * f >> 16);
    *alpha = (int32_t)flow_scrim[k] + (int32_t)((int64_t)((int32_t)flow_scrim[k + 1] - (int32_t)flow_scrim[k]) * f >> 16);
}

/* Stacked tiles: the centre at full size, neighbours smaller (integer fifths of it) and offset towards the sides, a scrim
 * darkening the far ones. No perspective. Each cover sits at a fractional slot p = index - centre (16.16); its size, offset and
 * scrim are continuous in |p| and the covers are drawn far to near, so a glide moves every cover smoothly from its old slot to
 * its new one (I400). */
static void draw_flow_rows(desk* d, sel_gui_rect mn, const sel_view_state* v, int32_t body_y, int lib_reg) {
    int32_t mg = lpx(d, LIB_MARGIN), right = mn.x + mn.w; /* the flow has no Statistics strip and uses the whole main width (I382, I391) */
    int32_t ref = lpx(d, FLOW_ART), avail = right - mn.x - 2 * mg, cx = (mn.x + right) / 2, y0;
    int32_t capt = lpx(d, HOME_TILE_TEXT_GAP + HOME_TITLE_H + HOME_META_H); /* the caption block under the centre cover */
    int32_t top = body_y + mg, area_h = mn.y + mn.h - mg - top; /* free height: below the tab row to the panel bottom */
    int32_t full = flow_stack_w(ref); /* width of the whole stack at the reference size */
    int32_t fit_w = ref * (avail > 0 ? avail : 0) / full, fit_h = area_h - capt; /* the centre cover that fills the width / the height */
    int32_t big = fit_w < fit_h ? fit_w : fit_h;
    big = big > lpx(d, 48) ? big : lpx(d, 48); /* the floor; the cover size follows the free area (I382, I391) */
    y0 = top + (area_h - (big + capt)) / 2;    /* the stack and its caption centred vertically in the free area */
    d->flow_area = rect_of(mn.x + mg, top, avail, area_h);
    int32_t gl = sel_gui_tween_value(&d->tl, ANIM_KEY_FLOW, 65536);
    int64_t c = flow_centre_fx(d, v->cursor, gl);
    int64_t g0 = (c >> 16) - FLOW_SIDES, g;
    struct { int64_t g; int32_t a, p; } cv[2 * FLOW_SIDES + 4];
    uint32_t n = 0u, i, j;
    const sel_view_row* cr = v->cursor >= v->window_start && v->cursor - v->window_start < v->row_n ? &v->rows[v->cursor - v->window_start] : NULL;
    d->flow_draws++;
    d->frame_flow = gl < 65536 ? 2 : 1;
    d->flow_centre = v->cursor;
    d->grid_box = rect_of(0, 0, 0, 0);
    d->flow_cover = rect_of(0, 0, 0, 0);
    d->flow_rec_n = 0u;
    /* The covers that can be visible: |p| < FLOW_SIDES + 1. Insertion sort by |p| descending (stable: the left one of a tie first). */
    for (g = g0 - 1; g <= g0 + 2 * FLOW_SIDES + 2 && n < 2u * FLOW_SIDES + 4u; g++) {
        int64_t p = g * 65536 - c;
        int32_t a = (int32_t)(p < 0 ? -p : p);
        if (g < 0 || g < (int64_t)v->window_start || g >= (int64_t)v->window_start + (int64_t)v->row_n || a >= (FLOW_SIDES + 1) * 65536) {
            continue;
        }
        for (i = n; i > 0u && cv[i - 1u].a < a; i--) {
            cv[i] = cv[i - 1u];
        }
        cv[i].g = g;
        cv[i].a = a;
        cv[i].p = p < 0 ? -1 : 1;
        n++;
    }
    for (j = 0u; j < n; j++) {
        int32_t sz, off, alpha;
        sel_gui_rect r;
        const sel_view_row* row = &v->rows[cv[j].g - (int64_t)v->window_start];
        flow_slot(big, cv[j].a, &sz, &off, &alpha);
        r = rect_of(cx + cv[j].p * off - sz / 2, y0 + (big - sz) / 2, sz, sz);
        d->grid_box = rect_union(d->grid_box, r);
        if (j + 1u == n) { /* the cover nearest the centre carries the shadow and the click target */
            d->flow_cover = r;
            sel_gui_shadow_draw(d->canvas, &d->shadow, r, 0); /* the cover flow art is square */
        }
        grid_row_cover(d, r, row, lib_reg ? (uint32_t)(cv[j].g - (int64_t)v->window_start) : UINT32_MAX, &d->pc_flow);
        if (alpha > 0) {
            sel_gui_scrim(d->canvas, r, ARGB(d->theme.token[SEL_GUI_TOKEN_SCRIM]) & 0xFFFFFFu, (uint32_t)alpha);
        }
        if (j + 1u == n) {
            if (lib_reg) {
                lib_view_row_reg(d, mn, sel_engine_state(d->engine), (uint32_t)(cv[j].g - (int64_t)v->window_start), r);
            } else if (row->key != NULL) {
                song_row_reg(d, r, row->key, 0, 0, 0);
            }
        }
        d->flow_rec[j].g = (int32_t)cv[j].g;
        d->flow_rec[j].x = r.x;
        d->flow_rec[j].w = r.w;
        d->flow_rec[j].a = cv[j].a;
    }
    d->flow_rec_n = n;
    if (cr != NULL) {
        lib_tile_text(d, rect_of(cx - big / 2, y0 + big + lpx(d, HOME_TILE_TEXT_GAP), big, 0), y0 + big + lpx(d, HOME_TILE_TEXT_GAP), cr,
                      ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT]), ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT_MUTED]), 1);
    }
    sel_gui_dirty_add(d->canvas, rect_of(mn.x, top, right - mn.x, area_h)); /* the whole free area: the stack moves with the canvas size */
}

static void draw_lib_flow(desk* d, sel_gui_rect mn) {
    draw_flow_rows(d, mn, sel_engine_state(d->engine)->view, mn.y + lib_body_y(d, sel_engine_state(d->engine)), 1);
}

static void draw_library(desk* d) {
    const sel_state* st = sel_engine_state(d->engine);
    if (d->page != PG_FOLDERS) {
        d->lib_off_page++;
    }
    sel_gui_rect mn = draw_panel(d, "main");
    int32_t mg = lpx(d, LIB_MARGIN);
    d->add_drawn = 0u;
    uint32_t text = ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT]), muted = ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT_MUTED]);
    uint32_t i = 0;
    const char* p;
    sel_gui_rect col = rect_of(mn.x + mg, mn.y + lpx(d, LIB_TITLE_Y), mn.w - 2 * mg, lpx(d, page_title_h(d)));
    sel_gui_clip_push(d->canvas, mn);
    d->library_sig = lib_hash(d, st);
    d->empty_text[0] = '\0';
    {
        uint32_t m;
        draw_text(d, col, "Library", 1, 32, text);
        {
            sel_gui_rect ab = lib_add_rect(d, mn);
            if (add_mode(d)) { /* I652b: add mode replaces Add Folder and Clear Library by the way back */
                {
                    char label[160];
                    snprintf(label, sizeof label, "Back to %s", d->add_name);
                    draw_button(d, add_back_rect(d, mn), ARGB(d->theme.token[SEL_GUI_TOKEN_TRACK]), label, lib_title_btn_px(d), ARGB(d->theme.token[SEL_GUI_TOKEN_ACCENT]));
                }
            } else if (d->scan_pending) {
                lib_scan_draw(d, mn);
            } else {
                draw_button(d, ab, ARGB(d->theme.token[SEL_GUI_TOKEN_TRACK]), "Add Folder", lib_title_btn_px(d), ARGB(d->theme.token[SEL_GUI_TOKEN_ACCENT]));
                draw_button(d, lib_clear_rect(d, mn), ARGB(d->theme.token[SEL_GUI_TOKEN_TRACK]), "Clear Library", lib_title_btn_px(d), ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT]));
                draw_lib_styles(d, mn, st);
            }
        }
        tl_draw(d, lib_tools_rect(d, mn)); /* I611: the Library page's search field and Sort by menu, the same row as the sidebar's */
        d->lib_tools_draws++;
        d->lib_tabs_drawn += LIB_TABS; /* I678i: the tab row has two tabs, never the six of the Library page */
        for (m = 0; m < LIB_TABS; m++) { /* the mode tabs: the active one in the accent colour */
            sel_gui_rect tr = lib_tab_rect(d, mn, m);
            const char* tname = sel_uim_libmode_name(k_folder_tab_mode[m]);
            if (k_folder_tab_mode[m] == d->lib_mode) { /* the active tab is filled like the selected Home and Search chips (I362) */
                draw_button_pad(d, tr, ARGB(d->theme.token[SEL_GUI_TOKEN_ACCENT]), tname, 14, ARGB(d->theme.token[SEL_GUI_TOKEN_ACCENT_TEXT]),
                                lib_tab_pad(d));
            } else { /* I547: the centring pad is the tab's own pad, so a tab squeezed below 8 px still shows its whole label */
                draw_text_centered_pad(d, tr, tname, 0, 14, muted, lib_tab_pad(d)); /* centred as on the active pill (I445) */
            }
        }
        if (lib_view_on(d, st) && d->lib_mode == 2u && st->view->drill_n > 0u) {
            char crumb[160];
            snprintf(crumb, sizeof crumb, "Back from %s", st->view->drill[st->view->drill_n - 1u]);
            draw_text(d, lib_crumb_rect(d, mn), crumb, 0, 14, muted);
        }
        if (lib_grid_on(st)) {
            if (st->view->style == (uint32_t)SEL_VIEW_STYLE_GRID) {
                draw_lib_grid(d, mn);
            } else {
                draw_lib_flow(d, mn);
            }
            p = NULL;
        } else if (lib_view_on(d, st)) { /* the engine's tree or column rows: node rows with their song count, songs indented */
            uint32_t k;
            for (k = 0; k < st->view->row_n; k++) {
                const sel_view_row* vr = &st->view->rows[k];
                sel_gui_rect r = lib_row_rect(d, mn, k);
                char line[48];
                int node = vr->kind == (uint32_t)SEL_ROW_NODE;
                int head = vr->kind == (uint32_t)SEL_ROW_HEADING || vr->kind == (uint32_t)SEL_ROW_DIR;
                int32_t ind = (int32_t)vr->depth * lpx(d, 24);
                if (d->kb_list && st->view->window_start + k == st->view->cursor) { /* the keyboard focus row */
                    sel_gui_fill_rect(d->canvas, r, ARGB(d->theme.token[SEL_GUI_TOKEN_SURFACE_RAISED]));
                }
                {   /* I690: a song row of the list whose song plays takes the accent ink */
                    char lp[sizeof((song_ref*)0)->path];
                    int pl = !node && !head && (vr->kind == (uint32_t)SEL_ROW_HOME ? vr->album == 0u && song_playing(d, vr->key)
                                                                                     : sel_engine_view_row_path(d->engine, k, lp, sizeof lp) && song_playing(d, lp));
                    draw_text(d, rect_of(r.x + ind, r.y, r.w * 2 / 3 - ind, r.h), vr->text != NULL ? vr->text : "", node || head || pl, 15,
                              row_ink(d, r, pl || (node && vr->expanded) ? ARGB(d->theme.token[SEL_GUI_TOKEN_ACCENT]) : vr->kind == (uint32_t)SEL_ROW_HEADING ? muted : text));
                }
                if (node) {
                    snprintf(line, sizeof line, "%u songs", (unsigned)vr->count);
                    draw_text(d, rect_of(r.x + r.w * 2 / 3, r.y, r.w / 3, r.h), line, 0, 14, row_ink(d, r, muted));
                } else if (vr->kind == (uint32_t)SEL_ROW_DIR) {
                    draw_text(d, rect_of(r.x + r.w * 2 / 3, r.y, r.w / 3, r.h), "Folder", 0, 14, row_ink(d, r, muted));
                }
                sel_gui_dirty_add(d->canvas, r);
                lib_view_row_reg(d, mn, st, k, r);
            }
            for (k = 0; k < st->view->row_n && st->view->rows[k].kind == (uint32_t)SEL_ROW_HEADING; k++) {
            }
            if (k == st->view->row_n && (st->composers == NULL || *st->composers == '\0')) { /* I548 (e): no rows but headings (History lists its shelf headings) in an empty library: the same hint as the Composers page */
                draw_empty_state(d, mn, lib_list_top(d, mn, st), "Your library is empty. Choose Add Folder to scan your music.");
            }
            p = NULL;
        } else {
        sel_gui_clip_push(d->canvas, rect_of(mn.x, lib_list_top(d, mn, st), mn.w, mn.y + mn.h - lib_list_top(d, mn, st)));
        for (p = lib_rows_first(st); p != NULL && *p != '\0'; i++) {
            const char* disp = lib_next(p);
            const char* cnt = lib_next(disp);
            sel_gui_rect r = lib_row_rect(d, mn, i);
            char line[48];
            if (r.y + r.h > lib_list_top(d, mn, st) && r.y < mn.y + mn.h) { /* only the rows in view are drawn (I310) */
                if ((int32_t)i == d->lib_focus) { /* the keyboard focus row (I498) */
                    sel_gui_fill_rect(d->canvas, r, ARGB(d->theme.token[SEL_GUI_TOKEN_SURFACE_RAISED]));
                }
                draw_text(d, rect_of(r.x, r.y, r.w * 2 / 3, r.h), disp, 1, 15, row_ink(d, r, text));
                snprintf(line, sizeof line, "%s songs", cnt);
                draw_text(d, rect_of(r.x + r.w * 2 / 3, r.y, r.w / 3, r.h), line, 0, 14, row_ink(d, r, muted));
                sel_gui_dirty_add(d->canvas, r);
            }
            p = lib_rows_next(st, p);
        }
        sel_gui_clip_pop(d->canvas);
        if (i == 0u) { /* I548 (e): an empty library names the way to fill it, in the one-line empty state the other pages use */
            draw_empty_state(d, mn, lib_list_top(d, mn, st), "Your library is empty. Choose Add Folder to scan your music.");
        }
        }
    }
    draw_lib_bar(d);
    if (st->stats != NULL && lib_stats_rect(d, mn, st).w > 0) { /* the Stats page (F24): the library totals as a strip of heading and label-over-value cells */
        sel_gui_rect sr = lib_stats_rect(d, mn, st);
        const sel_gui_atlas* hd = desk_font(d, 1, 18);
        const sel_gui_atlas* la = desk_font(d, 0, 14);
        const sel_gui_atlas* va = desk_font(d, 1, 14);
        int32_t x = sr.x, end = sr.x + sr.w, line = lpx(d, LIB_STATS_LINE);
        if (hd != NULL && la != NULL && va != NULL) {
            int32_t hw = sel_gui_text_width(hd, "Statistics", 10u);
            draw_text(d, rect_of(x, sr.y, hw, line), "Statistics", 1, 18, text);
            x += hw + lib_gap(d);
            end -= lpx(d, LIB_STATS_MORE_W) + lib_gap(d); /* the More control keeps the right end (I455) */
            for (p = st->stats; *p != '\0';) { /* as many cells as the strip holds, in the order of the statistics */
                const char* val = lib_next(p);
                int32_t lw = sel_gui_text_width(la, p, strlen(p)), vw = sel_gui_text_width(va, val, strlen(val)), cw = lw > vw ? lw : vw;
                int one_row = desk_bitmap_text(d); /* I548: label and value side by side on the label row */
                if (one_row) {
                    cw = lw + lib_gap(d) / 2 + vw;
                }
                if (x + cw > end) {
                    break;
                }
                draw_text(d, rect_of(x, sr.y, one_row ? lw : cw, line), p, 0, 14, muted);
                if (one_row) {
                    draw_text(d, rect_of(x + lw + lib_gap(d) / 2, sr.y, vw, line), val, 1, 14, text);
                } else {
                    draw_text(d, rect_of(x, sr.y + line, cw, line), val, 1, 14, text);
                }
                x += cw + lib_gap(d);
                p = lib_next(val);
            }
            {
                sel_gui_rect mr = lib_stats_more_rect(d, sr);
                hover_reg(d, mr);
                draw_text_centered(d, mr, d->stats_open ? "Less" : "More", 1, 14, hover_ink_of(d, mr, muted, text));
                sel_gui_dirty_add(d->canvas, mr);
            }
            if (d->stats_open) { /* the expanded panel over the grid: one raised surface, label left and value right in two columns */
                sel_gui_rect pr = lib_stats_panel_rect(d, sr, st);
                int32_t pad = 2 * lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_SPACING]), colw = (pr.w - 2 * pad) / LIB_STATS_PANEL_COLS;
                uint32_t n = 0, per = (lib_stats_rows(st) + LIB_STATS_PANEL_COLS - 1u) / LIB_STATS_PANEL_COLS;
                if (d->shadow_ready) {
                    sel_gui_shadow_draw(d->canvas, &d->shadow, pr, lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_RADIUS]));
                }
                sel_gui_round_rect(d->canvas, pr, lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_RADIUS]), ARGB(d->theme.token[SEL_GUI_TOKEN_SURFACE_RAISED]));
                for (p = st->stats; *p != '\0'; n++) {
                    const char* val = lib_next(p);
                    int32_t cx = pr.x + pad + (int32_t)(n / per) * colw, cy = pr.y + pad + (int32_t)(n % per) * line;
                    int32_t vw = sel_gui_text_width(va, val, strlen(val)), cw = colw - lib_gap(d);
                    draw_text(d, rect_of(cx, cy, cw - vw > 0 ? cw - vw : 0, line), p, 0, 14, muted);
                    draw_text(d, rect_of(cx + cw - vw, cy, vw, line), val, 1, 14, text);
                    p = lib_next(val);
                }
                d->stats_panel_rows = n;
                d->stats_panel_draws++;
                sel_gui_dirty_add(d->canvas, pr);
            }
        }
    }
    sel_gui_clip_pop(d->canvas);
    d->home_parts |= HP_SECTION;
}

/* Switches the Library page mode. Tree and columns list the library's songs through the engine view, the sequence the
 * text UI's library screen sends (LIBRARY_SEARCH, VIEW_SOURCE query, VIEW_STYLE, VIEW_GROUP format). */
/* ---- Sources tab (I265): one card per row of the engine's SOURCES reply, generic over the registered sources. The
 * switch writes the library.sources_off pref (PREF_SET) and asks SOURCES again; Sync of a remote source sends
 * LIBRARY_SYNC with the home the engine reported. Rects come from these helpers: drawing, hit-testing and the dump. */
enum { SRC_TOGGLE_W = 44, SRC_TOGGLE_H = 24, SRC_SYNC_W = 96, SRC_ICON = 20 };

static sel_gui_rect src_toggle_rect(desk* d, sel_gui_rect r) {
    return rect_of(r.x + r.w - lpx(d, 16 + SRC_TOGGLE_W), r.y + (r.h - lpx(d, SRC_TOGGLE_H)) / 2, lpx(d, SRC_TOGGLE_W), lpx(d, SRC_TOGGLE_H));
}

static sel_gui_rect src_sync_rect(desk* d, sel_gui_rect r) {
    return rect_of(src_toggle_rect(d, r).x - lpx(d, 16 + SRC_SYNC_W), r.y + (r.h - lpx(d, 32)) / 2, lpx(d, SRC_SYNC_W), lpx(d, 32));
}

static int src_remote(const desk* d, uint32_t i) {
    return strcmp(d->src[i].kind, "remote") == 0;
}

/* Sends the switch-off list with source `flip` toggled and the `force_off` sources off (Never, I602), then asks SOURCES for the stored state. */
static int src_off_send(desk* d, uint32_t flip, uint32_t force_off) {
    sel_uim_source m[32];
    char line[256];
    uint32_t k, n = d->src_n < 32u ? d->src_n : 32u;
    for (k = 0; k < n; k++) {
        m[k].key = d->src[k].key;
        m[k].path = d->src[k].path;
        m[k].enabled = d->src[k].enabled;
    }
    if (sel_uim_sources_off_line(m, n, flip, force_off, line, sizeof line) != 0u) {
        desk_send(d, line);
    }
    d->src_wait = 1;
    return desk_send(d, "SOURCES");
}

static int src_toggle_send(desk* d, uint32_t flip) {
    return src_off_send(d, flip, 0u);
}

/* One LIBRARY_SYNC per ticked source, the same message the Sources page Sync button sends (src_sync_send). */
static void src_sync_send(desk* d, uint32_t i) {
    char line[256], msg[64];
    sel_uim_sync_line(d->src[i].key, d->src[i].path, line, sizeof line);
    d->sync_pending |= 1u << i;
    d->sync_failed &= ~(1u << i);
    snprintf(d->src_syncing, sizeof d->src_syncing, "%s", d->src[i].name);
    snprintf(msg, sizeof msg, "Syncing %s", d->src[i].name);
    desk_toast(d, msg);
    desk_send(d, line);
}

/* The row of src[] whose pending (or failed) sync the engine named: ERROR sync_failed carries the path sent in LIBRARY_SYNC,
 * sync_busy the key, and the success LIBRARY_RESULT the path. -1 when no such row. */
static int sync_row_find(const desk* d, uint32_t mask, const char* name) {
    uint32_t i;
    for (i = 0; i < d->src_n; i++) {
        if ((mask & (1u << i)) && (strcmp(d->src[i].key, name) == 0 || strcmp(d->src[i].path, name) == 0)) {
            return (int)i;
        }
    }
    return -1;
}

/* A sync outcome for one row (I582): clears pending, marks failed, asks SOURCES so the due flags follow. */
static void sync_row_done(desk* d, int row, int failed) {
    d->sync_pending &= ~(1u << row);
    if (failed) {
        d->sync_failed |= 1u << row;
    } else {
        d->sync_failed &= ~(1u << row);
    }
    d->src_wait = 1;
    desk_send(d, "SOURCES");
    d->repaint = 1;
}

/* Stores the per-source snooze (library.sync_snoozed, I582) of the banner rows left unticked at Sync: "id:days" for them and
 * for every source the engine already reported a snooze for, then asks SOURCES. The global notice stays untouched. */
static void nag_snooze(desk* d, uint32_t unticked) {
    char line[160], csv[128];
    size_t n = 0;
    uint32_t k;
    csv[0] = '\0';
    for (k = 0; k < d->src_n; k++) {
        int64_t days = (unticked & (1u << k)) ? (int64_t)time(NULL) / 86400 : d->src[k].snooze / 86400;
        char entry[48];
        size_t len;
        if (days <= 0) {
            continue;
        }
        len = (size_t)snprintf(entry, sizeof entry, "%s%s:%lld", n > 0 ? "," : "", d->src[k].key, (long long)days);
        if (n + len < sizeof csv) {
            memcpy(csv + n, entry, len + 1u);
            n += len;
        }
    }
    snprintf(line, sizeof line, "PREF_SET library.sync_snoozed \"%s\"", csv);
    desk_send(d, line);
    d->src_wait = 1;
    desk_send(d, "SOURCES");
}

/* A click on the sync offer; 1 when it hit one of its controls. Not now stores library.sync_notice_at, which the engine
 * counts from. Sync stores no global notice (I582): its ticked rows stay due until their sync succeeds. */
static void nag_dismiss(desk* d) {
    char line[96];
    sel_uim_sync_notnow_line((int64_t)time(NULL), line, sizeof line);
    desk_send(d, line);
    d->src_wait = 1;
    desk_send(d, "SOURCES");
}

static int lib_set_mode(desk* d, uint32_t mode) {
    char lines[SEL_UIM_LIB_LINES][SEL_UIM_LIB_LINE_MAX];
    uint32_t i, n;
    if (mode == d->lib_mode) {
        return 0;
    }
    d->lib_mode = mode;
    d->lib_q16 = 0;
    n = sel_uim_libmode_lines(mode, (uint32_t)LIB_WINDOW_ROWS, lines); /* the shared model owns what each mode sends (MUI-P8-3) */
    for (i = 0; i < n; i++) {
        desk_send(d, lines[i]);
    }
    return 1;
}

/* I453: Grid and Cover Flow show the library's songs, the query source Tree and Columns list (LIBRARY_SEARCH "" then
 * VIEW_SOURCE query), never the play queue or the browse folder. The style click and the Library page entry both end here. */
static void lib_grid_source(desk* d) {
    char line[64];
    desk_send(d, "LIBRARY_SEARCH \"\"");
    desk_send(d, "VIEW_SOURCE query");
    snprintf(line, sizeof line, "VIEW_WINDOW %u", (unsigned)LIB_WINDOW_ROWS);
    desk_send(d, line);
}

/* I390: a click on a chooser segment. Grid and Cover Flow draw the Composers list, so they enter that mode as its tab does,
 * over the library's songs (I453); List returns to the regular rows. The same VIEW_STYLE command the --exec and --input
 * paths send. */
static int lib_style_click(desk* d, uint32_t i) {
    if (i != 2u) {
        d->lib_mode = 0u;
        d->lib_q16 = 0;
        lib_grid_source(d);
    }
    desk_send(d, lib_style_cmd[i]);
    return 1;
}

/* A request with one quoted argument. A canon holds spaces: the codec reads a backslash as the escape of the next character. */
static void desk_quoted_request(char* line, size_t cap, const char* head, const char* arg) {
    size_t o = (size_t)snprintf(line, cap, "%s \"", head);
    for (const char* c = arg; *c != '\0' && o + 3u < cap; c++) {
        if (*c == '"' || *c == '\\') {
            line[o++] = '\\';
        }
        line[o++] = *c;
    }
    line[o++] = '"';
    line[o] = '\0';
}

static void desk_composer_open(desk* d, const char* canon, const char* display) {
    desk_route(d, SEL_ROUTE_BROWSE_TILE, (uint32_t)SEL_LIB_ITEM_COMPOSER, 0u, canon, display, "");
}

/* Largest Library scroll offset in logical pixels, Q16: the UI-drawn list's content height minus the room under the list
 * top, or for the engine view modes the rows beyond its window. */
static int32_t lib_max_q16(desk* d, const sel_state* st) {
    sel_gui_rect mn = panel_rect(d, "main");
    int32_t max;
    if (lib_ui_list(d, st)) {
        max = (int32_t)lib_list_n(st) * LIB_ROW_H - px_to_logical(d, mn.y + mn.h - lib_list_top(d, mn, st));
    } else {
        max = ((int32_t)st->view->row_count - (int32_t)st->view->window_rows) * LIB_ROW_H;
    }
    return max > 0 ? max << 16 : 0;
}

/* A wheel over the Library page (DUI-P3-4, DUI-P4-3): the Songs pattern, a clamped Q16 pixel offset. The UI-drawn lists
 * draw at the offset; the engine view modes (tree, columns, Files, History) move the engine's window instead, by a
 * VIEW_CURSOR on the row that brings the wanted top row into the window (the view keeps its cursor in view). */
static int lib_scroll_set(desk* d, int64_t v) {
    const sel_state* st = sel_engine_state(d->engine);
    int32_t max_q16 = lib_max_q16(d, st);
    d->lib_q16 = v < 0 ? 0 : (v > max_q16 ? max_q16 : (int32_t)v);
    if (!lib_ui_list(d, st)) {
        uint32_t top = (uint32_t)(d->lib_q16 >> 16) / LIB_ROW_H, last = st->view->row_count > 0u ? st->view->row_count - 1u : 0u;
        uint32_t cur = top > st->view->window_start ? top + st->view->window_rows - 1u : top;
        char line[48];
        snprintf(line, sizeof line, "VIEW_CURSOR %u", (unsigned)(cur > last ? last : cur));
        desk_send(d, line);
    }
    draw_home(d); /* the main region only, as the Songs wheel does (I310): no full canvas repaint per wheel event */
    return 1;
}

static int lib_wheel(desk* d, int32_t dy) {
    return lib_scroll_set(d, (int64_t)d->lib_q16 - (int64_t)dy * SCROLL_NOTCH);
}

/* I541: every Library list scrolls without a wheel. The scrollbar sits in the right margin of the list: a drag sets the
 * offset, a click above or below the handle moves one page, and the keys below move the Sources list (the other lists
 * have their own list keys). The wheel, the bar and the keys all end in lib_scroll_set. */
enum { LIB_BAR_X = 18, LIB_BAR_W = 12, LIB_BAR_MIN_H = 24 };

static int lib_bar_rects(desk* d, sel_gui_rect* track, sel_gui_rect* handle) {
    const sel_state* st = sel_engine_state(d->engine);
    sel_gui_rect mn;
    int32_t max, top, view, content, hh;
    if (d->page != PG_FOLDERS || lib_grid_on(st) || st->composers == NULL) {
        return 0;
    }
    max = lib_max_q16(d, st);
    if (max <= 0) {
        return 0;
    }
    mn = panel_rect(d, "main");
    top = lib_list_top(d, mn, st);
    *track = rect_of(mn.x + mn.w - lpx(d, LIB_BAR_X), top, lpx(d, LIB_BAR_W), mn.y + mn.h - top);
    content = (int32_t)(lib_ui_list(d, st) ? lib_list_n(st) : st->view->row_count) * LIB_ROW_H;
    view = content - (max >> 16);
    hh = (int32_t)((int64_t)track->h * view / (content > 0 ? content : 1));
    hh = hh < lpx(d, LIB_BAR_MIN_H) ? lpx(d, LIB_BAR_MIN_H) : hh;
    hh = hh > track->h ? track->h : hh;
    *handle = rect_of(track->x, track->y + (int32_t)((int64_t)(track->h - hh) * (d->lib_q16 > max ? max : d->lib_q16) / max), track->w, hh);
    return 1;
}

static void draw_lib_bar(desk* d) {
    sel_gui_rect t, h;
    if (!lib_bar_rects(d, &t, &h)) {
        return;
    }
    d->lib_bar_draws++;
    sel_gui_round_rect(d->canvas, t, t.w / 2, ARGB(d->theme.token[SEL_GUI_TOKEN_TRACK]));
    sel_gui_round_rect(d->canvas, h, h.w / 2, ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT_MUTED]));
    sel_gui_dirty_add(d->canvas, t);
}

/* A press on the Library scrollbar: a click above or below the handle moves one page, a drag that starts in the track
 * puts the handle's centre where it is released. 1 when the event was on the bar. */
static int lib_bar_pointer(desk* d, const sel_gui_input* ev) {
    sel_gui_rect t, h, hit;
    int32_t span, page;
    int held = ev->kind == SEL_GUI_IN_MOVE && ev->code == 1u; /* I549: the primary button is down; x2 y2 is where it was pressed */
    if (!(ev->kind == SEL_GUI_IN_CLICK && ev->button == 0u) && ev->kind != SEL_GUI_IN_DRAG && !held) {
        return 0;
    }
    if (!lib_bar_rects(d, &t, &h)) {
        return 0;
    }
    hit = rect_of(t.x - lpx(d, 6), t.y, t.w + lpx(d, 6), t.h);
    if (!in_rect(hit, held ? ev->x2 : ev->x, held ? ev->y2 : ev->y)) {
        return 0;
    }
    span = t.h - h.h;
    if (held) { /* the handle follows the pointer while the button is held, not only at the release */
        int32_t at = ev->y - t.y - h.h / 2;
        at = at < 0 ? 0 : (at > span ? span : at);
        sel_gui_rect mn = panel_rect(d, "main");
        int32_t lt = lib_list_top(d, mn, sel_engine_state(d->engine)), moved;
        d->lib_held_frame = 1;
        /* Only the list body and the bar change while the handle moves: the clip keeps the repaint, and so the dirty
         * rectangle, to the list instead of the whole main panel. */
        sel_gui_clip_push(d->canvas, rect_of(mn.x, lt, mn.w, mn.y + mn.h - lt));
        moved = lib_scroll_set(d, span > 0 ? (int64_t)at * lib_max_q16(d, sel_engine_state(d->engine)) / span : 0);
        sel_gui_clip_pop(d->canvas);
        return moved;
    }
    if (ev->kind == SEL_GUI_IN_DRAG) {
        int32_t at = ev->y2 - t.y - h.h / 2;
        at = at < 0 ? 0 : (at > span ? span : at);
        return lib_scroll_set(d, span > 0 ? (int64_t)at * lib_max_q16(d, sel_engine_state(d->engine)) / span : 0);
    }
    page = px_to_logical(d, t.h) - LIB_ROW_H;
    page = page < LIB_ROW_H ? LIB_ROW_H : page;
    if (ev->y < h.y) {
        return lib_scroll_set(d, (int64_t)d->lib_q16 - ((int64_t)page << 16));
    }
    if (ev->y >= h.y + h.h) {
        return lib_scroll_set(d, (int64_t)d->lib_q16 + ((int64_t)page << 16));
    }
    return 1;
}

/* A click on a composer row opens that composer's collection page (I678f). */
static int library_click(desk* d, int32_t x, int32_t y) {
    const sel_state* st = sel_engine_state(d->engine);
    sel_gui_rect mn = panel_rect(d, "main");
    uint32_t i = 0;
    const char* p;
    char line[256];
    if (add_mode(d)) { /* I652b: Back, or Add on a song row */
        uint32_t k;
        if (in_rect(add_back_rect(d, mn), x, y)) {
            add_mode_back(d);
            return 1;
        }
        for (k = 0; k < d->sm_n; k++) {
            if (d->sm_rows[k].more.w > 0 && in_rect(add_btn_rect(d, d->sm_rows[k].r), x, y)) {
                pls_add_key(d, d->add_id, d->sm_rows[k].ref.path);
                return 1;
            }
        }
    }
    if (d->page == PG_FOLDERS && lib_stats_on(st)) { /* I455: More toggles the panel; the open panel owns clicks on it, any other click closes it */
        sel_gui_rect sr = lib_stats_rect(d, mn, st);
        if (in_rect(lib_stats_more_rect(d, sr), x, y)) {
            lib_stats_toggle(d, !d->stats_open);
            return 1;
        }
        if (d->stats_open) {
            int inside = in_rect(lib_stats_panel_rect(d, sr, st), x, y);
            lib_stats_toggle(d, 0);
            if (inside) {
                return 1;
            }
        }
    }
    if (d->page == PG_FOLDERS && !d->scan_pending && in_rect(lib_add_rect(d, mn), x, y)) {
        return desk_add_folder(d);
    }
    if (d->page == PG_FOLDERS && !d->scan_pending && in_rect(lib_clear_rect(d, mn), x, y)) {
        dlg_open_clear(d);
        draw_home(d);
        return 1;
    }
    if (d->page == PG_FOLDERS && !d->scan_pending) {
        uint32_t sg;
        for (sg = 0; sg < (uint32_t)LIB_STYLES; sg++) {
            if (in_rect(lib_style_rect(d, mn, sg), x, y)) {
                return lib_style_click(d, sg);
            }
        }
    }
    if (d->page == PG_FOLDERS) { /* I611: the Library page's tools row */
        int32_t tk = tl_at(d, lib_tools_rect(d, mn), x, y);
        if (tk >= 0) {
            d->lib_tools_clicks++;
            tl_click(d, lib_tools_rect(d, mn), tk);
            draw_home(d);
            return 1;
        }
    }
    if (d->page != PG_FOLDERS || st->composers == NULL) { /* the rows and tabs below are drawn on the Library page only (dui-search) */
        return 0;
    }
    {
        uint32_t m;
        for (m = 0; m < LIB_TABS; m++) {
            if (in_rect(lib_tab_rect(d, mn, m), x, y)) {
                return lib_set_mode(d, k_folder_tab_mode[m]);
            }
        }
        if (lib_view_on(d, st)) {
            uint32_t k;
            if (d->lib_mode == 2u && st->view->drill_n > 0u && in_rect(lib_crumb_rect(d, mn), x, y)) {
                return desk_send(d, "VIEW_DRILL 0 ..");
            }
            for (k = 0; k < st->view->row_n; k++) {
                if (lib_row_hit(d, mn, k, x, y)) {
                    snprintf(line, sizeof line, "VIEW_CURSOR %u", (unsigned)(st->view->window_start + k));
                    desk_send(d, line);
                    return desk_send(d, "VIEW_ACTIVATE"); /* a node expands or drills in, a song plays */
                }
            }
            return 0;
        }
    }
    for (p = st->composers; *p != '\0'; i++) {
        const char* disp = lib_next(p);
        if (lib_row_hit(d, mn, i, x, y)) {
            desk_composer_open(d, p, disp); /* I678f: the composer's collection page */
            return 1;
        }
        p = lib_next(lib_next(disp));
    }
    return 0;
}

/* ---- Search (DUI-P4-6): the query field on top, facet chips, and the engine's QUERY rows grouped by type. The engine
 * does the search (LIBRARY_SEARCH, the query language incl. `format:mod`); the debounce reuses the tooltip delay pattern
 * (a UI clock stamp, a wait hint for the idle block, a tick that fires once). */
enum { SEARCH_DEBOUNCE_MS = 250, SEARCH_ROW_H = 32, SEARCH_FACETS = 4, SEARCH_WINDOW_ROWS = 64 };
static const char* const search_facet_name[SEARCH_FACETS] = {"All", "Songs", "Albums", "Artists"};

/* Sends the pending query to the engine as the text UI's library screen does; an empty query leaves the results. */
static void desk_search_fire(desk* d) {
    char line[160];
    d->search_pending = 0;
    if (d->qlen == 0u) {
        d->search_on = 0;
        d->repaint = 1;
        d->uim.dirty |= SEL_UIM_DIRTY_ALL;
        return;
    }
    d->query[d->qlen < sizeof d->query ? d->qlen : sizeof d->query - 1u] = '\0';
    (void)sel_uim_search_line(d->query, line, sizeof line);
    desk_send(d, line);
    desk_send(d, "VIEW_SOURCE search"); /* the engine groups the result by type: Songs, Albums, Artists */
    /* the view style stays as the user picked it (grid, cover flow, list): draw_search renders the results in it (I615b) */
    snprintf(line, sizeof line, "VIEW_WINDOW %u", (unsigned)SEARCH_WINDOW_ROWS);
    desk_send(d, line);
    d->searches++;
    d->lib_searches++;
    d->search_on = 1;
    d->repaint = 1;
    d->uim.dirty |= SEL_UIM_DIRTY_ALL;
}

/* Milliseconds until the debounced search is due, 0 when none is pending (or it is due now). */
static uint32_t desk_search_wait(const desk* d) {
    uint32_t due = d->search_since + SEARCH_DEBOUNCE_MS;
    return d->search_pending && due > d->now_ms ? due - d->now_ms : 0u;
}

static void desk_search_tick(desk* d) {
    if (d->search_pending && d->now_ms >= d->search_since + SEARCH_DEBOUNCE_MS) {
        desk_search_fire(d);
    }
}

static sel_gui_rect search_facet_rect(desk* d, sel_gui_rect mn, uint32_t k) {
    const sel_gui_atlas* chip = desk_font(d, 1, 13);
    int32_t x = mn.x + lpx(d, HOME_MARGIN), i;
    for (i = 0; i < (int32_t)k && chip != NULL; i++) {
        x += sel_gui_text_width(chip, search_facet_name[i], strlen(search_facet_name[i])) + 2 * lpx(d, 14) + lpx(d, 8);
    }
    return rect_of(x, mn.y + lpx(d, SEARCH_FIELD_Y + SEARCH_FIELD_H + 16),
                   (chip != NULL ? sel_gui_text_width(chip, search_facet_name[k], strlen(search_facet_name[k])) : 40) + 2 * lpx(d, 14),
                   lpx(d, HOME_CHIP_H));
}

/* The Browse tile colours are design tokens, one per tile in turn. */
static const int browse_palette[BROWSE_PALETTE] = {SEL_GUI_TOKEN_ACCENT, SEL_GUI_TOKEN_HEADER_TINT, SEL_GUI_TOKEN_SUCCESS, SEL_GUI_TOKEN_ERROR, SEL_GUI_TOKEN_WARNING};
static const char* const browse_head[4] = {"Formats", "Composers", "Sources", "Folders"};

static void draw_browse(desk* d) {
    sel_gui_rect mn = region_rect(d, "main");
    browse_tile t[BROWSE_MAX];
    uint32_t n = browse_tiles(d, t, BROWSE_MAX), i, cols, kind, col, order;
    int32_t gap = lpx(d, GRID_GAP), margin = lpx(d, LIB_MARGIN), avail = mn.w - 2 * margin, tw, th = lpx(d, BROWSE_TILE_H), y = mn.y + lpx(d, 16);
    sel_gui_clip_push(d->canvas, mn);
    cols = avail >= lpx(d, BROWSE_TILE_MIN_W) ? (uint32_t)((avail + gap) / (lpx(d, BROWSE_TILE_MIN_W) + gap)) : 1u;
    tw = (avail - (int32_t)(cols - 1u) * gap) / (int32_t)cols;
    for (i = 0; i < BROWSE_MAX; i++) {
        d->browse_rect[i] = rect_of(0, 0, 0, 0);
    }
    for (order = 0; order < 4u; order++) {
        int head = 0;
        kind = (order + 3u) % 4u; /* I678i: the Folders tile first (kind 3), then Formats, Composers, Sources */
        col = 0;
        for (i = 0; i < n; i++) {
            sel_gui_rect r;
            char sub[24];
            int32_t pad;
            if (t[i].kind != kind) {
                continue;
            }
            if (!head) {
                draw_text(d, rect_of(mn.x + margin, y, avail, lpx(d, BROWSE_HEAD_H)), browse_head[kind], 1, 18, ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT]));
                y += lpx(d, BROWSE_HEAD_H);
                head = 1;
            }
            r = rect_of(mn.x + margin + (int32_t)col * (tw + gap), y, tw, th);
            pad = lpx(d, 12);
            if (r.y < mn.y + mn.h) {
                sel_gui_round_rect(d->canvas, r, lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_RADIUS_LARGE]), ARGB(d->theme.token[browse_palette[i % BROWSE_PALETTE]]));
                draw_text(d, rect_of(r.x + pad, r.y + pad, r.w - 2 * pad, lpx(d, 28)), t[i].label, 1, 18, ARGB(d->theme.token[SEL_GUI_TOKEN_ACCENT_TEXT]));
                snprintf(sub, sizeof sub, t[i].kind == 3u ? "Tree and Columns" : "%u songs", (unsigned)t[i].count);
                draw_text(d, rect_of(r.x + pad, r.y + r.h - pad - lpx(d, 18), r.w - 2 * pad, lpx(d, 18)), sub, 0, 13, ARGB(d->theme.token[SEL_GUI_TOKEN_ACCENT_TEXT]));
                d->browse_rect[i] = r;
            }
            col++;
            if (col == cols) {
                col = 0;
                y += th + gap;
            }
        }
        if (head && col != 0u) {
            y += th + gap;
        }
    }
    d->browse_n = n;
    d->browse_draws++;
    sel_gui_clip_pop(d->canvas);
    sel_gui_dirty_add(d->canvas, mn);
}

static int browse_click(desk* d, int32_t x, int32_t y) {
    browse_tile t[BROWSE_MAX];
    uint32_t n = browse_tiles(d, t, BROWSE_MAX), i;
    for (i = 0; i < n && i < d->browse_n; i++) {
        if (!in_rect(d->browse_rect[i], x, y)) {
            continue;
        }
        d->browse_opens++;
        if (t[i].kind == 3u) {
            d->browse_folders_opens++;
            desk_page_goto(d, PG_FOLDERS);
        } else if (t[i].kind == 1u) {
            char canon[SEL_COMPOSER_MAX]; /* copied first: the page change replaces the catalog */
            snprintf(canon, sizeof canon, "%s", t[i].arg);
            desk_composer_open(d, canon, canon); /* I678f */
        } else {
            snprintf(d->query, sizeof d->query, "%s:%s", t[i].kind == 0u ? "format" : "source", t[i].arg);
            d->qlen = strlen(d->query);
            d->search_facet = 0u;
            d->search_keep = 1;
            desk_page_goto(d, PG_SEARCH);
            d->search_keep = 0;
            d->browse_filter = 1;
            desk_search_fire(d);
        }
        return 1;
    }
    return 0;
}

static void draw_search(desk* d) {
    const sel_state* st = sel_engine_state(d->engine);
    sel_gui_rect mn = draw_panel(d, "main");
    int32_t mg = lpx(d, HOME_MARGIN), rad = lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_RADIUS_LARGE]), ic = lpx(d, 18);
    sel_gui_rect sr = search_field_rect(d, mn);
    uint32_t text = ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT]), muted = ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT_MUTED]);
    uint32_t k, shown = 0;
    int32_t y;
    char hint[64];
    d->empty_text[0] = '\0';
    const sel_view_state* v = st->view != NULL && st->view->source == SEL_VIEW_SEARCH ? st->view : NULL;
    sel_gui_clip_push(d->canvas, mn);
    if (!topbar_shown(d)) { /* I615: with a top bar the field is drawn there */
        sel_gui_round_rect(d->canvas, sr, rad, ARGB(d->theme.token[SEL_GUI_TOKEN_SURFACE]));
        sel_gui_icon_draw(d->canvas, &d->icons, SEL_GUI_ICON_SEARCH, rect_of(sr.x + lpx(d, 14), sr.y + (sr.h - ic) / 2, ic, ic), ic, muted);
        search_hint(d, hint, sizeof hint, search_facet_name, SEARCH_FACETS);
        desk_draw_query(d, sr, hint);
    }
    for (k = 0; k < SEARCH_FACETS; k++) { /* facet chips: the active one filled with the accent */
        sel_gui_rect cr = search_facet_rect(d, mn, k);
        draw_chip(d, cr, search_facet_name[k], k == d->search_facet, SEL_GUI_TOKEN_SURFACE);
    }
    y = mn.y + lpx(d, SEARCH_FIELD_Y + SEARCH_FIELD_H + 16) + lpx(d, HOME_CHIP_H + 16);
    d->search_groups[0] = d->search_groups[1] = d->search_groups[2] = 0;
    if (v != NULL && (d->browse_filter || v->style == (uint32_t)SEL_VIEW_STYLE_GRID || v->style == (uint32_t)SEL_VIEW_STYLE_COVERFLOW)) {
        /* I615b: the results follow the view mode the user picked. Grid and Cover Flow draw the entries of the shown facet as
         * artwork tiles, the same renderers as the Library; the group headings only feed the counts. Only song entries keep
         * their key, so only they open the song menu. */
        sel_view_row tiles[SEARCH_WINDOW_ROWS];
        sel_view_state fv = *v;
        uint32_t group = 0, n = 0;
        int show = 0;
        fv.cursor = 0;
        for (k = 0; k < v->row_n && n < SEARCH_WINDOW_ROWS; k++) {
            const sel_view_row* r = &v->rows[k];
            if (r->kind == SEL_ROW_HEADING) {
                const char* t = r->text != NULL ? r->text : "", *par = strchr(t, '(');
                group = strncmp(t, "Songs", 5) == 0 ? 1u : strncmp(t, "Albums", 6) == 0 ? 2u : 3u;
                show = d->search_facet == 0u || d->search_facet == group;
                d->search_groups[group - 1u] = (uint32_t)strtoul(par != NULL ? par + 1 : "0", NULL, 10);
            } else if (show) {
                tiles[n] = *r;
                if (group != 1u || r->album) {
                    tiles[n].key = NULL;
                }
                if (v->window_start + k == v->cursor) {
                    fv.cursor = n;
                }
                shown += group == 1u;
                n++;
            }
        }
        fv.window_start = 0;
        fv.row_n = fv.row_count = n;
        fv.rows = tiles;
        if (d->browse_filter || v->style == (uint32_t)SEL_VIEW_STYLE_GRID) {
            draw_grid_rows(d, mn, &fv, y, 0);
            d->search_grid_draws++;
        } else {
            draw_flow_rows(d, mn, &fv, y, 0);
            d->search_flow_draws++;
        }
        d->search_drawn = n;
    } else if (v != NULL) { /* the engine's groups: a heading row (type and count) starts each, its entries follow */
        uint32_t group = 0, drawn_all = 0;
        int show = 0;
        for (k = 0; k < v->row_n; k++) {
            const sel_view_row* r = &v->rows[k];
            const char* t = r->text != NULL ? r->text : "";
            if (r->kind == SEL_ROW_HEADING) {
                const char* par = strchr(t, '(');
                group = strncmp(t, "Songs", 5) == 0 ? 1u : strncmp(t, "Albums", 6) == 0 ? 2u : 3u;
                show = d->search_facet == 0u || d->search_facet == group;
                d->search_groups[group - 1u] = (uint32_t)strtoul(par != NULL ? par + 1 : "0", NULL, 10);
                if (show && y + lpx(d, 32) < mn.y + mn.h) {
                    draw_text(d, rect_of(mn.x + mg, y, mn.w - 2 * mg, lpx(d, 28)), t, 1, 18, text);
                    y += lpx(d, 32);
                    drawn_all++;
                }
            } else if (show && y + lpx(d, SEARCH_ROW_H) < mn.y + mn.h) {
                if (d->kb_list && v->window_start + k == v->cursor) { /* the keyboard focus row (I498) */
                    sel_gui_fill_rect(d->canvas, rect_of(mn.x + mg, y, mn.w - 2 * mg, lpx(d, SEARCH_ROW_H)), ARGB(d->theme.token[SEL_GUI_TOKEN_SURFACE_RAISED]));
                }
                {
                    int pl = group == 1u && !r->album && song_playing(d, r->key); /* I690 */
                    draw_text(d, rect_of(mn.x + mg, y, mn.w - 2 * mg, lpx(d, SEARCH_ROW_H)), t, pl, 14, pl ? ARGB(d->theme.token[SEL_GUI_TOKEN_ACCENT]) : text);
                }
                if (group == 1u && !r->album && r->key != NULL) {
                    song_row_set_list(d, song_row_reg(d, rect_of(mn.x + mg, y, mn.w - 2 * mg, lpx(d, SEARCH_ROW_H)), r->key, 0, 0, 0), SEL_UIM_LIST_VIEW, (uint32_t)d->page, v->version, v->window_start + k);
                }
                y += lpx(d, SEARCH_ROW_H);
                drawn_all++;
                shown += group == 1u;
            }
        }
        d->search_drawn = drawn_all;
    } else if (!d->search_on) { /* no query yet: a muted hint under the chips */
        draw_empty_state(d, mn, y, "Type to search songs, albums and artists");
    }
    d->search_draws++;
    d->search_rows = shown;
    sel_gui_clip_pop(d->canvas);
    sel_gui_dirty_add(d->canvas, mn);
}

static void draw_home_content(desk* d) {
    const sel_state* st = sel_engine_state(d->engine);
    const char** akeys = NULL;
    uint32_t akn = 0;
    d->sm_n = 0;
    np_title_reg(d);
    hover_prune(d, region_rect(d, "main")); /* the main region lists its controls afresh: a page change leaves none behind */
    if (d->page == PG_BROWSE) {
        draw_browse(d);
        return;
    }
    if ((d->search_on && !(d->pl_page && d->pls_id != 0u)) || d->page == PG_SEARCH) { /* I652a: an open playlist keeps its page, its search shows inside it; the Search page always draws its own field and facet chips, also before a query */
        draw_search(d);
        return;
    }
    if (d->queue_page) {
        draw_queue(d);
        return;
    }
    if (d->pl_page) {
        draw_playlists(d);
        return;
    }
    if (d->si_page) {
        draw_songinfo(d);
        return;
    }
    if (d->tr_page) {
        draw_trackerpage(d);
        return;
    }
    if (d->viz_page) {
        draw_vizpage(d);
        return;
    }
    if (d->np_page) {
        draw_nowplaypage(d);
        return;
    }
    sel_gui_rect mn = draw_panel(d, "main");
    int32_t pad = lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_TILE_PAD]);
    int32_t hv = 65536; /* a hovered tile is lit at once (I351); no tile fades out */
    uint32_t i;
    char name[256];
    /* The page picks the content. The engine state lags a page change (COMPOSERS_HIDE is read on the engine's next pump), so
     * a Library page drawn on st->composers alone stays over the page just entered (I274). */
    if (d->page == PG_FOLDERS && st->composers != NULL) {
        draw_library(d);
        return;
    }
    if (d->songs) {
        draw_songs(d);
        draw_songs_targets(d);
        return;
    }
    if (d->open_tile > 0) {
        draw_detail(d);
        return;
    }
    {
        const char** keys = (const char**)malloc((size_t)(st->queue_count + SHELF_MAX * SHELF_WINDOW + 1u) * sizeof *keys);
        akn = keys != NULL ? art_keys(st, keys, st->queue_count + SHELF_MAX * SHELF_WINDOW) : 0u;
        load_tile_art(d, keys, akn);
        akeys = keys;
    }
    sel_gui_clip_push(d->canvas, mn);
    if (home_shelf_mode(st)) {
        sel_gui_rect shifted = mn;
        shifted.y -= lpx(d, d->home_y_q16 >> 16); /* the header scrolls away with the shelves */
        draw_home_header(d, shifted, 0);
        tile_palette(d, akeys, akn);
        draw_shelves(d, mn);
        free((void*)akeys);
        sel_gui_clip_pop(d->canvas);
        sel_gui_dirty_add(d->canvas, mn);
        return;
    }
    if (home_empty(st)) {
        sel_gui_rect b = home_empty_button(d);
        draw_home_header(d, mn, 0);
        draw_text(d, rect_of(mn.x + lpx(d, HOME_MARGIN), mn.y + lpx(d, HOME_SECTION_Y), mn.w - 2 * lpx(d, HOME_MARGIN), lpx(d, HOME_SECTION_H)),
                  "Nothing played yet. Songs you play show up here.", 0, 14, ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT_MUTED]));
        draw_button(d, b, ARGB(d->theme.token[SEL_GUI_TOKEN_ACCENT]), "Browse files", 13, ARGB(d->theme.token[SEL_GUI_TOKEN_ACCENT_TEXT]));
        free((void*)akeys);
        sel_gui_clip_pop(d->canvas);
        sel_gui_dirty_add(d->canvas, mn);
        return;
    }
    draw_home_header(d, mn, 1);
    tile_palette(d, akeys, akn);
    free((void*)akeys);
    for (i = 0; i < st->queue_count; i++) {
        sel_gui_rect t = tile_rect(d, i);
        sel_gui_rect ar = rect_of(t.x + pad, t.y + pad, t.w - 2 * pad, t.w - 2 * pad);
        const uint32_t* art;
        char format[16];
        int hovered = d->hover_tile > 0 && (uint32_t)d->hover_tile - 1u == i;
        uint32_t panel = ARGB(d->theme.token[SEL_GUI_TOKEN_PANEL]);
        uint32_t pc_slot = 0; /* I691b: family + 1 of the platform cover this tile drew, 0 = real art */
        queue_title(st, i, name, sizeof name);
        queue_format(st, i, format, sizeof format);
        sel_gui_fill_rect(d->canvas, t, panel); /* the corners blend with the panel, not with the last frame's card */
        sel_gui_round_rect(d->canvas, t, lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_RADIUS_LARGE]),
                           hovered ? desk_mix(panel, ARGB(d->theme.token[SEL_GUI_TOKEN_SURFACE]), (uint32_t)hv) : panel);
        d->tile_cards++;
        d->home_parts |= HP_TILE_CARD;
        {
            uint32_t t_art = sel_gui_prof_begin(d->canvas);
            art = art_tile(d->art, st->queue_text + st->queue[i].path_offset);
            sel_gui_prof_end(d->canvas, SEL_GUI_PROF_ART, t_art);
        }
        if (d->shadow_ready) {
            sel_gui_shadow_draw_round(d->canvas, &d->shadow, ar, lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_RADIUS_LARGE]), lpx(d, HOME_COVER_SHADOW_LIFT));
        }
        if (art != NULL) {
            sel_gui_set_region(d->canvas, SEL_GUI_RGN_TILE);
            sel_gui_blit_image_round(d->canvas, ar, art, DESK_ART_TILE_EDGE, DESK_ART_TILE_EDGE, DESK_ART_TILE_EDGE, lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_RADIUS_LARGE]));
            sel_gui_set_region(d->canvas, SEL_GUI_RGN_UI);
        } else {
            /* No cover: the platform cover of the song's family (gui_genart.h). */
            song_platform_cover(d, ar, st->queue_text + st->queue[i].path_offset, name, &pc_slot);
        }
        if (i == 0u) {
            d->pc_tile0 = pc_slot;
            d->art_tile_hash = rect_hash(d->canvas, ar);
        }
        draw_text(d, rect_of(ar.x, ar.y + ar.h + lpx(d, HOME_TILE_TEXT_GAP), ar.w, lpx(d, HOME_TITLE_H)), name, 1, 14,
                  ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT]));
        draw_text(d, rect_of(ar.x, ar.y + ar.h + lpx(d, HOME_TILE_TEXT_GAP + HOME_TITLE_H), ar.w, lpx(d, HOME_META_H)), format, 0,
                  13, ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT_MUTED]));
        d->home_parts |= HP_ARTIST_LINE;
        song_row_reg(d, t, st->queue_text + st->queue[i].path_offset, st->queue[i].id, 0, 1); /* I372: the queue tile is a song */
        if (hovered && hv > 0) { /* the play affordance */
            int32_t ps = lpx(d, HOME_PLAY_SIZE), in = lpx(d, HOME_PLAY_INSET);
            sel_gui_rect pb = rect_of(ar.x + ar.w - in - ps, ar.y + ar.h - in - ps, ps, ps);
            sel_gui_round_rect(d->canvas, pb, ps / 2, ARGB(d->theme.token[SEL_GUI_TOKEN_ACCENT]));
            sel_gui_icon_draw(d->canvas, &d->icons, SEL_GUI_ICON_PLAY, rect_of(pb.x + ps / 4, pb.y + ps / 4, ps / 2, ps / 2),
                              ps / 2, ARGB(d->theme.token[SEL_GUI_TOKEN_ACCENT_TEXT]));
            d->home_parts |= HP_PLAY_HOVER;
        }
    }
    sel_gui_clip_pop(d->canvas);
    sel_gui_dirty_add(d->canvas, mn);
}

/* History arrow k (0 back, 1 forward) at the top right of the main pane; page controls on that row end left of it. */
static sel_gui_rect nav_arrow_rect(const desk* d, sel_gui_rect mn, int32_t k) {
    int32_t bs = lpx(d, 28), gap = lpx(d, 8);
    /* I599: vertically centred on the search field row (the field is at SEARCH_FIELD_Y, SEARCH_FIELD_H high) */
    return rect_of(mn.x + mn.w - lpx(d, HOME_MARGIN) - 2 * bs - gap + k * (bs + gap),
                   mn.y + lpx(d, SEARCH_FIELD_Y) + (lpx(d, SEARCH_FIELD_H) - bs) / 2, bs, bs);
}

/* The arrows are drawn in the full layout on a main pane of at least 400 px; page content on their row ends left of them. */
static int nav_arrows_shown(const desk* d, sel_gui_rect mn) {
    return d->uim.layout == SEL_UIM_LAYOUT_FULL && mn.w >= lpx(d, 400);
}

/* The history arrows (I266) at the top right of the main pane, after Spotify's back and forward buttons: a round
 * button each, the text colour while the history can move that way and the muted colour when it cannot. */
static void draw_nav_arrows(desk* d, sel_gui_rect mn) {
    int32_t bs = lpx(d, 28), k;
    int can[2] = {sel_route_hist_can(&d->rh, -1), sel_route_hist_can(&d->rh, 1)};
    sel_gui_rect* slot[2] = {&d->nav_back, &d->nav_fwd};
    if (!nav_arrows_shown(d, mn)) {
        d->nav_back = rect_of(0, 0, 0, 0);
        d->nav_fwd = rect_of(0, 0, 0, 0);
        return;
    }
    for (k = 0; k < 2; k++) {
        sel_gui_rect b = nav_arrow_rect(d, mn, k);
        uint32_t ink = ARGB(d->theme.token[can[k] ? SEL_GUI_TOKEN_TEXT : SEL_GUI_TOKEN_TEXT_MUTED]);
        int32_t ic = lpx(d, 18);
        *slot[k] = b;
        hover_reg(d, b);
        sel_gui_round_rect(d->canvas, b, bs / 2, hover_fill_of(d, b, ARGB(d->theme.token[SEL_GUI_TOKEN_SURFACE])));
        /* I599: the baked Lucide chevron, centred in the round button; the icon path scales to tall pixels, so both arms
         * have the same length (two hand-drawn strokes did not: sel_gui_line divides each y by ydv). */
        sel_gui_icon_draw(d->canvas, &d->icons, k == 0 ? SEL_GUI_ICON_CHEVRON_LEFT : SEL_GUI_ICON_CHEVRON_RIGHT,
                          rect_of(b.x + (bs - ic) / 2, b.y + (bs - ic) / 2, ic, ic), ic, ink);
        d->nav_icon_draws++;
        sel_gui_dirty_add(d->canvas, b);
    }
}

/* INDEX8 only (DUI-P6-4): the 64 tile-row entries (64-127) come from a median cut over 8x8 thumbnails of the visible
 * cover arts, so the row of covers shares one palette. Rewritten only when the colours change. */
static void tile_palette_raw(desk* d, const char* const* keys, uint32_t nk);
/* I395: the whole tile palette step (art lookup, 8x8 scale, quantise, palette rebuild) is one profiled class. */
static void tile_palette(desk* d, const char* const* keys, uint32_t nk) {
    uint32_t t0 = sel_gui_prof_begin(d->canvas);
    tile_palette_raw(d, keys, nk);
    sel_gui_prof_end(d->canvas, SEL_GUI_PROF_QUANT, t0);
}

static void tile_palette_raw(desk* d, const char* const* keys, uint32_t nk) {
    uint32_t px[SEL_GUI_QUANT_MAX], thumb[64], q[64], rgb[64], i, n = 0, m;
    static uint32_t last[64];
    if (d->canvas->format != SEL_GUI_INDEX8) {
        return;
    }
    for (i = 0; i < nk && n + 64u <= SEL_GUI_QUANT_MAX; i++) {
        const uint32_t* art = art_tile(d->art, keys[i]);
        if (art != NULL) {
            sel_gui_image_scale(art, DESK_ART_TILE_EDGE, DESK_ART_TILE_EDGE, thumb, 8, 8);
            memcpy(px + n, thumb, sizeof thumb);
            n += 64u;
        }
    }
    if (n == 0u) {
        return;
    }
    m = sel_gui_quantise(px, n, q, 64u);
    d->quant_calls++;
    for (i = 0; i < 64u; i++) {
        rgb[i] = q[i < m ? i : m - 1u];
    }
    if (memcmp(rgb, last, sizeof rgb) != 0) {
        memcpy(last, rgb, sizeof rgb);
        sel_gui_set_palette(d->canvas, rgb, 64u, 64u);
    }
}

/* The main region with the running tweens on top: the hover fade of one tile (a light veil, full strength at rest)
 * and the view cross-fade (the new view fades in from the surface colour). At rest both are the plain view. */
/* I615: the window-wide top bar: the round Home button, then the pill search field (magnifier, the typed query or the hint)
 * with the Browse button behind a thin divider at its right end. Rects come from the layout; colours from the tokens. */
static void draw_topbar(desk* d) {
    sel_gui_rect bar, hm, sr, br;
    char hint[64];
    int32_t ic = lpx(d, 20), dh, dw;
    if (!topbar_shown(d)) {
        return;
    }
    bar = rect_of(lpx(d, d->layout.topbar.x), lpx(d, d->layout.topbar.y), lpx(d, (int32_t)d->layout.topbar.w), lpx(d, (int32_t)d->layout.topbar.h));
    hm = topbar_part(d, TOPBAR_HOME);
    sr = topbar_part(d, TOPBAR_FIELD);
    br = topbar_part(d, TOPBAR_BROWSE);
    dh = br.h / 2;
    dw = lpx(d, 1) > 0 ? lpx(d, 1) : 1;
    sel_gui_fill_rect(d->canvas, bar, ARGB(d->theme.token[SEL_GUI_TOKEN_FRAME]));
    sel_gui_round_rect(d->canvas, hm, hm.h / 2, ARGB(d->theme.token[d->page == PG_HOME ? SEL_GUI_TOKEN_SURFACE_HOVER : SEL_GUI_TOKEN_SURFACE_RAISED]));
    sel_gui_icon_draw(d->canvas, &d->icons, SEL_GUI_ICON_HOUSE, rect_of(hm.x + (hm.w - ic) / 2, hm.y + (hm.h - ic) / 2, ic, ic), ic,
                      ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT]));
    sel_gui_round_rect(d->canvas, sr, sr.h / 2, ARGB(d->theme.token[SEL_GUI_TOKEN_SURFACE]));
    sel_gui_icon_draw(d->canvas, &d->icons, SEL_GUI_ICON_SEARCH, rect_of(sr.x + lpx(d, 14), sr.y + (sr.h - ic) / 2, ic, ic), ic,
                      ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT_MUTED]));
    search_hint(d, hint, sizeof hint, home_chip_name, HOME_CHIPS);
    desk_draw_query(d, sr, hint);
    sel_gui_fill_rect(d->canvas, rect_of(br.x, br.y + (br.h - dh) / 2, dw, dh), ARGB(d->theme.token[SEL_GUI_TOKEN_DIVIDER]));
    sel_gui_icon_draw(d->canvas, &d->icons, SEL_GUI_ICON_LAYOUT_GRID, rect_of(br.x + (br.w - ic) / 2, br.y + (br.h - ic) / 2, ic, ic), ic,
                      ARGB(d->theme.token[d->page == PG_FOLDERS ? SEL_GUI_TOKEN_TEXT : SEL_GUI_TOKEN_TEXT_MUTED]));
    {
        sel_gui_rect tg = np_toggle_rect(d);
        if (tg.w > 0) { /* I619: Hide / Show Now Playing, pointing at the side the column moves to */
            hover_reg(d, tg);
            sel_gui_round_rect(d->canvas, tg, tg.h / 2, hover_fill_of(d, tg, ARGB(d->theme.token[SEL_GUI_TOKEN_SURFACE_RAISED])));
            sel_gui_icon_draw(d->canvas, &d->icons, d->detail_hidden ? SEL_GUI_ICON_CHEVRON_LEFT : SEL_GUI_ICON_CHEVRON_RIGHT,
                              rect_of(tg.x + (tg.w - ic) / 2, tg.y + (tg.h - ic) / 2, ic, ic), ic, ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT]));
        }
    }
    sel_gui_dirty_add(d->canvas, bar);
    d->topbar_draws++;
}

/* I615: a click on the top bar: Home opens the Home page, Browse the Browse tiles page (I615c), the field takes the keyboard on the
 * Search page. 1 when the click was the bar's. */
static int topbar_click(desk* d, int32_t x, int32_t y) {
    if (!topbar_shown(d)) {
        return 0;
    }
    if (np_toggle_rect(d).w > 0 && in_rect(np_toggle_rect(d), x, y)) { /* I619: hide or show the Now Playing column; the main region takes its width */
        d->np_toggle_clicks++;
        d->detail_hidden = d->detail_hidden ? 0u : 1u;
        desk_relayout(d);
        d->repaint = 1;
        d->uim.dirty |= SEL_UIM_DIRTY_ALL;
        return 1;
    }
    if (in_rect(topbar_part(d, TOPBAR_HOME), x, y)) {
        d->topbar_home_clicks++;
        desk_route(d, SEL_ROUTE_DOOR_HOME, 0u, 0u, "", "", "");
        return 1;
    }
    if (in_rect(topbar_part(d, TOPBAR_BROWSE), x, y)) {
        d->topbar_browse_clicks++;
        desk_route_door(d, PG_BROWSE);
        return 1;
    }
    if (in_rect(topbar_part(d, TOPBAR_FIELD), x, y)) {
        if (d->page == PG_SEARCH) { /* a click in the field focuses it */
            d->focus = (int32_t)d->layout.count;
            d->uim.dirty |= SEL_UIM_DIRTY_ALL;
            d->repaint = 1;
        } else { /* the field is the Search page's field: the click goes there with the query kept */
            d->search_keep = 1;
            desk_route(d, SEL_ROUTE_DOOR_SEARCH, 0u, 0u, "", "", "");
            d->search_keep = 0;
        }
        return 1;
    }
    return 0;
}

static void draw_home(desk* d) {
    sel_gui_rect mn = region_rect(d, "main"), pn = panel_rect(d, "main");
    int32_t view = sel_gui_tween_value(&d->tl, ANIM_KEY_VIEW, 65536);
    draw_home_content(d);
    select_sync(d);
    desk_draw_window_overlays(d);
    draw_nav_arrows(d, pn); /* I599: the panel rect, the one the search field row is laid out in */
    if (view < 65536) {
        sel_gui_scrim(d->canvas, rect_of(mn.x, songs_top(d), mn.w, mn.y + mn.h - songs_top(d)),
                      ARGB(d->theme.token[SEL_GUI_TOKEN_SURFACE]) & 0xFFFFFFu,
                      255u - (uint32_t)(((int64_t)view * 255) >> 16));
    }
}

/* Enters a view (DUI-P3-4): the forward history is dropped and the view is appended; the oldest entry falls off. */
static void view_goto(desk* d, int32_t tile) {
    if (d->hist_n == 0u) {
        d->hist[0] = d->open_tile;
        d->hist_n = 1u;
        d->hist_pos = 0u;
    }
    d->hist_n = d->hist_pos + 1u;
    if (d->hist_n == sizeof d->hist / sizeof d->hist[0]) {
        memmove(d->hist, d->hist + 1, (d->hist_n - 1u) * sizeof d->hist[0]);
        d->hist_n--;
        d->hist_pos--;
    }
    d->hist[d->hist_n++] = tile;
    d->hist_pos = d->hist_n - 1u;
    d->open_tile = tile;
}

/* Swipe (DUI-P3-4): right is Back, left is Forward in the view history. Returns 1 when the view changed. */
static int view_swipe(desk* d, int32_t dir) {
    if (d->hist_n == 0u) {
        return 0;
    }
    if (dir > 0 && d->hist_pos > 0u) {
        d->hist_pos--;
    } else if (dir < 0 && d->hist_pos + 1u < d->hist_n) {
        d->hist_pos++;
    } else {
        return 0;
    }
    d->open_tile = d->hist[d->hist_pos];
    return 1;
}

/* The page flags (I262): the one place that turns a page into the main region's mode. --view and the doors both
 * call it, so a page entered by either is the same page. */
static void desk_page_flags(desk* d, desk_page pg) {
    d->home = 1;
    d->page = pg;
    d->songs = pg == PG_SONGS;
    d->row_focus = -1;
    d->col_focus = -1;
    d->q_focus = -1;
    d->lib_focus = -1;
    d->kb_list = 0;
    d->queue_page = pg == PG_QUEUE;
    d->pl_page = pg == PG_PLAYLISTS || pg == PG_COLLECTION;
    d->pls_id = 0u;
    d->add_id = 0u; /* I652b: leaving or entering a page ends the Library's add mode */
    d->si_page = pg == PG_SONGINFO;
    d->si_y_q16 = 0; /* I298: the info list starts at the top on every entry */
    d->viz_page = pg == PG_VIZ;
    d->tr_page = pg == PG_TRACKER;
    d->tr_asked = 0u;
    d->np_page = pg == PG_NOWPLAYING;
    d->search_on = 0;
    if (pg != PG_SEARCH) {
        d->browse_filter = 0;
    }
    d->open_tile = 0;
    d->hist_n = 0u; /* the view history is per page */
}

/* The page of a --view name, PG_COUNT when the name is not a page. library opens Browse Folders (I678i). */
static desk_page desk_page_named(desk* d, const char* name) {
    uint32_t i;
    if (strcmp(name, "library") == 0) { /* I678i: the Library page is retired; its old --view name opens Browse Folders (D3) */
        d->library_remaps++;
        name = "folders";
    }
    for (i = 0; i < PG_COUNT; i++) {
        if (strcmp(g_page_name[i], name) == 0) {
            return (desk_page)i;
        }
    }
    return PG_COUNT;
}

/* The "/" key and the Search door: the search field takes the keyboard with an empty query. */
static void desk_search_focus(desk* d, int keep_query) {
    d->focus = (int32_t)d->layout.count;
    if (!keep_query) {
        d->qlen = 0u;
        d->query[0] = '\0';
    }
    d->page = PG_SEARCH;
    d->repaint = 1;
    d->uim.dirty |= SEL_UIM_DIRTY_ALL;
}

/* Appends a page to the history (I266): the forward pages are dropped, the oldest falls off at the bound, a page equal
 * to the current one is not repeated. The first entry seeds the history with the page the window was on. */
static uint32_t route_page_of(desk_page pg) { /* the route table's page ids; the pages it does not name keep 100 + the desk page */
    return pg == PG_HOME ? (uint32_t)SEL_ROUTE_PAGE_HOME
           : pg == PG_SEARCH ? (uint32_t)SEL_ROUTE_PAGE_SEARCH
           : pg == PG_SHELF ? (uint32_t)SEL_ROUTE_PAGE_SHELF
           : pg == PG_COLLECTION ? (uint32_t)SEL_ROUTE_PAGE_COLLECTION
                                 : 100u + (uint32_t)pg;
}

static desk_page desk_page_of(uint32_t rp) {
    return rp == (uint32_t)SEL_ROUTE_PAGE_HOME ? PG_HOME
           : rp == (uint32_t)SEL_ROUTE_PAGE_SEARCH ? PG_SEARCH
           : rp == (uint32_t)SEL_ROUTE_PAGE_SHELF ? PG_SHELF
           : rp == (uint32_t)SEL_ROUTE_PAGE_COLLECTION ? PG_COLLECTION
           : rp >= 100u && rp - 100u < (uint32_t)PG_COUNT ? (desk_page)(rp - 100u)
                                                            : PG_HOME;
}

/* The scroll the current page keeps while it is left (Home keeps its own across visits, so only a collection page needs it). */
static int32_t desk_rt_scroll(const desk* d) { return d->page == PG_COLLECTION ? (int32_t)d->pls_scroll : 0; }

/* Appends an entry to the history (I266, I678h): the first one seeds it with the page the window is on, the leaving page keeps its
 * scroll, the forward entries are dropped, the oldest falls off at the bound, an entry equal to the current one is not repeated. */
static void desk_rt_push(desk* d, const sel_route_entry* to) {
    sel_route_entry cur;
    memset(&cur, 0, sizeof cur);
    cur.page = route_page_of(d->page);
    sel_route_hist_seed(&d->rh, &cur);
    sel_route_hist_scroll(&d->rh, desk_rt_scroll(d));
    sel_route_hist_push(&d->rh, to);
}

/* The engine requests that fill a page: the view source and window, the Library screens, the kept search query. Entering
 * a page and a finished scan both end here, so the list of requests exists once (I296). */
static void desk_page_load(desk* d, desk_page pg) {
    if (pg == PG_HOME || pg == PG_SHELF) { /* the Shelf page reads the Home source too */
        desk_send(d, "VIEW_SOURCE home");
        desk_send(d, "VIEW_WINDOW 32");
    } else if (pg == PG_FOLDERS) {
        d->lib_q16 = 0;
        desk_send(d, "VIEW_SHOW browse");
        const sel_state* st = sel_engine_state(d->engine);
        desk_send(d, "COMPOSERS_SHOW");
        if (st->view != NULL && (st->view->style == (uint32_t)SEL_VIEW_STYLE_GRID || st->view->style == (uint32_t)SEL_VIEW_STYLE_COVERFLOW)) {
            /* VIEW_SHOW browse took the source: a kept Grid or Cover Flow lists the library again */
            lib_grid_source(d);
        }
        desk_send(d, "STATS_SHOW");
        if (d->lib_mode != 2u) { /* I678i: Browse Folders opens on Tree (or stays on Columns); the retired tabs are never a start mode */
            d->lib_mode = 0xFFu;
            lib_set_mode(d, 1u);
        }
    } else if (pg == PG_BROWSE) { /* I615c: the catalog the tiles are built from: the formats (the grouped library view), the composers and the sources */
        desk_send(d, "FORMATS_SHOW");
        desk_send(d, "COMPOSERS_SHOW");
        d->src_wait = 1;
        desk_send(d, "SOURCES");
    } else if (pg == PG_PLAYLISTS || pg == PG_COLLECTION || pg == PG_SONGS) { /* the folders source holds the saved playlists */
        desk_send(d, "VIEW_SOURCE folders");
        desk_send(d, "VIEW_WINDOW 64");
        d->folder_opens++;
    } else if (pg == PG_SEARCH) {
        if (d->qlen > 0u) { /* the kept query shows its results again */
            d->search_pending = 1;
            d->search_since = d->now_ms;
        }
    }
}

/* Enters a page (I262) with the engine verbs the page reads: the view source and window, and the Library screens
 * opened and closed. Every way into a page ends here (doors, keys, --view, Back and Forward), so the history is kept
 * here. Does not draw. */
static void desk_page_apply(desk* d, desk_page pg) {
    const sel_state* st = sel_engine_state(d->engine);
    desk_page from = d->page;
    if (!d->pg_walk) { /* a page-only entry; desk_route and the walk enter with pg_walk set, having pushed their own */
        sel_route_entry e;
        memset(&e, 0, sizeof e);
        e.page = route_page_of(pg);
        e.id = pg == PG_SHELF ? d->shelf_open : 0u;
        desk_rt_push(d, &e);
    }
    if (pg != PG_FOLDERS) {
        d->stats_open = 0;
        if (st->composers != NULL) {
            desk_send(d, "COMPOSERS_HIDE");
        }
        if (st->stats != NULL) {
            desk_send(d, "STATS_HIDE");
        }
    }
    desk_page_flags(d, pg);
    if (from == PG_SHELF && pg == PG_HOME) {
        d->home_y_q16 = d->shelf_back_y_q16; /* I617: Back lands on Home where it was left */
    }
    d->focus = -1;
    if (pg == PG_SEARCH) {
        desk_search_focus(d, d->pg_walk || d->search_keep);
    }
    desk_page_load(d, pg);
    d->repaint = 1;
    d->uim.dirty |= SEL_UIM_DIRTY_ALL;
}

/* I617: opens shelf s (0-based) of Home as the Shelf page, through the page history. */
static int shelf_open_page(desk* d, uint32_t s) {
    if (d->page == PG_HOME) {
        d->shelf_back_y_q16 = d->home_y_q16;
    }
    d->home_y_q16 = 0;
    d->shelf_page_opens++;
    return desk_route(d, SEL_ROUTE_SHOW_ALL, 0u, s + 1u, "", "", "");
}

static int desk_page_goto(desk* d, desk_page pg) {
    desk_page_apply(d, pg);
    return desk_present(d);
}

/* I617: Enter with the engine cursor on the heading row of a shelf opens that shelf's page. Returns 1 when it did. */
static int shelf_header_enter(desk* d) {
    const sel_state* st = sel_engine_state(d->engine);
    const sel_view_state* sv = st->view;
    shelf sh[SHELF_MAX];
    uint32_t s, n, at;
    if (sv == NULL || sv->cursor < sv->window_start) {
        return 0;
    }
    at = sv->cursor - sv->window_start;
    n = home_shelves(st, sh);
    for (s = 0; s < n; s++) {
        if (sh[s].head == at && sh[s].tiles > 0u) {
            d->kb_activates++;
            d->show_all_clicks++;
            shelf_open_page(d, s);
            return 1;
        }
    }
    return 0;
}

/* Back (dir -1) and Forward (+1) in the page history. Returns 1 when the picture changed. */
/* Fills the page an entry names, after desk_page_goto opened it: a playlist or a kind page asks the engine for its items. A bare entry
 * (a door page) has nothing to fill. */
static void desk_route_enter(desk* d, const sel_route_entry* e, int restore) {
    if (e->page != (uint32_t)SEL_ROUTE_PAGE_COLLECTION) {
        return;
    }
    if (e->kind == (uint32_t)SEL_LIB_ITEM_PLAYLIST) {
        pls_open(d, e->id, e->name);
    } else if (e->kind != 0u || e->key[0] != '\0' || e->name[0] != '\0') {
        col_kind_open(d, e->kind, e->key, e->name, e->sub, restore);
    }
}

static int desk_page_walk(desk* d, int dir) {
    int moved;
    sel_route_entry e;
    const sel_route_entry* w;
    if (!sel_route_hist_can(&d->rh, dir)) {
        return 0;
    }
    sel_route_hist_scroll(&d->rh, desk_rt_scroll(d));
    w = sel_route_hist_walk(&d->rh, dir);
    e = *w;
    d->pg_walk = 1;
    if (e.page == (uint32_t)SEL_ROUTE_PAGE_SHELF) {
        d->shelf_open = e.id;
    }
    moved = desk_page_goto(d, desk_page_of(e.page));
    desk_route_enter(d, &e, 1);
    if (e.page == (uint32_t)SEL_ROUTE_PAGE_COLLECTION) {
        d->pls_scroll = (uint32_t)e.scroll; /* the page is where it was left */
    }
    d->pg_walk = 0;
    return moved;
}

/* I678h: the one entry point of a click that opens a page (design doc section 9). The route table says which page; the history takes
 * the entry (the leaving page keeps its scroll); the page opens through desk_page_goto and the entry fills it. Returns 1 when a page
 * opened. A target that opens nothing (the "Your Library" title, a chip) is still counted. */
static int desk_route(desk* d, sel_route_target target, uint32_t item_kind, uint32_t id, const char* key, const char* name, const char* sub) {
    sel_route_entry e;
    if (!sel_route_go(&d->rh, target, item_kind, id, key, name, sub, &e)) {
        return 0;
    }
    desk_rt_push(d, &e);
    d->pg_walk = 1;
    if (e.page == (uint32_t)SEL_ROUTE_PAGE_SHELF) {
        d->shelf_open = e.id;
    }
    desk_page_goto(d, desk_page_of(e.page));
    desk_route_enter(d, &e, 0);
    d->pg_walk = 0;
    return 1;
}

/* A door of any page goes through the route table (I678h2): Home and Search have their own targets, every other page is a door page. */
static int desk_route_door(desk* d, desk_page pg) {
    if (pg == PG_HOME) {
        return desk_route(d, SEL_ROUTE_DOOR_HOME, 0u, 0u, "", "", "");
    }
    if (pg == PG_SEARCH) {
        return desk_route(d, SEL_ROUTE_DOOR_SEARCH, 0u, 0u, "", "", "");
    }
    return desk_route(d, SEL_ROUTE_DOOR_PAGE, (uint32_t)pg, 0u, "", "", "");
}

/* A click at (x, y): opens the tile under it. Returns 1 when a tile opened. */
static int desk_click(desk* d, int32_t x, int32_t y) {
    const sel_state* st = sel_engine_state(d->engine);
    sel_gui_rect mn = region_rect(d, "main");
    uint32_t i;
    if (d->page == PG_BROWSE && in_rect(mn, x, y)) {
        return browse_click(d, x, y);
    }
    if (d->page == PG_SEARCH && in_rect(mn, x, y)) { /* a facet chip filters the drawn groups; without results it is kept for the next ones */
        for (i = 0; i < SEARCH_FACETS; i++) {
            if (in_rect(search_facet_rect(d, panel_rect(d, "main"), i), x, y)) {
                int changed = d->search_facet != i;
                d->search_facet = i;
                d->repaint = 1;
                d->uim.dirty |= SEL_UIM_DIRTY_ALL;
                return changed;
            }
        }
    }
    if (d->page == PG_HOME && d->open_tile == 0 && !d->songs && st->composers == NULL && in_rect(mn, x, y)) {
        for (i = 0; i < HOME_CHIPS; i++) { /* the Home header chips */
            if (in_rect(home_chip_rect(d, panel_rect(d, "main"), i), x, y)) {
                int moved;
                if (i == 0u) {
                    d->search_facet = i;
                    d->search_keep = 1;
                }
                moved = desk_route_door(d, i == 0u ? PG_SEARCH : i == 1u ? PG_SONGS : i == 2u ? PG_FOLDERS : PG_PLAYLISTS); /* I678h3: a chip is a door, so Back returns to Home */
                d->search_keep = 0;
                return moved;
            }
        }
    }
    if (d->page == PG_SEARCH && in_rect(mn, x, y)) { /* a click in the field focuses it; elsewhere in main drops focus */
        int32_t want = in_rect(search_field_rect(d, panel_rect(d, "main")), x, y) ? (int32_t)d->layout.count : -1;
        if (want >= 0 || d->focus == (int32_t)d->layout.count) {
            int changed = d->focus != want;
            d->focus = want;
            d->uim.dirty |= SEL_UIM_DIRTY_ALL;
            d->repaint = 1;
            return changed || want >= 0;
        }
    }
    if (d->page == PG_HOME && d->open_tile == 0 && !d->songs && st->composers == NULL && in_rect(search_field_rect(d, panel_rect(d, "main")), x, y)) {
        int moved; /* the Home search field is the Search page's field: the click goes there with the query kept */
        d->search_keep = 1;
        moved = desk_page_goto(d, PG_SEARCH);
        d->search_keep = 0;
        return moved;
    }
    if (d->np_draws > 0u && in_rect(d->np_like, x, y)) { /* Now Playing: like the playing song, or set its rating */
        return desk_send(d, "LIKE");
    }
    for (i = 0; d->np_draws > 0u && i < 5u; i++) {
        if (in_rect(d->np_rate[i], x, y)) {
            char line[16];
            /* The mark of the stored rating clicked again takes the rating back (I306). */
            sel_uim_rate_line(d->np_rating > 0 ? (uint32_t)d->np_rating : 0u, i + 1u, line, sizeof line);
            return desk_send(d, line);
        }
    }
    if (!d->home || d->songs || d->open_tile > 0 || x < mn.x || y < mn.y || x >= mn.x + mn.w || y >= mn.y + mn.h) {
        return 0;
    }
    if (home_empty(st)) { /* the empty state's only action */
        if (in_rect(home_empty_button(d), x, y)) {
            desk_send(d, "VIEW_SOURCE browse");
            return 1;
        }
        return 0;
    }
    if (home_shelf_mode(st)) { /* a shelf tile: the engine plays the song or opens the album */
        uint32_t s, k;
        for (s = 0; s < 8u && !shelf_page_on(d); s++) { /* I617: the Show all link of a shelf header */
            if (d->show_all_rect[s].w > 0 && in_rect(d->show_all_rect[s], x, y)) {
                d->show_all_clicks++;
                return shelf_open_page(d, s);
            }
        }
        if (shelf_nav_click(d, x, y)) { /* I607: a previous/next button sits over the covers: it wins over the tile */
            draw_home(d);
            return 1;
        }
        if (quick_hit(d, x, y, &k)) {
            quick_activate(d, k);
            draw_home(d);
            return 1;
        }
        if (!shelf_hit(d, x, y, &s, &k)) {
            return 0;
        }
        shelf_activate(d, s, k);
        draw_home(d);
        return 1;
    }
    for (i = 0; i < st->queue_count; i++) {
        sel_gui_rect t = tile_rect(d, i);
        if (x >= t.x && x < t.x + t.w && y >= t.y && y < t.y + t.h) {
            view_goto(d, (int32_t)i + 1);
            d->view_opens++;
            desk_tween(d, ANIM_KEY_VIEW, SEL_GUI_ANIM_NORMAL_MS, SEL_GUI_EASE_OUT);
            draw_home(d);
            return 1;
        }
    }
    return 0;
}

/* An input event changed the picture: flush it as one frame. */
static int desk_event_frame(desk* d, int changed) {
    if (changed) {
        d->dirty_pixels = desk_flush(d);
        d->dirty_total += d->dirty_pixels;
        d->frames++;
    }
    return changed;
}

/* A frame is drawn only when a dirty bit is set or a tween runs (plan 2.9); this is the idle block. */
static int desk_should_draw(const desk* d, int animated) {
    return d->uim.dirty != 0u || animated;
}

/* ---- Tooltips (DUI-P3-6): a pointer resting 600 ms on a transport button shows the action's tooltip, with its key in
 * brackets; moving off or clicking hides it. The buttons are the actions named here, in this order, centred in the
 * player region ("play" sits where the placeholder play box is drawn). ---- */
enum { TIP_DELAY_MS = 600, TIP_BTN = 32, TIP_H = 24 };
/* The transport row, left to right (DUI-P4-1): previous subsong, shuffle, previous, play or pause, next, repeat, next
 * subsong. The play slot sits in the middle where the placeholder box was drawn. */

static const sel_uim_action* desk_action_by_id(const char* id) {
    uint32_t i, n = sel_uim_action_count();
    if (id == NULL) {
        return NULL;
    }
    for (i = 0; i < n; i++) {
        const sel_uim_action* a = sel_uim_action_at(i);
        if (strcmp(a->id, id) == 0) {
            return a;
        }
    }
    return NULL;
}

/* The action id of transport slot i from the engine state, NULL when the slot is not shown (the subsong buttons need
 * a song with more than one subsong). Shuffle and repeat name the action a click fires: the next state. */
static const char* desk_slot_action(const desk* d, uint32_t i) {
    const sel_state* st = sel_engine_state(d->engine);
    int repeat_all = strcmp(st->options[SEL_OPT_REPEAT], "all") == 0;
    int repeat_one = strcmp(st->options[SEL_OPT_REPEAT], "one") == 0;
    switch (i) {
    case SLOT_SUB_PREV:
        return st->song.subsong_count > 1u ? "subsong_prev" : NULL;
    case SLOT_SHUFFLE:
        return strcmp(st->options[SEL_OPT_RANDOM], "on") == 0 ? "random_off" : "random_on";
    case SLOT_PREV:
        return "prev";
    case SLOT_PLAY:
        return st->play == SEL_PLAYING ? "pause" : "play";
    case SLOT_NEXT:
        return "next";
    case SLOT_REPEAT:
        return repeat_all ? "repeat_one" : repeat_one ? "repeat_off" : "repeat_all";
    default:
        return st->song.subsong_count > 1u ? "subsong_next" : NULL;
    }
}

/* Canvas rectangle of transport button i: the row is centred in the player region, the gap is the spacing token. */
static sel_gui_rect desk_button_rect(const desk* d, uint32_t i) {
    sel_gui_rect pl = region_rect(d, "player");
    int32_t sp = lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_SPACING]);
    int32_t cx = pl.x + pl.w / 2;
    if (footer_one_row(d)) { /* DUI-P4-20: from the left edge; shuffle first, the subsong buttons and their label around it */
        int32_t step = lpx(d, TIP_BTN) + sp;
        int32_t x = pl.x + sp + (desk_slot_action(d, SLOT_SUB_PREV) != NULL ? lpx(d, 56) + sp + step : 0);
        return rect_of(x + ((int32_t)i - SLOT_SHUFFLE) * step, pl.y + (pl.h - lpx(d, TIP_BTN)) / 2, lpx(d, TIP_BTN), lpx(d, TIP_BTN));
    }
    return rect_of(cx - lpx(d, TIP_BTN / 2) + ((int32_t)i - SLOT_PLAY) * (lpx(d, TIP_BTN) + sp), pl.y + sp + lpx(d, 24),
                   lpx(d, TIP_BTN), lpx(d, TIP_BTN));
}

/* 1-based transport button under the canvas point, 0 = none. */
static int32_t desk_button_at(const desk* d, int32_t x, int32_t y) {
    uint32_t i;
    for (i = 0; i < TRANSPORT_COUNT; i++) {
        if (desk_slot_action(d, i) != NULL && in_rect(desk_button_rect(d, i), x, y)) {
            return (int32_t)i + 1;
        }
    }
    return 0;
}

/* The sidebar doors (DUI-P4-1, I262): an icon and a label per page, one row each under the brand. The icon-only breakpoint
 * (a narrow sidebar) draws the icons alone. door_page maps a door to the page it opens. */
static const uint32_t g_door_icon[DOOR_COUNT] = {SEL_GUI_ICON_HOUSE, SEL_GUI_ICON_SEARCH, SEL_GUI_ICON_LIBRARY, SEL_GUI_ICON_DISC,
                                                  SEL_GUI_ICON_LIST,  SEL_GUI_ICON_MUSIC,  SEL_GUI_ICON_USER};

/* Door i's page: the shared table (MUI-P4-0) names it, g_page_name maps the name to this UI's enum. */
static desk_page door_page(uint32_t i) {
    const sel_uim_door* dr = sel_uim_door_at(i);
    uint32_t p;
    for (p = 0; dr != NULL && p < PG_COUNT; p++) {
        if (strcmp(g_page_name[p], dr->page) == 0) {
            return (desk_page)p;
        }
    }
    return PG_HOME;
}

/* I673: the labelled sidebar opens the Library page from the "Your Library" header (Spotify), so the Library door row would only repeat
 * it and is not shown there; the icon-only rail has no header title and keeps the door. */
static int desk_door_shown(const desk* d, uint32_t i) {
    if (!sel_uim_door_shown(i, topbar_shown(d))) {
        return 0;
    }
    return !(door_page(i) == PG_FOLDERS && region_rect(d, "sidebar").w > lpx(d, 120));
}

static uint32_t desk_door_slot(const desk* d, uint32_t i) {
    uint32_t k, n = 0;
    for (k = 0; k < i && k < (uint32_t)DOOR_COUNT; k++) {
        n += (uint32_t)desk_door_shown(d, k);
    }
    return n;
}

/* The row of door i, shared by the drawing and the click: the panel inset of draw_panel, the brand, then the rows. */
static sel_gui_rect sidebar_door_rect(const desk* d, uint32_t i) {
    sel_gui_rect r = region_rect(d, "sidebar");
    int32_t sp = lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_SPACING]);
    int32_t g = sp / 2, row = lpx(d, 40);
    int32_t y = r.y + g + 2 * sp + lpx(d, HOME_BRAND) + lpx(d, 12) + (int32_t)desk_door_slot(d, i) * row;
    if (!desk_door_shown(d, i)) { /* I615a: the top bar owns this page: no row; I673: so does the Your Library header */
        return rect_of(r.x, y, 0, 0);
    }
    return rect_of(r.x + g + sp, y, r.w - 2 * g - 2 * sp, row);
}

/* I547: the width the labelled rail needs on the bitmap tier: the door inset, the row pad, icon, icon gap and the measured
 * widest label (the shorter form where a door has one, as draw_sidebar falls back to it), and the pad again so the label
 * never touches the edge of the highlighted row. 0 off the bitmap tier (the layout keeps its own widths). */
static uint32_t desk_rail_w(desk* d) {
    const sel_gui_atlas* at = desk_font(d, 1, 15);
    int32_t sp = lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_SPACING]), widest = 0, w;
    uint32_t i;
    if (!desk_bitmap_text(d) || at == NULL) {
        return 0u;
    }
    for (i = 0; i < DOOR_COUNT; i++) {
        const sel_uim_door* dr = sel_uim_door_at(i);
        const char* s = dr->shorter != NULL ? dr->shorter : dr->label;
        int32_t tw = sel_gui_text_width(at, s, strlen(s));
        widest = tw > widest ? tw : widest;
    }
    w = 2 * (sp / 2 + sp) + 2 * lpx(d, 12) + lpx(d, 22) + lpx(d, 8) + widest;
    return (uint32_t)px_to_logical_ceil(d, w);
}

/* The door under a point, -1 for none. */
static int32_t sidebar_door_at(const desk* d, int32_t x, int32_t y) {
    uint32_t i;
    for (i = 0; i < DOOR_COUNT; i++) {
        if (in_rect(sidebar_door_rect(d, i), x, y)) {
            return (int32_t)i;
        }
    }
    return -1;
}

/* I608: the Your Library list under the doors. The header band keeps its right end free for the controls of I609 and the
 * chips of I610; the rows below it scroll. One geometry for drawing, hit testing, the wheel and the test dump. */
static sel_gui_rect side_lib_header_rect(const desk* d) {
    sel_gui_rect r = sidebar_door_rect(d, DOOR_COUNT - 1u);
    int32_t h = lpx(d, 36);
    return rect_of(r.x, r.y + r.h + lpx(d, 8), r.w, h);
}

/* I609: the header controls. Create Playlist (plus) and Expand/Collapse Library (chevron) sit at the band's right end, 28 px
 * squares; the icon-only rail keeps the width toggle alone, centred, and the bitmap (Amiga) tier only the plus: it has no
 * wider layout. A zero rectangle is a control that is not drawn. */
#define TIP_HDR_BASE (TIP_CTL_BASE + (int32_t)CTL_COUNT)

static sel_gui_rect side_hdr_rect(const desk* d, uint32_t which) {
    sel_gui_rect h = side_lib_header_rect(d);
    int32_t sz = lpx(d, 28), pad = lpx(d, 8), y = h.y + (h.h - sz) / 2;
    int rail = region_rect(d, "sidebar").w <= lpx(d, 120);
    if (which == SIDE_HDR_WIDE) {
        if (desk_bitmap_text(d)) {
            return rect_of(0, 0, 0, 0);
        }
        return rect_of(rail ? h.x + (h.w - sz) / 2 : h.x + h.w - pad - sz, y, sz, sz);
    }
    if (rail) {
        return rect_of(0, 0, 0, 0);
    }
    return rect_of(h.x + h.w - pad - sz - (desk_bitmap_text(d) ? 0 : sz + lpx(d, 4)), y, sz, sz);
}

/* I673: the "Your Library" title at the left of the header band is the door to the Library page; empty on the icon-only rail. */
static sel_gui_rect side_hdr_title_rect(const desk* d) {
    sel_gui_rect h = side_lib_header_rect(d);
    int32_t w = side_hdr_rect(d, SIDE_HDR_PLUS).x - lpx(d, 4) - h.x;
    if (region_rect(d, "sidebar").w <= lpx(d, 120) || w <= 0) {
        return rect_of(0, 0, 0, 0);
    }
    return rect_of(h.x, h.y, w, h.h);
}

static int32_t side_hdr_at(const desk* d, int32_t x, int32_t y) {
    uint32_t k;
    for (k = 1u; k <= (uint32_t)SIDE_HDR_COUNT; k++) {
        sel_gui_rect r = side_hdr_rect(d, k);
        if (r.w > 0 && in_rect(r, x, y)) {
            return (int32_t)k;
        }
    }
    return 0;
}

static const char* side_hdr_label(const desk* d, uint32_t which) {
    return which == SIDE_HDR_PLUS ? "Create Playlist" : d->side_wide ? "Collapse Library" : "Expand Library";
}

/* I610: the chip row under the header. The chips are the kinds a library lists, Spotify's order. On the bitmap tier one
 * cycle button takes the row. The icon-only sidebar has no row. */
static const char* const side_chip_name[SIDE_CHIPS] = {"Playlists", "Albums", "Artists", "Composers", "Sources", "Local Files"};
/* I678b: the kind mask each chip hands the library call (design 2026-10-29 section 4): the store filters, not a UI loop. */
static const uint32_t side_chip_mask[SIDE_CHIPS] = {
    SEL_LIB_ITEM_BIT(SEL_LIB_ITEM_PLAYLIST), SEL_LIB_ITEM_BIT(SEL_LIB_ITEM_ALBUM),  SEL_LIB_ITEM_BIT(SEL_LIB_ITEM_ARTIST),
    SEL_LIB_ITEM_BIT(SEL_LIB_ITEM_COMPOSER), SEL_LIB_ITEM_BIT(SEL_LIB_ITEM_SOURCE), SEL_LIB_ITEM_BIT(SEL_LIB_ITEM_LOCAL)};

static int32_t side_chips_h(const desk* d) {
    return region_rect(d, "sidebar").w > lpx(d, 120) ? lpx(d, HOME_CHIP_H + 8) : 0;
}

static sel_gui_rect side_chips_rect(const desk* d) {
    sel_gui_rect h = side_lib_header_rect(d);
    return rect_of(h.x, h.y + h.h, h.w, side_chips_h(d));
}

/* I669: pads and gap tight enough that the four chips fit the expanded sidebar (360 wide). */
enum { SIDE_CHIP_PAD = 10, SIDE_CHIP_GAP = 6 };

/* The chip's width: its label plus the side pads. */
static int32_t side_chip_w(desk* d, uint32_t k) {
    const sel_gui_atlas* at = desk_font(d, 1, 13);
    return (at != NULL ? sel_gui_text_width(at, side_chip_name[k], strlen(side_chip_name[k])) : 40) + 2 * lpx(d, SIDE_CHIP_PAD);
}

static int32_t side_chip_total(desk* d) {
    int32_t t = 0;
    uint32_t k;
    for (k = 0; k < SIDE_CHIPS; k++) {
        t += side_chip_w(d, k) + (k != 0u ? lpx(d, SIDE_CHIP_GAP) : 0);
    }
    return t;
}


/* The scrolling band of the chips: the row less the pad; with an overflow a chevron button at each end takes its square. The
 * bitmap tier always counts as an overflow: its 6-row chips would be too small to read side by side (I669). */
static sel_gui_rect side_chip_band(desk* d, int* overflow) {
    sel_gui_rect r = side_chips_rect(d);
    int32_t pad = lpx(d, 12), cv = lpx(d, HOME_CHIP_H), gap = lpx(d, 4);
    sel_gui_rect b = rect_of(r.x + pad, r.y, r.w - 2 * pad, r.h);
    *overflow = desk_bitmap_text(d) || side_chip_total(d) > b.w;
    if (!*overflow && side_chip_has_x(d)) { /* only the selected chip shows, right of the X */
        b = rect_of(b.x + cv + gap, b.y, b.w - (cv + gap), b.h);
    }
    if (*overflow) {
        b = rect_of(b.x + cv + gap, b.y, b.w - 2 * (cv + gap), b.h);
    }
    return b;
}

/* I678b: with a chip selected the left slot of the row is the round clear (X) button; the bitmap tier keeps its stepping chevron. */
static int side_chip_has_x(desk* d) {
    return d->side_chip != 0u && !desk_bitmap_text(d);
}

static sel_gui_rect side_chip_chevron(desk* d, int right) {
    sel_gui_rect r = side_chips_rect(d);
    int32_t pad = lpx(d, 12), cv = lpx(d, HOME_CHIP_H);
    return rect_of(right ? r.x + r.w - pad - cv : r.x + pad, r.y, cv, cv);
}

/* I669: one control instead of the chip row when the chips do not fit the sidebar (and on the bitmap tier): a label naming
 * the filter in force that the chevrons step through All and the chips (a row of chips with only one in view hides the
 * choices, so a click on it means nothing). */
static int side_chip_single(desk* d) {
    int ov;
    side_chip_band(d, &ov);
    return ov;
}

/* The chip rectangle k; with a single control chip 0 is that control across the band and the rest are empty. */
static sel_gui_rect side_chip_rect(desk* d, uint32_t k) {
    int ov;
    uint32_t i;
    sel_gui_rect b = side_chip_band(d, &ov);
    int32_t x = b.x;
    if (side_chip_single(d)) {
        return k == 0u ? rect_of(b.x, b.y, b.w, lpx(d, HOME_CHIP_H)) : rect_of(0, 0, 0, 0);
    }
    if (d->side_chip != 0u && !side_chip_single(d) && side_chip_has_x(d)) { /* I678b: the other chips hide while one is selected */
        return k + 1u == d->side_chip ? rect_of(x, b.y, side_chip_w(d, k), lpx(d, HOME_CHIP_H)) : rect_of(0, 0, 0, 0);
    }
    for (i = 0; i < k; i++) {
        x += side_chip_w(d, i) + lpx(d, SIDE_CHIP_GAP);
    }
    return rect_of(x, b.y, side_chip_w(d, k), lpx(d, HOME_CHIP_H));
}

/* The control under a point: a chip 0..SIDE_CHIPS-1, SIDE_CHIPS for the left and SIDE_CHIPS + 1 for the right chevron, -1 for none. */
static int32_t side_chip_at(desk* d, int32_t x, int32_t y) {
    int ov;
    uint32_t k;
    sel_gui_rect b;
    if (side_chips_h(d) == 0) {
        return -1;
    }
    b = side_chip_band(d, &ov);
    if (side_chip_has_x(d) && in_rect(side_chip_chevron(d, 0), x, y)) {
        return SIDE_CHIPS + 2; /* I678b: the clear button */
    }
    if (ov) {
        if (in_rect(side_chip_chevron(d, 0), x, y)) {
            return SIDE_CHIPS;
        }
        if (in_rect(side_chip_chevron(d, 1), x, y)) {
            return SIDE_CHIPS + 1;
        }
        return -1; /* I669: the label between the chevrons only names the filter */
    }
    if (!in_rect(b, x, y)) {
        return -1;
    }
    for (k = 0; k < (uint32_t)SIDE_CHIPS; k++) {
        if (in_rect(side_chip_rect(d, k), x, y)) {
            return (int32_t)k;
        }
    }
    return -1;
}

/* A click on a chip: the chip in force again clears the filter, another one takes over. A chevron steps the filter one back or on, from All to the last chip, stopping at both ends (I669). */
static void side_chip_click(desk* d, int32_t c) {
    if (c == SIDE_CHIPS) {
        d->side_chip -= d->side_chip > 0u ? 1u : 0u;
        d->side_chip_scrolls++;
    } else if (c == SIDE_CHIPS + 1) {
        d->side_chip += d->side_chip < (uint32_t)SIDE_CHIPS ? 1u : 0u;
        d->side_chip_scrolls++;
    } else if (c == SIDE_CHIPS + 2) { /* I678b: the X clears the chip */
        d->side_chip = 0u;
    } else {
        d->side_chip = d->side_chip == (uint32_t)c + 1u ? 0u : (uint32_t)c + 1u;
    }
    if (d->side_chip != 1u) {
        d->side_sub = 0u; /* I678d: the second-level chips belong to Playlists */
    }
    d->side_y_q16 = 0;
    d->side_stale = 1u; /* the list is read again through the filter */
    d->repaint = 1;
    d->uim.dirty |= SEL_UIM_DIRTY_ALL;
}

/* I611: the tools row under the chips: magnifier and filter field on the left, the sort label on the right. */
static int32_t side_tools_h(const desk* d) {
    return side_chips_h(d) > 0 ? lpx(d, 36) : 0;
}

static sel_gui_rect side_tools_rect(const desk* d) {
    sel_gui_rect c = side_chips_rect(d);
    return rect_of(c.x, c.y + c.h, c.w, side_tools_h(d));
}

/* The tools row geometry is a function of the row's rectangle `t`, so the sidebar and the Library page share one layout (I611). */
static sel_gui_rect tl_mag_rect(const desk* d, sel_gui_rect t) {
    return rect_of(t.x + lpx(d, 12), t.y + lpx(d, 4), lpx(d, 28), lpx(d, 28));
}

static sel_gui_rect tl_sort_rect(const desk* d, sel_gui_rect t) {
    int32_t w = d->lf_open ? lpx(d, 28) : (t.w / 2 < lpx(d, 150) ? t.w / 2 : lpx(d, 150)), pad = lpx(d, 12);
    return rect_of(t.x + t.w - pad - w, t.y + lpx(d, 4), w, lpx(d, 28));
}

static sel_gui_rect tl_field_rect(const desk* d, sel_gui_rect t) {
    sel_gui_rect m = tl_mag_rect(d, t), s = tl_sort_rect(d, t);
    int32_t x = m.x + m.w + lpx(d, 4), w = s.x - lpx(d, 4) - x;
    return d->lf_open && w > 0 ? rect_of(x, m.y, w, m.h) : rect_of(0, 0, 0, 0);
}

/* 1 magnifier, 2 filter field, 3 sort label; -1 for none. */
static int32_t tl_at(const desk* d, sel_gui_rect t, int32_t x, int32_t y) {
    if (t.w <= 0 || t.h <= 0) {
        return -1;
    }
    if (in_rect(tl_mag_rect(d, t), x, y)) {
        return 1;
    }
    if (d->lf_open && in_rect(tl_field_rect(d, t), x, y)) {
        return 2;
    }
    return in_rect(tl_sort_rect(d, t), x, y) ? 3 : -1;
}

static sel_gui_rect side_mag_rect(const desk* d) {
    return tl_mag_rect(d, side_tools_rect(d));
}

static sel_gui_rect side_sort_rect(const desk* d) {
    return tl_sort_rect(d, side_tools_rect(d));
}

static sel_gui_rect side_field_rect(const desk* d) {
    return tl_field_rect(d, side_tools_rect(d));
}

static int32_t side_tools_at(const desk* d, int32_t x, int32_t y) {
    return side_tools_h(d) == 0 ? -1 : tl_at(d, side_tools_rect(d), x, y);
}

/* The Library page's tools row (I611): the same row under its tabs, bound to the same filter and sort state. */
static sel_gui_rect lib_tools_rect(desk* d, sel_gui_rect mn) {
    int32_t mg = lpx(d, LIB_MARGIN);
    return lib_tools_h(d) > 0 ? rect_of(mn.x + mg, mn.y + lib_list_y(d) - lib_tools_h(d), mn.w - 2 * mg, lib_tools_h(d)) : rect_of(0, 0, 0, 0);
}

/* The view size sets the cell: columns and cell height. The rail (icons only) always lists. */
static uint32_t side_lib_cols(const desk* d) {
    int32_t w = region_rect(d, "sidebar").w - lpx(d, 8), cell = d->side_view == SEL_UIM_LVIEW_SMALL ? lpx(d, 72) : lpx(d, 104);
    uint32_t n;
    if (side_chips_h(d) == 0 || d->side_view < SEL_UIM_LVIEW_SMALL) {
        return 1u;
    }
    n = (uint32_t)(w / cell);
    return n > 0u ? n : 1u;
}

/* I678d: the expanded sidebar in List density is a table (Title, Type, Date Added, Played) with a header row above the rows. */
static int side_table(const desk* d) {
    return d->side_wide && side_chips_h(d) > 0 && d->side_view == SEL_UIM_LVIEW_LIST;
}

static int32_t side_table_head_h(const desk* d) {
    return side_table(d) ? lpx(d, 24) : 0;
}

static sel_gui_rect side_lib_view_rect(const desk* d) {
    sel_gui_rect sb = region_rect(d, "sidebar"), h = side_lib_header_rect(d);
    int32_t y = h.y + h.h + side_chips_h(d) + side_tools_h(d) + side_table_head_h(d), bottom = sb.y + sb.h - lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_SPACING]);
    return rect_of(h.x, y, h.w, bottom > y ? bottom - y : 0);
}

/* I678d: the four densities (design section 6): Compact 32 px one line, List 48 px with the cover, Small grid the cover alone,
 * Large grid the cover with title and subtitle under it. The rail (icons only) always lists at 56 px. */
static int32_t side_lib_row_h(const desk* d) {
    if (side_chips_h(d) == 0) {
        return lpx(d, 56);
    }
    if (d->side_view == SEL_UIM_LVIEW_LIST) {
        return lpx(d, 48);
    }
    if (d->side_view == SEL_UIM_LVIEW_COMPACT) {
        return lpx(d, 32);
    }
    {
        int32_t cw = side_lib_view_rect(d).w / (int32_t)side_lib_cols(d);
        return d->side_view == SEL_UIM_LVIEW_SMALL ? cw : cw + lpx(d, 36);
    }
}

static uint32_t side_pinned_n(const desk* d) {
    uint32_t n = 0u;
    while (n < d->side_n && d->side_items[n].pinned) {
        n++;
    }
    return n;
}

/* I678d (Johan 2026-10-29): with no saved playlist, Spotify's "Create your first playlist" card sits BELOW the pinned rows on the
 * default view. Its height, rounded up to whole rows; 0 when it is not shown (a chip of another kind, a filter, a playlist exists, bitmap tier). */
static int32_t side_card_h(const desk* d) {
    uint32_t i;
    int32_t rh, ch = lpx(d, 128);
    if (side_chips_h(d) == 0 || desk_bitmap_text(d) || d->side_chip > 1u || d->lf_text[0] != '\0' || d->side_open_n != 0u || side_lib_view_rect(d).h <= 0) {
        return 0;
    }
    for (i = 0; i < d->side_n; i++) {
        if (d->side_items[i].kind == (uint32_t)SEL_LIB_ITEM_PLAYLIST) {
            return 0;
        }
    }
    rh = side_lib_row_h(d);
    return ((ch + rh - 1) / rh) * rh;
}

/* Where row i sits: column, y from the top of the list (the card, when shown, pushes the rows after the pinned ones down). */
static void side_lib_pos(const desk* d, uint32_t i, uint32_t* col, int32_t* y) {
    uint32_t cols = side_lib_cols(d), pn = side_pinned_n(d), row;
    int32_t extra = 0;
    if (side_card_h(d) > 0 && i >= pn) {
        row = (pn + cols - 1u) / cols + (i - pn) / cols;
        *col = (i - pn) % cols;
        extra = side_card_h(d);
    } else {
        row = i / cols;
        *col = i % cols;
    }
    *y = (int32_t)row * side_lib_row_h(d) + extra;
}

static int32_t side_lib_total(const desk* d) {
    uint32_t cols = side_lib_cols(d), pn = side_card_h(d) > 0 ? side_pinned_n(d) : 0u, rows;
    rows = pn > 0u ? (pn + cols - 1u) / cols + (d->side_n - pn + cols - 1u) / cols : (d->side_n + cols - 1u) / cols;
    return (int32_t)rows * side_lib_row_h(d) + side_card_h(d);
}

static int32_t side_lib_max_q16(const desk* d) {
    int32_t total = side_lib_total(d), view = side_lib_view_rect(d).h;
    return total > view ? px_to_logical(d, total - view) << 16 : 0;
}

/* I608, I631: the one read of the saved playlists, shared by the sidebar list and the song menu's Add to Playlist step. */
static uint32_t lib_touch_calls(desk* d) {
    sel_engine_stats ts;
    sel_engine_stats_get(d->engine, &ts);
    return ts.lib_touch_calls;
}

static void side_lib_reload(desk* d) {
    lsort_prefs_load(d);
    /* I678b: the chip is a kind mask the library applies; a pending Create Playlist reads playlists only (it waits for the new one). */
    d->side_n = sel_engine_library_items(d->engine, d->side_open_n != 0u ? SEL_LIB_ITEM_BIT(SEL_LIB_ITEM_PLAYLIST)
                                                                         : d->side_chip != 0u ? side_chip_mask[d->side_chip - 1u] : SEL_LIB_ITEM_ALL,
                                         0u, d->side_items, SIDE_LIB_MAX, NULL);
    if (d->side_sub == 1u && d->side_chip == 1u && d->side_open_n == 0u) { /* I678d: By You keeps the playlists you made */
        uint32_t a, b = 0u;
        for (a = 0u; a < d->side_n; a++) {
            if (strcmp(d->side_items[a].creator, "You") == 0) {
                d->side_items[b++] = d->side_items[a];
            }
        }
        d->side_n = b;
    }
    d->side_item_calls++;
    d->side_stale = 0u;
    d->side_filters++; /* I610: filter passes run */
    if (d->side_open_n != 0u) { /* I646: Create playlist asked to open the new playlist once the engine has made it */
        if (d->page != PG_COLLECTION || ++d->side_open_tries > 300u) {
            d->side_open_n = 0u;
        } else if (d->side_n >= d->side_open_n) { /* the list is oldest first: the new one is last */
            d->side_open_n = 0u;
            pls_open(d, d->side_items[d->side_n - 1u].id, d->side_items[d->side_n - 1u].name);
            d->pl_nlen = 0u;
            d->pl_name[0] = '\0';
            d->pl_edit = 1; /* the name field has the keyboard: typing renames it */
        } else {
            d->side_stale = 1u; /* the engine has not applied PLAYLIST_CREATE yet */
        }
    }
    if (d->side_open_n == 0u) { /* I611: the text filter AND-ed with the chip, then the sort; the stable insertion sort keeps equal rows in engine order */
        uint32_t i, j, w = 0, pinned = 0;
        for (i = 0; i < d->side_n; i++) {
            if (sel_uim_lfilter_match(d->side_items[i].name, d->lf_text)) {
                d->side_items[w++] = d->side_items[i];
            }
        }
        d->side_n = w;
        while (pinned < d->side_n && d->side_items[pinned].pinned) { /* I678b: the pinned rows keep their fixed order above the sort */
            pinned++;
        }
        for (i = pinned + 1u; i < d->side_n; i++) {
            sel_engine_library_item it = d->side_items[i];
            sel_uim_lsort_key ka, kb = {it.kind, it.name, it.creator, it.added, it.last_played, it.opened};
            for (j = i; j > pinned; j--) { /* I678c: the kind-aware order lives in the core (Recents, the "The" rule, Creator per kind) */
                const sel_engine_library_item* p = &d->side_items[j - 1u];
                ka = (sel_uim_lsort_key){p->kind, p->name, p->creator, p->added, p->last_played, p->opened};
                if (sel_uim_lsort_item_cmp(d->side_sort, &ka, &kb) <= 0) {
                    break;
                }
                d->side_items[j] = d->side_items[j - 1u];
            }
            d->side_items[j] = it;
        }
        d->side_sorts++;
    }
    d->side_y_q16 = d->side_y_q16 > side_lib_max_q16(d) ? side_lib_max_q16(d) : d->side_y_q16;
}

/* The sort and view choice survive a restart: ui.desktop.library_sort and ui.desktop.library_view (include/selecta/prefs.def). Read once, written on every pick. */
static void lib_view_push(desk* d);

static void lsort_prefs_load(desk* d) {
    const char* v;
    if (d->side_prefs_read) {
        return;
    }
    d->side_prefs_read = 1u;
    v = desk_pref(d, "ui.desktop.library_sort", "0");
    if (v != NULL && v[0] >= '0' && v[0] < (char)('0' + SEL_UIM_LSORTS) && v[1] == '\0') {
        d->side_sort = (uint32_t)(v[0] - '0');
    }
    v = desk_pref(d, "ui.desktop.library_view", "1");
    if (v != NULL && v[0] >= '0' && v[0] < (char)('0' + SEL_UIM_LVIEWS) && v[1] == '\0') {
        d->side_view = (uint32_t)(v[0] - '0');
    }
    if (d->side_sort != 0u) { /* the saved sort also orders the Library page rows */
        lib_view_push(d);
    }
}

/* I611: the Library page rows are the engine's composer list; the filter text and sort of the tools row (the state the sidebar
 * Your Library list shares) go to the engine as COMPOSERS_VIEW, the one place that filters and orders them. */
static void lib_view_push(desk* d) {
    char line[160];
    size_t o = (size_t)snprintf(line, sizeof line, "COMPOSERS_VIEW \"");
    for (const char* c = d->lf_text; *c != '\0' && o + 4u < sizeof line - 8u; c++) {
        if (*c == '"' || *c == '\\') {
            line[o++] = '\\';
        }
        line[o++] = *c;
    }
    snprintf(line + o, sizeof line - o, "\" %u", (unsigned)d->side_sort);
    desk_send(d, line);
    d->lib_view_sends++;
}

static void lsort_prefs_save(desk* d, const char* key, uint32_t value) {
    char line[96];
    snprintf(line, sizeof line, "PREF_SET %s \"%u\"", key, (unsigned)value);
    desk_send(d, line);
    d->side_pref_sets++;
}

static void side_sort_pick(desk* d, uint32_t sort) {
    d->side_sort = sort;
    lsort_prefs_save(d, "ui.desktop.library_sort", sort);
    d->side_sort_picks++;
    lib_view_push(d);
    d->side_stale = 1u;
    d->side_y_q16 = 0;
    d->repaint = 1;
    d->uim.dirty |= SEL_UIM_DIRTY_ALL;
}

static void side_view_pick(desk* d, uint32_t view) {
    d->side_view = view;
    lsort_prefs_save(d, "ui.desktop.library_view", view);
    d->side_view_picks++;
    d->side_y_q16 = 0;
    d->repaint = 1;
    d->uim.dirty |= SEL_UIM_DIRTY_ALL;
}

/* The menu under the sort label (Spotify's Sort by and View as); on the bitmap tier the label cycles the sort instead. */
static void tl_click(desk* d, sel_gui_rect t, int32_t k) {
    if (k == 1) { /* the magnifier opens the filter field with the keyboard; again it closes the field and drops the filter */
        d->lf_open = !d->lf_open;
        d->lf_edit = d->lf_open;
        if (!d->lf_open && d->lf_len > 0u) {
            d->lf_len = 0u;
            d->lf_text[0] = '\0';
            d->side_stale = 1u;
            lib_view_push(d);
        }
    } else if (k == 2) {
        d->lf_edit = 1;
    } else if (desk_bitmap_text(d)) {
        side_sort_pick(d, (d->side_sort + 1u) % SEL_UIM_LSORTS);
    } else {
        sel_gui_rect r = tl_sort_rect(d, t);
        d->sm_open = 1;
        d->sm_step = 4;
        d->menu_x = r.x;
        d->menu_y = r.y + r.h;
        d->menus++;
        d->side_menu_opens++;
    }
    d->repaint = 1;
    d->uim.dirty |= SEL_UIM_DIRTY_ALL;
}

/* A typed key while the library filter field has the keyboard (the same edit rules as the playlist name field). */
static int lf_key(desk* d, const sel_gui_input* ev) {
    if (ev->code == 13u || ev->code == 27u) {
        d->lf_edit = 0;
    } else if (ev->code == 127u) {
        if (d->lf_len > 0u) {
            d->lf_text[--d->lf_len] = '\0';
            d->side_stale = 1u;
            lib_view_push(d);
            d->side_filter_keys++;
        }
    } else if (ev->code >= 32u && ev->code < 127u && (ev->mods & (TXT_MOD_CTRL | TXT_MOD_ALT)) == 0u) {
        if (d->lf_len + 1u < sizeof d->lf_text) {
            d->lf_text[d->lf_len++] = (char)ev->code;
            d->lf_text[d->lf_len] = '\0';
            d->side_stale = 1u;
            lib_view_push(d);
            d->side_filter_keys++;
        }
    } else {
        return 0;
    }
    d->repaint = 1;
    d->uim.dirty |= SEL_UIM_DIRTY_ALL;
    return desk_present(d);
}

static sel_gui_rect lmenu_strip_rect(const desk* d, uint32_t seg) {
    sel_gui_rect m = song_menu_item_rect(d, 7);
    int32_t inset = lpx(d, 8), sw = (m.w - 2 * inset) / (int32_t)SEL_UIM_LVIEWS;
    return rect_of(m.x + inset + (int32_t)seg * sw, m.y, sw, m.h);
}

static sel_gui_rect side_lib_row_rect(const desk* d, uint32_t i) {
    sel_gui_rect v = side_lib_view_rect(d);
    uint32_t cols = side_lib_cols(d), col;
    int32_t cw = v.w / (int32_t)cols, y;
    side_lib_pos(d, i, &col, &y);
    return rect_of(v.x + (int32_t)col * cw, v.y + y - lpx(d, d->side_y_q16 >> 16), cw, side_lib_row_h(d));
}

/* The row under a canvas point, -1 for none; rows scrolled out of the view do not count. */
static int32_t side_lib_at(const desk* d, int32_t x, int32_t y) {
    uint32_t i;
    if (!in_rect(side_lib_view_rect(d), x, y)) {
        return -1;
    }
    for (i = 0; i < d->side_n; i++) {
        if (in_rect(side_lib_row_rect(d, i), x, y)) {
            return (int32_t)i;
        }
    }
    return -1;
}

/* The item the main region shows now: a playlist is open while the Playlists page selects it. */
static int side_lib_open_item(const desk* d, uint32_t i) {
    const sel_engine_library_item* it = &d->side_items[i];
    if (it->kind == (uint32_t)SEL_LIB_ITEM_PLAYLIST) {
        return d->page == PG_COLLECTION && d->pl_sel_kind == (uint32_t)SEL_ROW_PLAYLIST && d->pl_sel_id == it->id;
    }
    /* I678f/I678g: every other kind has its own collection page, which marks its row */
    return d->page == PG_COLLECTION && d->pls_id != 0u && d->col_kind == col_ui_kind(it->kind) && strcmp(d->col_key, it->key) == 0;
}

/* The tools row: magnifier, the filter field when open, and the sort label with the list icon (sidebar and Library page). */
static void tl_draw(desk* d, sel_gui_rect t) {
    lsort_prefs_load(d);
    {   /* I611: the tools row: magnifier, the filter field when open, and the sort label with the list icon */
        sel_gui_rect mg = tl_mag_rect(d, t), fr = tl_field_rect(d, t), sr = tl_sort_rect(d, t);
        uint32_t muted = ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT_MUTED]), ink = hover_ink_of(d, mg, muted, ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT]));
        sel_gui_icon_draw(d->canvas, &d->icons, SEL_GUI_ICON_SEARCH, rect_of(mg.x + lpx(d, 4), mg.y + lpx(d, 4), lpx(d, 20), lpx(d, 20)), lpx(d, 20), ink);
        if (fr.w > 0) {
            const sel_gui_atlas* at = desk_font(d, 0, 13);
            sel_gui_round_rect(d->canvas, fr, lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_RADIUS_SMALL]), ARGB(d->theme.token[SEL_GUI_TOKEN_SURFACE_RAISED]));
            if (d->lf_len > 0u) {
                draw_text(d, rect_of(fr.x + lpx(d, 8), fr.y, fr.w - lpx(d, 16), fr.h), d->lf_text, 0, 13, ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT]));
            } else {
                draw_text(d, rect_of(fr.x + lpx(d, 8), fr.y, fr.w - lpx(d, 16), fr.h), "Search in Your Library", 0, 13, muted);
            }
            if (d->lf_edit) { /* the caret: a thin bar at the text end */
                int32_t tw = at != NULL ? sel_gui_text_width(at, d->lf_text, d->lf_len) : 0, ch = lpx(d, 16);
                sel_gui_fill_rect(d->canvas, rect_of(fr.x + lpx(d, 8) + tw, fr.y + (fr.h - ch) / 2, lpx(d, 1) > 0 ? lpx(d, 1) : 1, ch), ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT]));
            }
        }
        hover_reg(d, sr);
        {
            uint32_t sink = hover_ink_of(d, sr, muted, ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT]));
            int32_t isz = lpx(d, 18);
            sel_gui_icon_draw(d->canvas, &d->icons, SEL_GUI_ICON_LIST, rect_of(sr.x + sr.w - isz, sr.y + (sr.h - isz) / 2, isz, isz), isz, sink);
            if (sr.w > lpx(d, 40)) {
                draw_text(d, rect_of(sr.x, sr.y, sr.w - isz - lpx(d, 6), sr.h), sel_uim_lsort_label(d->side_sort), 0, 13, sink);
            }
        }
        hover_reg(d, mg);
        sel_gui_dirty_add(d->canvas, t);
    }
}

/* I678b: the cover of a row: the pinned rows get a raised tile with their icon (Liked Songs a heart, Recently Played a list,
 * Local Files the library mark); everything else the generated art from the name (round avatars and album covers: I678c). */
static void side_item_thumb(desk* d, sel_gui_rect ar, const sel_engine_library_item* it) {
    if (it->pinned) {
        uint32_t ic = it->kind == (uint32_t)SEL_LIB_ITEM_LIKED ? SEL_GUI_ICON_HEART : it->kind == (uint32_t)SEL_LIB_ITEM_RECENT ? SEL_GUI_ICON_LIST : SEL_GUI_ICON_LIBRARY;
        sel_gui_round_rect(d->canvas, ar, lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_RADIUS]), ARGB(d->theme.token[SEL_GUI_TOKEN_SURFACE_RAISED]));
        sel_gui_icon_draw(d->canvas, &d->icons, ic, rect_of(ar.x + ar.w / 4, ar.y + ar.h / 4, ar.w / 2, ar.h / 2), ar.w / 2, ARGB(d->theme.token[SEL_GUI_TOKEN_ACCENT]));
        return;
    }
    if (it->kind == (uint32_t)SEL_LIB_ITEM_ALBUM) { /* I692: an album row shows its cover, named like a Home album tile: "<album>\x1f<artist>" */
        char ref[200];
        snprintf(ref, sizeof ref, it->sub[0] != '\0' ? "%s\x1f%s" : "%s", it->name, it->sub);
        if (cover_draw(d, ar, ref, lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_RADIUS]))) {
            d->pc_side = 0u;
            d->cover_side++;
            return;
        }
    }
    d->pc_side = 0u;
    family_platform_cover(d, d->canvas, ar, sel_platform_of_ext(it->format), it->name, &d->pc_side); /* I691d: mark only on this small thumb */
}

static void side_lib_draw(desk* d, sel_gui_rect sb) {
    sel_gui_rect hd = side_lib_header_rect(d), v = side_lib_view_rect(d);
    int32_t pad = lpx(d, 12), cov = lpx(d, 44), gap = lpx(d, 12);
    int labels = sb.w > lpx(d, 120);
    uint32_t i;
    {   /* I678c: the engine applied a LIBRARY_TOUCH (an opened time changed): the Recents order is stale */
        sel_engine_stats ts;
        sel_engine_stats_get(d->engine, &ts);
        if (ts.lib_touch_calls != d->side_touch_seen) {
            d->side_touch_seen = ts.lib_touch_calls;
            d->side_stale = 1u;
        }
    }
    if (d->side_stale) { /* the saved playlists changed: read them again */
        side_lib_reload(d);
    }
    if (labels) {
        int32_t hw = side_hdr_rect(d, SIDE_HDR_PLUS).x - lpx(d, 4) - (hd.x + pad);
        sel_gui_rect tr = side_hdr_title_rect(d);
        uint32_t ink = ARGB(d->theme.token[d->page == PG_FOLDERS ? SEL_GUI_TOKEN_TEXT : SEL_GUI_TOKEN_TEXT_MUTED]);
        if (d->page == PG_FOLDERS) {
            hover_reg(d, tr);
        } else {
            ink = hover_ink_of(d, tr, ink, ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT]));
        }
        draw_text(d, rect_of(hd.x + pad, hd.y, hw > 0 ? hw : 0, hd.h), "Your Library", 1, 15, ink);
    }
    for (i = 1u; i <= (uint32_t)SIDE_HDR_COUNT; i++) { /* I609: Create Playlist and Expand/Collapse Library */
        sel_gui_rect br = side_hdr_rect(d, i);
        uint32_t ic = i == SIDE_HDR_PLUS ? SEL_GUI_ICON_PLUS : d->side_wide ? SEL_GUI_ICON_CHEVRON_LEFT : SEL_GUI_ICON_CHEVRON_RIGHT;
        if (br.w > 0) {
            uint32_t ink = hover_ink_of(d, br, ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT_MUTED]), ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT]));
            sel_gui_icon_draw(d->canvas, &d->icons, ic, rect_of(br.x + lpx(d, 4), br.y + lpx(d, 4), lpx(d, 20), lpx(d, 20)), lpx(d, 20), ink);
        }
    }
    d->side_rows = 0u;
    d->side_empty_btn = rect_of(0, 0, 0, 0);
    if (labels) { /* I610: the chip row, or the single label with chevrons (I669) when the chips do not fit or on the bitmap tier */
        int ov;
        uint32_t k;
        sel_gui_rect band = side_chip_band(d, &ov), cr = side_chips_rect(d);
        sel_gui_clip_push(d->canvas, band);
        if (side_chip_single(d)) {
            sel_gui_rect sr = side_chip_rect(d, 0);
            if (d->side_chip == 1u && side_chip_has_x(d)) { /* I678d: room at the right end for the By You chip */
                sr.w -= lpx(d, 64) + lpx(d, SIDE_CHIP_GAP);
            }
            draw_chip(d, sr, d->side_chip == 0u ? "All" : side_chip_name[d->side_chip - 1u], d->side_chip != 0u, SEL_GUI_TOKEN_SURFACE_RAISED);
        } else {
            for (k = 0; k < SIDE_CHIPS; k++) {
                if (side_chip_rect(d, k).w > 0) { /* I678b: a selected chip hides the others */
                    draw_chip(d, side_chip_rect(d, k), side_chip_name[k], d->side_chip == k + 1u, SEL_GUI_TOKEN_SURFACE_RAISED);
                }
            }
        }
        sel_gui_clip_pop(d->canvas);
        d->side_sub_btn = rect_of(0, 0, 0, 0);
        if (d->side_chip == 1u && side_chip_has_x(d)) { /* I678d: the second-level chip of Playlists */
            sel_gui_rect pr = side_chip_rect(d, 0);
            d->side_sub_btn = rect_of(side_chip_single(d) ? pr.x + pr.w - lpx(d, 64) : pr.x + pr.w + lpx(d, SIDE_CHIP_GAP), pr.y, lpx(d, 64), pr.h);
            draw_chip(d, d->side_sub_btn, "By You", d->side_sub == 1u, SEL_GUI_TOKEN_SURFACE_RAISED);
        }
        if (side_chip_has_x(d)) { /* I678b: the round clear button left of the selected chip */
            sel_gui_rect xr = side_chip_chevron(d, 0);
            hover_reg(d, xr);
            sel_gui_round_rect(d->canvas, xr, xr.h / 2, hover_fill_of(d, xr, ARGB(d->theme.token[SEL_GUI_TOKEN_SURFACE_RAISED])));
            sel_gui_icon_draw(d->canvas, &d->icons, SEL_GUI_ICON_X, rect_of(xr.x + xr.w / 4, xr.y + xr.h / 4, xr.w / 2, xr.h / 2), xr.w / 2,
                              ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT]));
        }
        if (ov) {
            for (k = side_chip_has_x(d) ? 1u : 0u; k < 2u; k++) {
                sel_gui_rect cv = side_chip_chevron(d, (int)k);
                int can = k == 0u ? d->side_chip > 0u : d->side_chip < (uint32_t)SIDE_CHIPS;
                hover_reg(d, cv);
                sel_gui_round_rect(d->canvas, cv, cv.h / 2, hover_fill_of(d, cv, ARGB(d->theme.token[SEL_GUI_TOKEN_SURFACE_RAISED])));
                sel_gui_icon_draw(d->canvas, &d->icons, k == 0u ? SEL_GUI_ICON_CHEVRON_LEFT : SEL_GUI_ICON_CHEVRON_RIGHT,
                                  rect_of(cv.x + cv.w / 4, cv.y + cv.h / 4, cv.w / 2, cv.h / 2), cv.w / 2,
                                  ARGB(d->theme.token[can ? SEL_GUI_TOKEN_TEXT : SEL_GUI_TOKEN_TEXT_MUTED]));
            }
        }
        sel_gui_dirty_add(d->canvas, cr);
        tl_draw(d, side_tools_rect(d));
    }
    if (d->side_n == 0u && labels && v.h > 0 && d->side_chip > 1u) { /* a chip of a kind without rows */
        draw_text(d, rect_of(v.x + pad, v.y, v.w - 2 * pad, lpx(d, 28)), "Nothing here yet.", 0, 14, ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT_MUTED]));
    } else if (d->side_n == 0u && labels && v.h > 0 && desk_bitmap_text(d)) {
        draw_text(d, rect_of(v.x + pad, v.y, v.w - 2 * pad, lpx(d, 28)), "No playlists yet.", 0, 14, ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT_MUTED]));
    }
    if (labels && side_table(d)) { /* I678d: the table header over the rows */
        uint32_t muted = ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT_MUTED]);
        int32_t hy = v.y - side_table_head_h(d), tw = v.w - 2 * pad, c1 = v.x + pad + tw * 52 / 100, c2 = v.x + pad + tw * 68 / 100, c3 = v.x + pad + tw * 84 / 100;
        draw_text(d, rect_of(v.x + pad, hy, c1 - v.x - pad, side_table_head_h(d)), "Title", 1, 12, muted);
        draw_text(d, rect_of(c1, hy, c2 - c1, side_table_head_h(d)), "Type", 1, 12, muted);
        draw_text(d, rect_of(c2, hy, c3 - c2, side_table_head_h(d)), "Date Added", 1, 12, muted);
        draw_text(d, rect_of(c3, hy, v.x + v.w - pad - c3, side_table_head_h(d)), "Played", 1, 12, muted);
    }
    if (labels && side_card_h(d) > 0) { /* I627, I678d: no saved playlist: Spotify's card with one button, below the pinned rows */
        uint32_t text = ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT]), muted = ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT_MUTED]), cn = side_pinned_n(d), cc = side_lib_cols(d);
        int32_t cw = v.w - 2 * pad, ch = lpx(d, 120), bw = lpx(d, 150);
        sel_gui_rect card = rect_of(v.x + pad, v.y + (int32_t)((cn + cc - 1u) / cc) * side_lib_row_h(d) + lpx(d, 4) - lpx(d, d->side_y_q16 >> 16), cw, ch);
        sel_gui_clip_push(d->canvas, v);
        sel_gui_round_rect(d->canvas, card, lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_RADIUS]), ARGB(d->theme.token[SEL_GUI_TOKEN_SURFACE]));
        draw_text(d, rect_of(card.x + pad, card.y + lpx(d, 12), card.w - 2 * pad, lpx(d, 20)), "Create your first playlist", 1, 14, text);
        draw_text(d, rect_of(card.x + pad, card.y + lpx(d, 34), card.w - 2 * pad, lpx(d, 18)), "Name it, then add songs from your library", 0, 12, muted);
        d->side_empty_btn = rect_of(card.x + pad, card.y + lpx(d, 68), bw < card.w - 2 * pad ? bw : card.w - 2 * pad, lpx(d, 36));
        draw_button(d, d->side_empty_btn, text, "Create playlist", 13, ARGB(d->theme.token[SEL_GUI_TOKEN_PANEL]));
        d->side_empty_draws++;
        sel_gui_clip_pop(d->canvas);
    }
    sel_gui_clip_push(d->canvas, v);
    for (i = 0; i < d->side_n; i++) {
        sel_gui_rect r = side_lib_row_rect(d, i), ar;
        int open = side_lib_open_item(d, i);
        if (r.y + r.h <= v.y || r.y >= v.y + v.h) {
            continue;
        }
        if (open) {
            sel_gui_round_rect(d->canvas, r, lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_RADIUS]), ARGB(d->theme.token[SEL_GUI_TOKEN_SURFACE]));
        }
        if (labels && d->side_view >= SEL_UIM_LVIEW_SMALL) { /* I611: grid cells: the cover fills the cell, the large grid names it below */
            int32_t g = lpx(d, 4), cs = r.w - 2 * g;
            ar = rect_of(r.x + g, r.y + g, cs, cs);
            side_item_thumb(d, ar, &d->side_items[i]);
            if (d->side_view == SEL_UIM_LVIEW_LARGE) {
                char gsub[128];
                draw_text(d, rect_of(ar.x, ar.y + cs, cs, lpx(d, 18)), d->side_items[i].name, 1, 12, open ? ARGB(d->theme.token[SEL_GUI_TOKEN_ACCENT]) : row_ink(d, r, ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT])));
                sel_uim_library_item_subtitle(d->side_items[i].kind, d->side_items[i].sub, d->side_items[i].songs, gsub, sizeof gsub);
                draw_text(d, rect_of(ar.x, ar.y + cs + lpx(d, 16), cs, lpx(d, 16)), gsub, 0, 11, ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT_MUTED]));
            }
            d->side_rows++;
            continue;
        }
        if (labels && d->side_view == SEL_UIM_LVIEW_COMPACT) { /* I678d: compact: no cover, the bold title then a muted "Type - Subtitle" on one line */
            char sub[128], line[256];
            int32_t tx = r.x + pad, tw = r.x + r.w - pad - tx, nw;
            uint32_t ink = open ? ARGB(d->theme.token[SEL_GUI_TOKEN_ACCENT]) : row_ink(d, r, ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT]));
            sel_uim_library_item_subtitle(d->side_items[i].kind, d->side_items[i].sub, d->side_items[i].songs, sub, sizeof sub);
            nw = tw * 55 / 100;
            draw_text(d, rect_of(tx, r.y, nw, r.h), d->side_items[i].name, 1, 14, ink);
            snprintf(line, sizeof line, "%s%s%s", sel_uim_library_item_type(d->side_items[i].kind), sub[0] ? " \xE2\x80\xA2 " : "", sub);
            draw_text(d, rect_of(tx + nw + gap, r.y, tw - nw - gap, r.h), line, 0, 12, ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT_MUTED]));
            d->side_rows++;
            continue;
        }
        cov = labels && r.h < lpx(d, 56) ? r.h - lpx(d, 8) : lpx(d, 44);
        ar = rect_of(r.x + (labels ? pad : (r.w - cov) / 2), r.y + (r.h - cov) / 2, cov, cov);
        side_item_thumb(d, ar, &d->side_items[i]);
        if (labels) {
            int32_t tx = ar.x + cov + gap, tw = r.x + r.w - pad - tx, th = lpx(d, 20);
            if (side_table(d)) { /* I678d: the expanded table: Title | Type | Date Added | Played */
                int32_t tt = v.w - 2 * pad, c1 = v.x + pad + tt * 52 / 100, c2 = v.x + pad + tt * 68 / 100, c3 = v.x + pad + tt * 84 / 100;
                char when[48];
                uint32_t mu = ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT_MUTED]);
                tw = c1 - gap - tx;
                sel_uim_relative_time((int64_t)time(NULL), d->side_items[i].kind == (uint32_t)SEL_LIB_ITEM_PLAYLIST ? 0u : d->side_items[i].added, when, sizeof when);
                draw_text(d, rect_of(c1, r.y, c2 - c1, r.h), sel_uim_library_item_type(d->side_items[i].kind), 0, 12, mu);
                draw_text(d, rect_of(c2, r.y, c3 - c2, r.h), when, 0, 12, mu);
                sel_uim_relative_time((int64_t)time(NULL), d->side_items[i].last_played, when, sizeof when);
                draw_text(d, rect_of(c3, r.y, v.x + v.w - pad - c3, r.h), when, 0, 12, mu);
            }
            uint32_t ink = open ? ARGB(d->theme.token[SEL_GUI_TOKEN_ACCENT]) : row_ink(d, r, ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT]));
            draw_text(d, rect_of(tx, ar.y + lpx(d, 2), tw, th), d->side_items[i].name, 1, 14, ink);
            char subtitle[128];
            sel_uim_library_item_subtitle(d->side_items[i].kind, d->side_items[i].sub, d->side_items[i].songs, subtitle, sizeof subtitle);
            draw_text(d, rect_of(tx, ar.y + lpx(d, 2) + th, tw, th), subtitle, 0, 12, ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT_MUTED]));
        }
        d->side_rows++;
    }
    sel_gui_clip_pop(d->canvas);
    d->side_draws++;
    sel_gui_dirty_add(d->canvas, rect_of(hd.x, hd.y, hd.w, hd.h + side_chips_h(d) + side_tools_h(d) + v.h));
}

static void draw_sidebar(desk* d) {
    sel_gui_rect sb = draw_panel(d, "sidebar");
    int32_t sp = lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_SPACING]);
    int32_t row = lpx(d, 40), isz = lpx(d, 22), br = lpx(d, HOME_BRAND), y = sb.y + 2 * sp;
    int32_t pad = lpx(d, 12), lab = lpx(d, 16);
    const sel_gui_atlas* door_at = desk_font(d, 1, 15);
    sel_gui_rect logo = rect_of(sb.x + pad + sp, y, br, br);
    uint32_t i;
    sel_gui_round_rect(d->canvas, logo, lpx(d, 7), ARGB(d->theme.token[SEL_GUI_TOKEN_ACCENT]));
    sel_gui_icon_draw(d->canvas, &d->icons, SEL_GUI_ICON_PLAY, rect_of(logo.x + lpx(d, 6), logo.y + lpx(d, 6), lab, lab), lab,
                      ARGB(d->theme.token[SEL_GUI_TOKEN_ACCENT_TEXT]));
    if (sb.w > lpx(d, 120)) {
        draw_text(d, rect_of(logo.x + br + lpx(d, 10), y, sb.x + sb.w - (logo.x + br + lpx(d, 10)), br), "Selecta", 1, 18,
                  ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT]));
    }
    d->home_parts |= HP_BRAND;
    y += br + pad;
    d->lit_doors = 0u;
    d->doors_drawn = 0u;
    for (i = 0; i < DOOR_COUNT; i++) {
        sel_gui_rect r = sidebar_door_rect(d, i);
        if (!desk_door_shown(d, i)) { /* I615a: Home and Search live in the top bar; I673: Your Library is the header */
            continue;
        }
        d->doors_drawn |= 1u << i;
        sel_gui_rect ic = rect_of(r.x + pad, r.y + (row - isz) / 2, isz, isz);
        int here = door_page(i) == d->page;
        uint32_t ink = ARGB(d->theme.token[here ? SEL_GUI_TOKEN_TEXT : SEL_GUI_TOKEN_TEXT_MUTED]);
        if (here) { /* the current page: a raised row behind it */
            d->lit_doors |= 1u << i;
            hover_reg(d, r);
            sel_gui_round_rect(d->canvas, r, lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_RADIUS]), ARGB(d->theme.token[SEL_GUI_TOKEN_SURFACE]));
        } else { /* another door: no background, its ink fades up as the pointer enters and back down as it leaves (I346) */
            ink = hover_ink_of(d, r, ink, ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT]));
        }
        sel_gui_icon_draw(d->canvas, &d->icons, g_door_icon[i], ic, isz, ink);
        if (sb.w > lpx(d, 120)) {
            /* I537: the label box is the measured width the rail leaves right of the icon; a label wider than it takes its
             * shorter form (draw_text would ellipsize it), and the gap to the icon shrinks to the pad on the bitmap tier. */
            int32_t gap = desk_bitmap_text(d) ? lpx(d, 8) : lab, lx = ic.x + isz + gap, lw = r.x + r.w - lx;
            const sel_uim_door* dr = sel_uim_door_at(i);
            const char* label = dr->label;
            if (door_at != NULL && dr->shorter != NULL && sel_gui_text_width(door_at, label, strlen(label)) > lw) {
                label = dr->shorter;
            }
            draw_text(d, rect_of(lx, r.y, lw, row), label, 1, 15, ink);
        }
        sel_gui_dirty_add(d->canvas, r);
        d->frame_parts++;
        d->home_parts |= HP_NAV;
    }
    side_lib_draw(d, sb);
}

/* The player bar's left block (mockup footer): artwork of the playing song with its title and artist beside it. Hidden
 * when the bar is too narrow to share the row with the transport. */
static void draw_player_left(desk* d) {
    const sel_uim_frame* f = &d->uim.frame;
    sel_gui_rect pl = region_rect(d, "player");
    int32_t sp = lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_SPACING]);
    int32_t as = lpx(d, 56), gap = lpx(d, 12);
    sel_gui_rect ar, box, all;
    if (pl.w < lpx(d, 900) || pl.h < as) {
        return;
    }
    ar = rect_of(pl.x + sp, pl.y + (pl.h - as) / 2, as, as);
    box = rect_of(ar.x + as + gap, ar.y, lpx(d, 300) - as - gap - sp, as);
    all = rect_of(pl.x, pl.y, lpx(d, 300) + sp, pl.h);
    sel_gui_fill_rect(d->canvas, all, ARGB(d->theme.token[SEL_GUI_TOKEN_FRAME]));
    if (cover_draw(d, ar, sel_engine_state(d->engine)->song.path, lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_RADIUS]))) { /* I692 */
        d->cover_bar++;
        d->pc_bar = 0;
    } else {
        song_platform_cover(d, ar, sel_engine_state(d->engine)->song.path, f->title, &d->pc_bar);
    }
    d->home_parts |= HP_PLAYER_ART;
    draw_text(d, rect_of(box.x, box.y + lpx(d, 10), box.w, lpx(d, 18)), f->title, 1, 14, ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT]));
    draw_text(d, rect_of(box.x, box.y + lpx(d, 30), box.w, lpx(d, 16)), f->artist, 0, 12,
              ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT_MUTED]));
    d->home_parts |= HP_PLAYER_TITLE;
    sel_gui_dirty_add(d->canvas, all);
}

/* The Now Playing panel (DUI-P4-1, DUI-P4-7) in the detail region, after Spotify's Now Playing view: a header tinted from the
 * artwork, the artwork, title and artist, the catalogue pairs the engine reports for the song (the text UI's information
 * panel data), the synced lyrics with the current line highlighted, the next queue entries, and like and rating marks. */
#define NP_PAIRS_MAX 6u
#define NP_UPNEXT_MAX 3u
#define NP_MESSAGE_LINES 6
static uint32_t np_art_average(const desk* d, sel_gui_rect ar) {
    const uint32_t* px = (const uint32_t*)d->canvas->pixels;
    uint32_t r = 0, g = 0, b = 0, n = 0;
    int32_t ix, iy;
    if (d->canvas->format != SEL_GUI_RGBA32) {
        return ARGB(d->theme.token[SEL_GUI_TOKEN_HEADER_TINT]);
    }
    for (iy = 1; iy < 8; iy++) {
        for (ix = 1; ix < 8; ix++) {
            int32_t yd = d->canvas->ydiv > 1 ? d->canvas->ydiv : 1;
            int32_t x = ar.x + ar.w * ix / 8, y = (ar.y + ar.h * iy / 8) / yd;
            uint32_t c;
            if (x < 0 || y < 0 || x >= d->canvas->w || y >= d->canvas->h) {
                continue;
            }
            c = px[(size_t)y * (size_t)d->canvas->w + (size_t)x];
            r += (c >> 16) & 255u;
            g += (c >> 8) & 255u;
            b += c & 255u;
            n++;
        }
    }
    return n == 0u ? ARGB(d->theme.token[SEL_GUI_TOKEN_HEADER_TINT]) : 0xFF000000u | ((r / n) << 16) | ((g / n) << 8) | (b / n);
}

static const char* np_basename(const char* path) {
    const char* slash = strrchr(path, '/');
    return slash != NULL ? slash + 1 : path;
}

static void draw_nowplaying(desk* d) {
    const sel_uim_frame* f = &d->uim.frame;
    const sel_state* st = sel_engine_state(d->engine);
    sel_gui_rect dt = region_rect(d, "detail"), pn = panel_rect(d, "detail");
    int32_t sp = lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_SPACING]);
    uint32_t text = ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT]), muted = ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT_MUTED]);
    uint32_t accent = ARGB(d->theme.token[SEL_GUI_TOKEN_ACCENT]);
    int32_t side = pn.w - 2 * sp, asz, y, rating = 0, liked = 0, ic = lpx(d, 20);
    sel_gui_rect ar, band, hdr;
    uint32_t k, first;
    const char* ctx;
    d->np_title = rect_of(0, 0, 0, 0);
    d->np_add = rect_of(0, 0, 0, 0);
    d->np_like = rect_of(0, 0, 0, 0);
    d->np_ctx_shown[0] = '\0';
    if (dt.w <= 0 || dt.h <= 0) {
        return;
    }
    draw_panel(d, "detail");
    /* I619: the context the song plays from heads the panel (Spotify): the playlist, else the release, else the plain heading. */
    ctx = d->np_ctx;
    if (ctx[0] == '\0' && st->panel != NULL) {
        const char* q = st->panel;
        while (*q != '\0') {
            const char* v = q + strlen(q) + 1;
            if (strcmp(q, "Release") == 0 && v[0] != '\0') {
                ctx = v;
            }
            q = v + strlen(v) + 1;
        }
    }
    hdr = rect_of(pn.x + sp, dt.y + sp, side, lpx(d, 28));
    draw_text(d, hdr, ctx[0] != '\0' ? ctx : "Now Playing", 1, 20, text);
    snprintf(d->np_ctx_shown, sizeof d->np_ctx_shown, "%s", ctx[0] != '\0' ? ctx : "Now Playing");
    /* The artwork spans the full content width between the two insets and stays square, so its height follows the panel
     * width (I443); the text lines below share the same insets (I404). */
    asz = pn.w; /* I619: the artwork is full bleed, the whole panel width, and fades into the panel colour at its bottom */
    ar = rect_of(pn.x, hdr.y + hdr.h + sp, asz, asz);
    d->np_art = ar;
    d->np_panel = pn;
    if (cover_draw(d, ar, st->song.path, 0)) { /* I692: the full-bleed cover; np_art_average below reads the pixels it drew */
        d->cover_np++;
        d->pc_np = 0;
    } else {
        song_platform_cover(d, ar, st->song.path, f->title, &d->pc_np);
        d->np_genart++;
    }
    /* The header tint is the artwork's own average colour blended into the header token, fading to the panel colour. The
     * artwork is drawn once, so the gradient is clipped to the band around it: above the artwork and on both sides. */
    band = rect_of(dt.x, dt.y, dt.w, 2 * sp + asz / 2);
    {
        uint32_t tint = desk_mix(ARGB(d->theme.token[SEL_GUI_TOKEN_HEADER_TINT]), np_art_average(d, ar), 32768);
        sel_gui_rect around[3];
        uint32_t i;
        around[0] = rect_of(band.x, band.y, band.w, ar.y - band.y);
        around[1] = rect_of(band.x, ar.y, ar.x - band.x, band.y + band.h - ar.y);
        around[2] = rect_of(ar.x + ar.w, ar.y, band.x + band.w - ar.x - ar.w, band.y + band.h - ar.y);
        sel_gui_dirty_add(d->canvas, band);
        for (i = 0; i < 3u; i++) {
            if (around[i].w > 0 && around[i].h > 0 && sel_gui_clip_push(d->canvas, around[i])) {
                sel_gui_gradient(d->canvas, band, 1, tint, ARGB(d->theme.token[SEL_GUI_TOKEN_PANEL]));
                sel_gui_clip_pop(d->canvas);
            }
        }
    }
    d->np_fade = 0u;
    if (d->pc_np == 0u) { /* I693b, Johan 2026-10-29: only real art fades into the panel; a platform cover keeps its family word */
        int32_t fh = asz / 3, n = fh / lpx(d, 2) > 0 ? fh / lpx(d, 2) : 1, i; /* the fade: strips of the panel colour, opaque at the bottom */
        uint32_t pc = ARGB(d->theme.token[SEL_GUI_TOKEN_PANEL]) & 0xFFFFFFu;
        d->np_fade = (uint32_t)n;
        for (i = 0; i < n; i++) {
            sel_gui_scrim(d->canvas, rect_of(ar.x, ar.y + ar.h - fh + i * fh / n, ar.w, (i + 1) * fh / n - i * fh / n), pc,
                          (uint32_t)(255 * (i + 1) / n));
        }
    }
    y = ar.y + ar.h + sp;
    {
        int32_t bd = lpx(d, 32), tw = side - bd - sp; /* I619: the round add button sits beside the big title, the title keeps the rest */
        uint32_t lk = 0u;
        if (st->panel != NULL) {
            const char* q = st->panel;
            while (*q != '\0') {
                const char* v = q + strlen(q) + 1;
                lk |= strcmp(q, "Liked") == 0;
                q = v + strlen(v) + 1;
            }
        }
        d->np_add = rect_of(pn.x + sp + side - bd, y, bd, bd);
        hover_reg(d, d->np_add);
        sel_gui_round_rect(d->canvas, d->np_add, bd / 2, hover_ink_of(d, d->np_add, ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT_MUTED]), text));
        sel_gui_round_rect(d->canvas, rect_of(d->np_add.x + lpx(d, 2), d->np_add.y + lpx(d, 2), bd - 2 * lpx(d, 2), bd - 2 * lpx(d, 2)), bd / 2 - lpx(d, 2),
                           ARGB(d->theme.token[SEL_GUI_TOKEN_PANEL]));
        sel_gui_icon_draw(d->canvas, &d->icons, lk ? SEL_GUI_ICON_CHECK : SEL_GUI_ICON_PLUS,
                          rect_of(d->np_add.x + (bd - ic) / 2, d->np_add.y + (bd - ic) / 2, ic, ic), ic, lk ? accent : text);
        draw_text(d, rect_of(pn.x + sp, y, tw, lpx(d, 28)), f->title, 1, 20, text);
    }
    /* No artist: the artist line takes no height, so the like row follows the title (I446). */
    if (f->artist[0] != '\0') {
        draw_text(d, rect_of(pn.x + sp, y + lpx(d, 28), side, lpx(d, 18)), f->artist, 0, 13, muted);
    }
    d->np_title = rect_of(pn.x + sp, y, side - lpx(d, 32) - sp, lpx(d, f->artist[0] != '\0' ? 46 : 28)); /* I619: not under the add button */
    np_title_reg(d);
    /* Like and rating: the library's rating row gives the filled marks; a click sends LIKE or RATE n like the text UI. */
    y += d->np_title.h + lpx(d, 4);
    d->np_like = rect_of(pn.x + sp, y, ic, ic);
    if (st->panel != NULL) {
        const char* p = st->panel;
        while (*p != '\0') {
            const char* v = p + strlen(p) + 1;
            if (strcmp(p, "Rating") == 0) {
                rating = atoi(v);
            } else if (strcmp(p, "Liked") == 0) {
                liked = 1;
            }
            p = v + strlen(v) + 1;
        }
    }
    d->np_rating = rating;
    d->np_inset_l = d->np_like.x - pn.x;
    /* I619: the add button, when drawn, is the title row's right-most content; the title stops short of it. */
    d->np_inset_r = pn.x + pn.w - (d->np_add.w > 0 ? d->np_add.x + d->np_add.w : d->np_title.x + d->np_title.w);
    d->np_liked = liked;
    sel_gui_icon_draw(d->canvas, &d->icons, SEL_GUI_ICON_HEART, d->np_like, ic, row_ink(d, d->np_like, liked ? accent : muted));
    for (k = 0; k < 5u; k++) {
        int32_t m = lpx(d, 12);
        d->np_rate[k] = rect_of(pn.x + sp + ic + lpx(d, 12) + (int32_t)k * (m + lpx(d, 6)), y + (ic - m) / 2, m, m);
        /* A rating mark lights under the pointer (I357): a set mark brightens like an accent fill, an empty one warms to muted. */
        hover_reg(d, d->np_rate[k]);
        sel_gui_round_rect(d->canvas, d->np_rate[k], m / 2,
                           (int32_t)k < rating ? hover_fill_of(d, d->np_rate[k], accent)
                                               : hover_ink_of(d, d->np_rate[k], ARGB(d->theme.token[SEL_GUI_TOKEN_SURFACE_RAISED]), muted));
    }
    y += ic + sp;
    /* The visualizer strip (DUI-P4-11): the selected viz plugin's picture of the newest frame. */
    d->np_viz = rect_of(pn.x + sp, y, side, lpx(d, 72));
    if (st->song.path != NULL && d->np_viz.y + d->np_viz.h < dt.y + dt.h && d->viz_n > 0u) {
        viz_draw_area(d, d->np_viz, d->viz_sel);
        y += d->np_viz.h + sp / 2;
        viz_switch_draw(d, 0u, rect_of(pn.x + sp, y, side, lpx(d, 24)));
        y += lpx(d, 24) + sp;
    } else {
        d->np_viz = rect_of(0, 0, 0, 0);
        viz_switch_draw(d, 0u, rect_of(0, 0, 0, 0));
    }
    /* Metadata pairs. */
    d->np_rows = 0;
    d->np_msg_lines = 0;
    d->np_key_end = 0;
    d->np_val_x = 0;
    if (st->panel != NULL) {
        const char* p = st->panel;
        const sel_gui_atlas* kat = desk_font(d, 0, 11);
        int32_t kw = 0, kmax = side * 2 / 5;
        uint32_t n = 0;
        /* The label column is as wide as the widest label (capped at two fifths of the panel) and the value starts one
         * spacing token after it, so a label and its value stay together on a wide panel (I447). */
        for (; *p != '\0' && n < NP_PAIRS_MAX; n++) {
            int32_t w = kat != NULL ? sel_gui_text_width(kat, p, strlen(p)) : 0;
            kw = w > kw ? w : kw;
            p += strlen(p) + 1;
            p += strlen(p) + 1;
        }
        kw = kw < kmax ? kw : kmax;
        d->np_key_end = pn.x + sp + kw;
        d->np_val_x = d->np_key_end + sp;
        p = st->panel;
        while (*p != '\0' && d->np_rows < NP_PAIRS_MAX) {
            const char* v = p + strlen(p) + 1;
            draw_text(d, rect_of(pn.x + sp, y, kw, lpx(d, 16)), p, 0, 11, muted);
            draw_text(d, rect_of(d->np_val_x, y, pn.x + sp + side - d->np_val_x, lpx(d, 16)), v, 0, 12, text);
            y += lpx(d, 16);
            d->np_rows++;
            p = v + strlen(v) + 1;
        }
    }
    y += sp;
    /* Lyrics: the previous and next lines muted around the current line in the accent colour. */
    if (st->lyric_lines > 0u) {
        draw_text(d, rect_of(pn.x + sp, y, side, lpx(d, 16)), st->lyric_prev != NULL ? st->lyric_prev : "", 0, 12, muted);
        draw_text(d, rect_of(pn.x + sp, y + lpx(d, 18), side, lpx(d, 20)), st->lyric_current != NULL ? st->lyric_current : "", 1, 15, accent);
        draw_text(d, rect_of(pn.x + sp, y + lpx(d, 40), side, lpx(d, 16)), st->lyric_next != NULL ? st->lyric_next : "", 0, 12, muted);
        y += lpx(d, 62);
    } else if (st->lyric_text != NULL) {
        /* A tracker song message is several lines (a MOD's sample names, '\n' apart): laid out as wrapped lines, at most
         * NP_MESSAGE_LINES, so it never reaches the queue below (I290). */
        const sel_gui_atlas* at = desk_font(d, 0, 12);
        if (at != NULL) {
            sel_gui_rect box = rect_of(pn.x + sp, y, side, NP_MESSAGE_LINES * at->line_height);
            sel_gui_clip_push(d->canvas, box);
            d->np_msg_lines = (uint32_t)sel_gui_text_wrap(d->canvas, at, box, st->lyric_text, muted, NP_MESSAGE_LINES, &d->text);
            sel_gui_clip_pop(d->canvas);
            y += (int32_t)d->np_msg_lines * at->line_height + lpx(d, 6);
        }
    }
    /* Up next: the queue entries after the playing one. */
    for (k = 0; k < st->queue_count && st->queue[k].id != st->queue_playing; k++) {
    }
    draw_text(d, rect_of(pn.x + sp, y, side, lpx(d, 16)), "Next in Queue", 1, 11, muted);
    y += lpx(d, 18);
    for (first = ++k; k < st->queue_count && k - first < NP_UPNEXT_MAX && y + lpx(d, 16) < dt.y + dt.h; k++) {
        draw_text(d, rect_of(pn.x + sp, y, side, lpx(d, 16)), np_basename(st->queue_text + st->queue[k].path_offset), 0, 12, text);
        y += lpx(d, 18);
    }
    sel_gui_dirty_add(d->canvas, dt);
    d->np_draws++;
    d->frame_parts++;
}

/* The Now Playing page (I266), after Spotify's Now Playing view: the artwork large on the left, the title and composer,
 * the format, and the queue after the playing song on the right. The visualizer strip stays in the detail panel. */
static void draw_nowplaypage(desk* d) {
    const sel_uim_frame* f = &d->uim.frame;
    const sel_state* st = sel_engine_state(d->engine);
    sel_gui_rect mn = draw_panel(d, "main");
    int32_t mg = lpx(d, HOME_MARGIN), sp = lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_SPACING]);
    int32_t side = mn.h - lpx(d, 96), x, y;
    uint32_t text = ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT]), muted = ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT_MUTED]);
    uint32_t k, first;
    sel_gui_rect ar;
    if (side > mn.w / 2 - mg) {
        side = mn.w / 2 - mg;
    }
    if (side < lpx(d, 80)) {
        side = lpx(d, 80);
    }
    sel_gui_clip_push(d->canvas, mn);
    draw_text(d, rect_of(mn.x + mg, mn.y + lpx(d, 12), mn.w - 2 * mg - lpx(d, 80), lpx(d, page_title_h(d))), "Now Playing", 1, 32, text);
    ar = rect_of(mn.x + mg, mn.y + lpx(d, page_y(d, 64)), side, side);
    if (cover_draw(d, ar, st->song.path, lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_RADIUS_LARGE]))) { /* I692 */
        d->cover_np++;
        d->pc_page = 0;
    } else {
        song_platform_cover(d, ar, st->song.path, f->title, &d->pc_page);
    }
    x = ar.x + ar.w + mg;
    y = ar.y;
    snprintf(d->np_text_title, sizeof d->np_text_title, "%s", st->song.path != NULL ? f->title : "");
    snprintf(d->np_text_artist, sizeof d->np_text_artist, "%s", st->song.path != NULL ? f->artist : "");
    if (st->song.path != NULL) {
        draw_text(d, rect_of(x, y, mn.x + mn.w - x - mg, lpx(d, 40)), f->title, 1, 32, text);
        draw_text(d, rect_of(x, y + lpx(d, 42), mn.x + mn.w - x - mg, lpx(d, 22)), f->artist, 0, 16, muted);
        draw_text(d, rect_of(x, y + lpx(d, 66), mn.x + mn.w - x - mg, lpx(d, 20)), f->format, 0, 13, muted);
    } else {
        draw_text(d, rect_of(x, y, mn.x + mn.w - x - mg, lpx(d, 36)), "Nothing is playing", 1, 22, muted);
    }
    y += lpx(d, 104);
    draw_text(d, rect_of(x, y, mn.x + mn.w - x - mg, lpx(d, 20)), "Next in Queue", 1, 13, muted);
    y += lpx(d, 26);
    for (k = 0; k < st->queue_count && st->queue[k].id != st->queue_playing; k++) {
    }
    for (first = ++k; k < st->queue_count && k - first < 8u && y + lpx(d, 24) < mn.y + mn.h; k++, y += lpx(d, 26) + sp / 4) {
        draw_text(d, rect_of(x, y, mn.x + mn.w - x - mg, lpx(d, 24)), np_basename(st->queue_text + st->queue[k].path_offset), 0, 14, text);
    }
    sel_gui_clip_pop(d->canvas);
    sel_gui_dirty_add(d->canvas, mn);
    d->np_page_draws++;
    d->frame_parts++;
}

/* The transport row (DUI-P4-1): one round button per slot with its icon. Play is the accent button; shuffle and repeat
 * take the accent colour while on, previous and next fade when there is nothing to go to, the repeat button carries a
 * "1" in the repeat-one mode, and the subsong buttons come with an "n/m" label. */
static void draw_transport(desk* d) {
    const sel_uim_frame* f = &d->uim.frame;
    int32_t sp = lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_SPACING]);
    sel_gui_rect first = desk_button_rect(d, SLOT_SUB_PREV);
    sel_gui_rect label = rect_of(first.x - sp - lpx(d, 56), first.y, lpx(d, 56), first.h);
    uint32_t i;
    sel_gui_fill_rect(d->canvas, label, ARGB(d->theme.token[SEL_GUI_TOKEN_FRAME]));
    if (f->subsong_count > 1u) {
        char buf[24];
        snprintf(buf, sizeof buf, "%u/%u", (unsigned)f->subsong, (unsigned)f->subsong_count);
        draw_text(d, label, buf, 0, 11, ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT_MUTED]));
    }
    sel_gui_dirty_add(d->canvas, label);
    for (i = 0; i < TRANSPORT_COUNT; i++) {
        sel_gui_rect b = desk_button_rect(d, i);
        uint32_t icon = SEL_GUI_ICON_PLAY;
        uint32_t fill = d->theme.token[SEL_GUI_TOKEN_FRAME];
        uint32_t ink = d->theme.token[SEL_GUI_TOKEN_TEXT];
        uint32_t off = d->theme.token[SEL_GUI_TOKEN_TEXT_DISABLED];
        uint32_t on = d->theme.token[SEL_GUI_TOKEN_ACCENT];
        uint32_t mute = d->theme.token[SEL_GUI_TOKEN_TEXT_MUTED];
        int32_t isz = lpx(d, 20);
        sel_gui_rect ic;
        if (desk_slot_action(d, i) == NULL) {
            sel_gui_fill_rect(d->canvas, b, ARGB(d->theme.token[SEL_GUI_TOKEN_FRAME]));
            sel_gui_dirty_add(d->canvas, b);
            continue;
        }
        switch (i) {
        case SLOT_SUB_PREV:
            icon = SEL_GUI_ICON_MINUS;
            ink = f->subsong > 1u ? ink : off;
            break;
        case SLOT_SUB_NEXT:
            icon = SEL_GUI_ICON_PLUS;
            ink = f->subsong < f->subsong_count ? ink : off;
            break;
        case SLOT_SHUFFLE:
            icon = SEL_GUI_ICON_SHUFFLE;
            ink = f->random_on ? on : mute;
            break;
        case SLOT_PREV:
            icon = SEL_GUI_ICON_SKIP_BACK;
            ink = f->can_prev ? ink : off;
            break;
        case SLOT_PLAY:
            icon = f->play == SEL_PLAYING ? SEL_GUI_ICON_PAUSE : SEL_GUI_ICON_PLAY;
            fill = d->theme.token[SEL_GUI_TOKEN_TEXT];
            ink = d->theme.token[SEL_GUI_TOKEN_FRAME];
            isz = lpx(d, 24);
            break;
        case SLOT_NEXT:
            icon = SEL_GUI_ICON_SKIP_FORWARD;
            ink = f->can_next ? ink : off;
            break;
        default:
            icon = SEL_GUI_ICON_REPEAT;
            ink = f->repeat_mode != 0u ? on : mute;
            break;
        }
        sel_gui_fill_rect(d->canvas, b, ARGB(d->theme.token[SEL_GUI_TOKEN_FRAME])); /* a redraw must not blend the edges twice */
        hover_reg(d, b); /* a transport button fades its fill like any other control (DUI-P3-8) */
        sel_gui_round_rect(d->canvas, b, i == SLOT_PLAY ? b.h / 2 : lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_RADIUS]),
                           hover_fill_of(d, b, ARGB(fill)));
        ic = rect_of(b.x + (b.w - isz) / 2, b.y + (b.h - isz) / 2, isz, isz);
        sel_gui_icon_draw(d->canvas, &d->icons, icon, ic, isz, ARGB(ink));
        if (i == SLOT_REPEAT && f->repeat_mode == 2u) {
            draw_text(d, rect_of(b.x + b.w - lpx(d, 12), b.y + lpx(d, 2), lpx(d, 10), lpx(d, 12)), "1", 1, 11, ARGB(ink));
        }
        sel_gui_dirty_add(d->canvas, b);
        d->frame_parts++;
    }
}

/* The tooltip box above its button; the fade tween moves the box and the text from the surface to their final
 * colours. */
static void desk_draw_tip(desk* d) {
    sel_gui_rect b, box;
    int32_t door;
    const sel_gui_atlas* at;
    int32_t w, pad, q;
    if (d->tip_shown <= 0) {
        return;
    }
    at = desk_font(d, 0, 13);
    if (at == NULL) {
        return;
    }
    q = sel_gui_tween_value(&d->tl, ANIM_KEY_TIP, 65536);
    door = d->tip_shown - 1 - (int32_t)TRANSPORT_COUNT;
    pad = lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_SPACING]);
    w = sel_gui_text_width(at, d->tip_text, strlen(d->tip_text)) + 2 * pad;
    if (d->tip_shown > TIP_HDR_BASE) { /* I609: the box sits below its header control, right edge kept inside the sidebar */
        b = side_hdr_rect(d, (uint32_t)(d->tip_shown - TIP_HDR_BASE));
        box = rect_of(b.x + b.w - w, b.y + b.h + lpx(d, 4), w, lpx(d, TIP_H));
        if (box.x < 0) {
            box.x = 0;
        }
    } else if (d->tip_shown > TIP_CTL_BASE) { /* I244: the box sits above its control */
        b = desk_ctl_rect(d, (uint32_t)(d->tip_shown - TIP_CTL_BASE - 1));
        box = rect_of(b.x + b.w / 2 - w / 2, b.y - lpx(d, TIP_H + 4), w, lpx(d, TIP_H));
    } else if (door >= (int32_t)DOOR_COUNT) { /* a Library style segment: the box sits below it, centred */
        b = lib_style_rect(d, panel_rect(d, "main"), (uint32_t)(door - (int32_t)DOOR_COUNT));
        box = rect_of(b.x + b.w / 2 - w / 2, b.y + b.h + lpx(d, 4), w, lpx(d, TIP_H));
    } else if (door >= 0) { /* a rail door: the box sits to the right of the rail, level with the door (I363) */
        b = sidebar_door_rect(d, (uint32_t)door);
        box = rect_of(b.x + b.w + lpx(d, 4), b.y + (b.h - lpx(d, TIP_H)) / 2, w, lpx(d, TIP_H));
    } else {
        b = desk_button_rect(d, (uint32_t)d->tip_shown - 1u);
        box = rect_of(b.x + b.w / 2 - w / 2, b.y - lpx(d, TIP_H + 4), w, lpx(d, TIP_H));
    }
    sel_gui_round_rect(d->canvas, box, lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_RADIUS_SMALL]),
                       desk_mix(d->theme.token[SEL_GUI_TOKEN_SURFACE], d->theme.token[SEL_GUI_TOKEN_SURFACE_PRESSED], q));
    draw_text(d, rect_of(box.x + pad, box.y, w - 2 * pad, box.h), d->tip_text, 0, 13,
              desk_mix(d->theme.token[SEL_GUI_TOKEN_SURFACE_PRESSED], d->theme.token[SEL_GUI_TOKEN_TEXT], q));
    sel_gui_dirty_add(d->canvas, box);
}

/* I244: the action-bound controls besides the transport row share its tooltip path. Control c: 0..5 are the visualizer
 * switch segments (slot * 3 + plugin; the action id is the plugin id), 6 is the Now Playing heart ("like"). The tooltip
 * index of control c is TIP_CTL_BASE + c. */
static sel_gui_rect desk_ctl_rect(const desk* d, uint32_t c) {
    if (c == (uint32_t)CTL_LIKE) {
        return d->np_draws > 0u ? d->np_like : rect_of(0, 0, 0, 0);
    }
    if (c == (uint32_t)CTL_NP_ADD) {
        return d->np_draws > 0u ? d->np_add : rect_of(0, 0, 0, 0);
    }
    if (c == (uint32_t)CTL_NP_PANEL) {
        return np_toggle_rect(d);
    }
    return c % 3u < d->viz_n ? d->viz_btn[c / 3u][c % 3u] : rect_of(0, 0, 0, 0);
}

static const char* desk_ctl_action(const desk* d, uint32_t c) {
    if (c == (uint32_t)CTL_NP_ADD) {
        return "np_add";
    }
    if (c == (uint32_t)CTL_NP_PANEL) {
        return "np_panel";
    }
    return c == (uint32_t)CTL_LIKE ? "like" : d->viz_id[c % 3u];
}

/* Tooltip index (1-based within the control block, 0 = none) of the control under a canvas point. */
static int32_t desk_ctl_at(const desk* d, int32_t x, int32_t y) {
    uint32_t c;
    for (c = 0; c < (uint32_t)CTL_COUNT; c++) {
        sel_gui_rect r = desk_ctl_rect(d, c);
        if (r.w > 0 && in_rect(r, x, y)) {
            return (int32_t)c + 1;
        }
    }
    return 0;
}

/* The tooltip-bound controls on screen now: the transport buttons shown plus the other controls drawn (stat tooltip_targets). */
static uint32_t desk_tip_targets(const desk* d) {
    uint32_t i, c, n = 0u;
    for (i = 0; i < TRANSPORT_COUNT; i++) {
        n += desk_slot_action(d, i) != NULL;
    }
    for (c = 0; c < (uint32_t)CTL_COUNT; c++) {
        n += desk_ctl_rect(d, c).w > 0;
    }
    return n;
}

/* The pointer moved to canvas (x, y), or clicked (x < 0): a tooltip that is up goes away when the button changes;
 * the delay restarts at the new button. Returns 1 when the picture changed. */
static int desk_tip_pointer(desk* d, int32_t x, int32_t y) {
    int32_t btn = x < 0 ? 0 : desk_button_at(d, x, y);
    if (btn == 0 && x >= 0 && region_rect(d, "sidebar").w <= lpx(d, 120)) { /* the icon-only rail has no labels: its doors get the tooltip */
        int32_t door = sidebar_door_at(d, x, y);
        btn = door < 0 ? 0 : (int32_t)TRANSPORT_COUNT + 1 + door;
    }
    if (btn == 0 && x >= 0 && d->page == PG_FOLDERS && !d->scan_pending) {
        for (uint32_t i = 0; i < (uint32_t)LIB_STYLES; i++) { /* I390: the chooser segments follow the rail doors in the tooltip index */
            if (in_rect(lib_style_rect(d, panel_rect(d, "main"), i), x, y)) {
                btn = (int32_t)TRANSPORT_COUNT + 1 + (int32_t)DOOR_COUNT + (int32_t)i;
            }
        }
    }
    if (btn == 0 && x >= 0) { /* I609: the Your Library header controls */
        int32_t k = side_hdr_at(d, x, y);
        btn = k > 0 ? TIP_HDR_BASE + k : 0;
    }
    if (btn == 0 && x >= 0) { /* I244: the visualizer switch and the Now Playing heart */
        int32_t c = desk_ctl_at(d, x, y);
        btn = c > 0 ? TIP_CTL_BASE + c : 0;
    }
    int hide = d->tip_shown > 0 && btn != d->tip_shown;
    if (btn != d->tip_btn) {
        d->tip_btn = btn;
        d->tip_since = d->now_ms;
    }
    if (!hide) {
        return 0;
    }
    d->tip_shown = 0;
    d->repaint = 1;
    d->uim.dirty |= SEL_UIM_DIRTY_ALL;
    desk_present(d);
    if (d->home || d->songs) {
        draw_home(d);
        desk_event_frame(d, 1);
    }
    return 1;
}

/* Milliseconds until the pending tooltip is due, 0 when none is pending (or it is due now). */
static uint32_t desk_tip_wait(const desk* d) {
    uint32_t due = d->tip_since + TIP_DELAY_MS;
    return d->tip_btn > 0 && d->tip_shown == 0 && due > d->now_ms ? due - d->now_ms : 0u;
}

/* Shows the tooltip when the pointer has rested long enough. */
static void desk_tip_tick(desk* d) {
    const sel_uim_action* a;
    if (d->tip_btn <= 0 || d->tip_shown > 0 || d->now_ms < d->tip_since + TIP_DELAY_MS) {
        return;
    }
    if (d->tip_btn > TIP_HDR_BASE) { /* I609: a Your Library header control: its full label */
        snprintf(d->tip_text, sizeof d->tip_text, "%s", side_hdr_label(d, (uint32_t)(d->tip_btn - TIP_HDR_BASE)));
    } else if (d->tip_btn > TIP_CTL_BASE) { /* I244: an action-bound control: the action's tooltip text */
        uint32_t c = (uint32_t)(d->tip_btn - TIP_CTL_BASE - 1);
        if (c == (uint32_t)CTL_NP_ADD) { /* I619: the add button reads Add to Liked Songs, or Remove when the song is liked already */
            snprintf(d->tip_text, sizeof d->tip_text, "%s", d->np_liked ? "Remove from Liked Songs" : "Add to Liked Songs");
        } else if (c == (uint32_t)CTL_NP_PANEL) {
            snprintf(d->tip_text, sizeof d->tip_text, "%s", d->detail_hidden ? "Show Now Playing Panel" : "Hide Now Playing Panel");
        } else {
            a = desk_action_by_id(desk_ctl_action(d, c));
            if (a == NULL) {
                return;
            }
            sel_uim_action_tip_text(a, d->tip_text, sizeof d->tip_text);
        }
    } else if (d->tip_btn > (int32_t)(TRANSPORT_COUNT + DOOR_COUNT)) { /* a Library style segment */
        snprintf(d->tip_text, sizeof d->tip_text, "%s", lib_style_name[d->tip_btn - 1 - (int32_t)(TRANSPORT_COUNT + DOOR_COUNT)]);
    } else if (d->tip_btn > (int32_t)TRANSPORT_COUNT) { /* a rail door: its full label */
        snprintf(d->tip_text, sizeof d->tip_text, "%s", sel_uim_door_at((uint32_t)(d->tip_btn - 1 - (int32_t)TRANSPORT_COUNT))->label);
    } else {
        a = desk_action_by_id(desk_slot_action(d, (uint32_t)d->tip_btn - 1u));
        if (a == NULL) {
            return;
        }
        sel_uim_action_tip_text(a, d->tip_text, sizeof d->tip_text);
    }
    d->tip_shown = d->tip_btn;
    d->tooltips++;
    desk_tween(d, ANIM_KEY_TIP, SEL_GUI_ANIM_FAST_MS, SEL_GUI_EASE_OUT);
    desk_draw_tip(d);
    desk_event_frame(d, 1);
}

/* The layout region that holds a control box. */
static int hover_region_of(const desk* d, sel_gui_rect box) {
    if (box.w <= 0) {
        return HR_NONE;
    }
    if (rect_inside(box, region_rect(d, "sidebar"))) {
        return HR_SIDEBAR;
    }
    if (rect_inside(box, region_rect(d, "detail"))) {
        return HR_DETAIL;
    }
    if (rect_inside(box, region_rect(d, "player"))) {
        return HR_PLAYER;
    }
    return rect_inside(box, region_rect(d, "main")) ? HR_MAIN : HR_NONE;
}

/* Repaints a layout region so the fill or ink of a control in it follows its fade (DUI-P3-8). The main region of a
 * Home-mode page is painted by the callers' own draw_home. */
static void hover_paint_region(desk* d, int region) {
    if (region == HR_SIDEBAR) {
        draw_sidebar(d);
    } else if (region == HR_DETAIL) { /* the Now Playing panel: the like heart */
        draw_nowplaying(d);
    } else if (region == HR_PLAYER) { /* the transport buttons are draw_button boxes too */
        draw_transport(d);
    } else if (region == HR_MAIN && !d->home) {
        draw_home(d);
    }
}

/* Repaints the regions of the hovered control and of the one the pointer just left, each once, and no other region. */
static void hover_repaint(desk* d) {
    int a = hover_region_of(d, d->hover_box), b = hover_region_of(d, d->hover_prev);
    hover_paint_region(d, a);
    if (b != a) {
        hover_paint_region(d, b);
    }
    desk_draw_window_overlays(d);
}

/* True when a running tween repaints the main region: a hovered control or tile in it, one the pointer left, or the view or
 * list cross-fade. A fade confined to another region leaves the main region (and its dirty rectangle) alone. */
static int hover_main_painted(const desk* d) {
    return d->hover_tile > 0 || hover_region_of(d, d->hover_box) == HR_MAIN || hover_region_of(d, d->hover_prev) == HR_MAIN ||
           sel_gui_tween_value(&d->tl, ANIM_KEY_VIEW, 65536) < 65536 || sel_gui_tween_value(&d->tl, ANIM_KEY_FLOW, 65536) < 65536 || nav_animating(d);
}

/* The next due time of the three UI timers (tooltip delay, search debounce, toast), 0 when none is pending. One place for
 * the test clock (desk_advance) and the live loop, so a timer cannot be ticked in one and forgotten in the other. */
static uint32_t desk_timers_wait(const desk* d) {
    uint32_t w = desk_tip_wait(d), s = desk_search_wait(d);
    if (s > 0u && (w == 0u || s < w)) {
        w = s;
    }
    s = desk_toast_wait(d);
    if (s > 0u && (w == 0u || s < w)) {
        w = s;
    }
    return w;
}

/* Runs every timer that is due at the current UI clock. Returns 1 when the toast changed and the picture needs a frame. */
static int desk_timers_tick(desk* d) {
    desk_tip_tick(d);
    desk_search_tick(d);
    return desk_toast_tick(d);
}

/* I668: one step of the sidebar width tween, run after the clock moved (test clock and live loop alike): lays the window out
 * at the width the tween has reached and ends the tween on the exact target width. */
static void desk_side_step(desk* d) {
    if (d->side_anim == 0u) {
        return;
    }
    if (sel_gui_tween_value(&d->tl, ANIM_KEY_SIDE, 65536) >= 65536) {
        d->side_anim = 0u;
    }
    d->side_tween_frames++;
    desk_relayout(d);
    d->repaint = 1;
    d->uim.dirty |= SEL_UIM_DIRTY_ALL;
}

/* Moves the UI clock ms forward the way the live loop would: 16 ms steps while a tween runs, otherwise one jump to the
 * next visible-clock second boundary (the idle block). Draws a frame per step that has a reason. */
static void desk_advance(desk* d, uint32_t ms) {
    uint32_t end = d->now_ms + ms;
    while (d->now_ms < end) {
        uint32_t step = end - d->now_ms;
        int animated = sel_gui_timeline_running(&d->tl) > 0u;
        uint32_t tw = desk_timers_wait(d);
        if (animated) {
            if (step > 16u) {
                step = 16u;
            }
        } else if (step > 1000u - d->now_ms % 1000u) {
            step = 1000u - d->now_ms % 1000u;
        }
        if (tw > 0u && step > tw) {
            step = tw;
        }
        int main_moves = animated && d->home && hover_main_painted(d); /* asked before the advance: the step that ends a tween still paints it */
        d->now_ms += step;
        sel_gui_timeline_advance(&d->tl, d->now_ms);
        desk_side_step(d);
        sel_uim_update(&d->uim, sel_engine_state(d->engine));
        desk_timers_tick(d);
        if (!desk_should_draw(d, animated)) {
            continue;
        }
        if (main_moves) {
            draw_home(d);
        }
        if (animated) {
            hover_repaint(d);
            desk_draw_tip(d);
        }
        if (d->toast_on && animated) { /* the toast slides and fades with the tween: one full frame per step */
            if (!overlay_refresh(d)) {
                desk_event_frame(d, 1);
            }
            continue;
        }
        if (!desk_present(d)) {
            desk_event_frame(d, 1);
        }
    }
}

/* Focus stops (DUI-P3-3): the layout regions in order, then the search field. */
static int32_t desk_stops(const desk* d) {
    return (int32_t)d->layout.count + 1;
}

/* Moves the focus ring one stop (dir +1 Tab, -1 Shift+Tab); the whole canvas repaints so the old ring is gone. */
static void desk_focus_move(desk* d, int32_t dir) {
    int32_t n = desk_stops(d);
    d->focus = d->focus < 0 ? (dir > 0 ? 0 : n - 1) : (d->focus + dir + n) % n;
    d->focus_moves++;
    d->repaint = 1;
    d->uim.dirty |= SEL_UIM_DIRTY_ALL;
}

/* The ring of the focused stop: two logical pixels in the focus-ring token. The search stop is the field at the top
 * of the main region. */
static void desk_draw_focus(desk* d) {
    sel_gui_rect r;
    int32_t t = lpx(d, 2), rad = lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_RADIUS_LARGE]);
    uint32_t col = ARGB(d->theme.token[SEL_GUI_TOKEN_FOCUS_RING]);
    if (d->focus < 0 || d->layout.count == 0u) {
        return;
    }
    if ((uint32_t)d->focus < d->layout.count) {
        const sel_uim_region* g = &d->layout.regions[d->focus];
        r = rect_of(lpx(d, g->x), lpx(d, g->y), lpx(d, (int32_t)g->w), lpx(d, (int32_t)g->h));
    } else {
        sel_gui_rect f = search_field_rect(d, panel_rect(d, "main")); /* the ring hugs the pill: the field grown by the ring */
        r = rect_of(f.x - t, f.y - t, f.w + 2 * t, f.h + 2 * t);
        rad += t;
        d->ring = r;
    }
    if (r.w <= 2 * t || r.h <= 2 * t) {
        return;
    }
    if ((uint32_t)d->focus < d->layout.count) {
        sel_gui_fill_rect(d->canvas, rect_of(r.x, r.y, r.w, t), col);
        sel_gui_fill_rect(d->canvas, rect_of(r.x, r.y + r.h - t, r.w, t), col);
        sel_gui_fill_rect(d->canvas, rect_of(r.x, r.y, t, r.h), col);
        sel_gui_fill_rect(d->canvas, rect_of(r.x + r.w - t, r.y, t, r.h), col);
    } else {
        sel_gui_round_frame(d->canvas, r, rad, t, col);
    }
    sel_gui_dirty_add(d->canvas, r);
}

/* Sends one protocol line to the engine; 1 when it parsed. */
static int desk_send(desk* d, const char* line) {
    /* A decoded string argument points into the pool and the engine reads it on its next pump, after this returns: the
     * pool outlives the call, a ring so several lines queued before one pump keep their text. */
    static char ring[16][512];
    static uint32_t next;
    d->sends++;
    char* pool = ring[next++ % 16u];
    return sel_engine_send_line(d->engine, line, pool, sizeof ring[0]) == SEL_CODEC_OK;
}

/* A click on transport slot i fires the action the button names: its table line, or for the subsong buttons the line
 * SUBSONG with the neighbouring absolute index. Returns 1 when the engine took the line. */
static int desk_transport_click(desk* d, uint32_t i) {
    const sel_uim_action* a = desk_action_by_id(desk_slot_action(d, i));
    const sel_uim_frame* f = &d->uim.frame;
    char line[32];
    if (a == NULL) {
        return 0;
    }
    if (i == SLOT_SUB_PREV || i == SLOT_SUB_NEXT) {
        uint32_t to = i == SLOT_SUB_PREV ? f->subsong - 2u : f->subsong;
        if (i == SLOT_SUB_PREV ? f->subsong < 2u : f->subsong >= f->subsong_count) {
            return 0;
        }
        snprintf(line, sizeof line, "SUBSONG %u", (unsigned)to);
    } else {
        snprintf(line, sizeof line, "%s", a->line);
    }
    if (!desk_send(d, line)) {
        return 0;
    }
    d->transport_hits++;
    d->actions_fired++;
    return 1;
}

/* The keys of the action table become the desktop bindings, loaded through the same txt_bindings the text UI uses. */
/* The text UI's page keys and the page each opens: the one table both the key load and the key dispatch read (I279). */
static const struct {
    const char* action;
    desk_page page;
} k_page_keys[] = {{"@home", PG_HOME},       {"@library", PG_FOLDERS}, {"@folders", PG_PLAYLISTS},
                   {"@info", PG_SONGINFO},   {"@viz", PG_VIZ},         {"@pattern", PG_TRACKER}};

static void desk_bind_actions(desk* d) {
    uint32_t i, n = sel_uim_action_count();
    memset(&d->binds, 0, sizeof d->binds);
    for (i = 0; i < n; i++) {
        const sel_uim_action* a = sel_uim_action_at(i);
        char row[160];
        if (a->key[0] == '\0' || strcmp(a->key, "M-Left") == 0 || strcmp(a->key, "M-Right") == 0) {
            continue; /* Alt+Left and Alt+Right are the browser history keys on the desktop, not the seek keys */
        }
        snprintf(row, sizeof row, "%s %s\n", a->key, a->line);
        txt_bindings_load(&d->binds, row, NULL);
    }
    { /* The overlay keys (DUI-P4-13) are the text UI's: its default table names them, so each key lives in one place. */
        static const char* const overlay_actions[] = {"@command", "@palette", "@help", "@back"};
        txt_bindings def;
        uint32_t k;
        txt_bindings_default(&def);
        for (k = 0; k < sizeof overlay_actions / sizeof overlay_actions[0] + sizeof k_page_keys / sizeof k_page_keys[0]; k++) {
            const char* action = k < sizeof overlay_actions / sizeof overlay_actions[0] ? overlay_actions[k] : k_page_keys[k - sizeof overlay_actions / sizeof overlay_actions[0]].action;
            char key[24], row[64];
            if (txt_bindings_key_for(&def, action, key, sizeof key) != NULL) {
                snprintf(row, sizeof row, "%s %s\n", key, action);
                txt_bindings_load(&d->binds, row, NULL);
            }
        }
        txt_bindings_load(&d->binds, "M-Left @back\nM-Right @forward\n", NULL); /* the browser keys: Alt+Left and Alt+Right */
    }
}

/* Overlays (DUI-P4-13): the command palette, the shortcut help and the command line share the text UI's models
 * (txt_palette, txt_cmdline), so the ranking, the completion and the key names exist once. Toasts report a pick. */
enum { OV_NONE, OV_PALETTE, OV_HELP, OV_CMDLINE };
enum { OV_W = 560, OV_ROW_H = 28, OV_ROWS = 10, OV_HEAD_H = 40, OV_TOP = 72, TOAST_MS = 2500, TOAST_SLIDE = 12, CMD_BAR_H = 36 };

static sel_gui_rect overlay_box(const desk* d) {
    int32_t w = lpx(d, OV_W) < d->canvas->w ? lpx(d, OV_W) : d->canvas->w;
    return rect_of((d->canvas->w - w) / 2, lpx(d, OV_TOP), w, lpx(d, OV_HEAD_H) + (int32_t)OV_ROWS * lpx(d, OV_ROW_H) + lpx(d, 8));
}

static uint32_t overlay_first(const desk* d) {
    return d->pal.cursor >= (uint32_t)OV_ROWS ? d->pal.cursor - (uint32_t)OV_ROWS + 1u : 0u;
}

static sel_gui_rect overlay_row(const desk* d, uint32_t i) {
    sel_gui_rect b = overlay_box(d);
    return rect_of(b.x + lpx(d, 8), b.y + lpx(d, OV_HEAD_H) + (int32_t)i * lpx(d, OV_ROW_H), b.w - lpx(d, 16), lpx(d, OV_ROW_H));
}

/* I442: the toast sits in the bottom right corner of the app window, 16 px in from both edges. */
static sel_gui_rect toast_rect(desk* d) {
    return rect_of(d->canvas->w - lpx(d, 320 + 16), cvh(d) - lpx(d, 40 + 16), lpx(d, 320), lpx(d, 40));
}

static void desk_draw_overlay(desk* d) {
    uint32_t tx = ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT]), mu = ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT_MUTED]);
    int32_t rad = lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_RADIUS]);
    sel_gui_rect b;
    if (d->overlay == OV_PALETTE || d->overlay == OV_HELP) {
        uint32_t i, first = overlay_first(d);
        sel_gui_rect hd;
        b = overlay_box(d);
        sel_gui_round_rect(d->canvas, b, rad, ARGB(d->theme.token[SEL_GUI_TOKEN_SURFACE_RAISED]));
        hd = rect_of(b.x + lpx(d, 16), b.y, b.w - lpx(d, 32), lpx(d, OV_HEAD_H));
        draw_text(d, hd, d->pal.len > 0u ? d->pal.query : d->overlay == OV_PALETTE ? "Search commands" : "Keyboard shortcuts", 1, 15,
                  d->pal.len > 0u ? tx : mu);
        sel_gui_fill_rect(d->canvas, rect_of(b.x, b.y + lpx(d, OV_HEAD_H) - 1, b.w, 1), ARGB(d->theme.token[SEL_GUI_TOKEN_DIVIDER]));
        if (d->pal.count == 0u) {
            draw_text(d, overlay_row(d, 0), "No matching command", 0, 13, mu);
        }
        for (i = 0; i < (uint32_t)OV_ROWS && first + i < d->pal.count; i++) {
            const txt_palette_item* it = &d->pal.items[first + i];
            sel_gui_rect r = overlay_row(d, i);
            int hot = first + i == d->pal.cursor;
            int32_t kw = it->key[0] != '\0' ? sel_gui_text_width(desk_font(d, 0, 12), it->key, strlen(it->key)) : 0;
            if (hot) {
                sel_gui_round_rect(d->canvas, r, lpx(d, (int32_t)d->theme.token[SEL_GUI_TOKEN_RADIUS_SMALL]),
                                   ARGB(d->theme.token[SEL_GUI_TOKEN_ACCENT]));
            }
            draw_text(d, rect_of(r.x + lpx(d, 8), r.y, r.w / 2, r.h), it->name, 0, 13, hot ? ARGB(d->theme.token[SEL_GUI_TOKEN_ACCENT_TEXT]) : tx);
            draw_text(d, rect_of(r.x + r.w / 2, r.y, r.w / 2 - kw - lpx(d, 24), r.h), txt_palette_area_title(it->area), 0, 12,
                      hot ? ARGB(d->theme.token[SEL_GUI_TOKEN_ACCENT_TEXT]) : mu);
            if (kw > 0) {
                draw_text(d, rect_of(r.x + r.w - kw - lpx(d, 8), r.y, kw + lpx(d, 8), r.h), it->key, 0, 12,
                          hot ? ARGB(d->theme.token[SEL_GUI_TOKEN_ACCENT_TEXT]) : mu);
            }
        }
        sel_gui_dirty_add(d->canvas, b);
    } else if (d->overlay == OV_CMDLINE) {
        sel_gui_rect mn = region_rect(d, "main");
        char line[TXT_CMD_LINE + 4];
        b = rect_of(mn.x, mn.y + mn.h - lpx(d, CMD_BAR_H), mn.w, lpx(d, CMD_BAR_H));
        sel_gui_fill_rect(d->canvas, b, ARGB(d->theme.token[SEL_GUI_TOKEN_SURFACE_RAISED]));
        snprintf(line, sizeof line, ":%s_", d->cmd.text);
        draw_text(d, rect_of(b.x + lpx(d, 16), b.y, b.w - lpx(d, 32), b.h), line, 0, 14, tx);
        sel_gui_dirty_add(d->canvas, b);
        if (d->cmd.status[0] != '\0') {
            sel_gui_rect s = rect_of(mn.x, b.y - lpx(d, 28), mn.w, lpx(d, 28));
            sel_gui_fill_rect(d->canvas, s, ARGB(d->theme.token[SEL_GUI_TOKEN_SURFACE]));
            draw_text(d, rect_of(s.x + lpx(d, 16), s.y, s.w - lpx(d, 32), s.h), d->cmd.status, 0, 12, mu);
            sel_gui_dirty_add(d->canvas, s);
        }
    }
    if (d->toast_on) {
        int32_t q = sel_gui_tween_value(&d->tl, ANIM_KEY_TOAST, 65536); /* 0 hidden .. 65536 shown */
        uint32_t fill, ink;
        if (d->toast_out) {
            q = 65536 - q;
        }
        b = toast_rect(d);
        b.y += (int32_t)(((int64_t)(65536 - q) * lpx(d, TOAST_SLIDE)) >> 16); /* slides up into place, back down on the way out */
        fill = desk_mix(d->theme.token[SEL_GUI_TOKEN_SURFACE], d->theme.token[SEL_GUI_TOKEN_ACCENT], q);
        ink = desk_mix(d->theme.token[SEL_GUI_TOKEN_SURFACE], d->theme.token[SEL_GUI_TOKEN_ACCENT_TEXT], q);
        /* I442: no drop shadow; it read as a heavy black outline around the toast. */
        sel_gui_round_rect(d->canvas, b, rad, fill);
        draw_text(d, rect_of(b.x + lpx(d, 16), b.y, b.w - lpx(d, 32), b.h), d->toast_text, 1, 13, ink);
        sel_gui_dirty_add(d->canvas, rect_of(b.x, b.y, b.w, b.h + lpx(d, TOAST_SLIDE)));
    }
}

/* Repaints the whole canvas so a closed overlay leaves no pixels behind. */
static int overlay_refresh(desk* d) {
    d->repaint = 1;
    d->uim.dirty |= SEL_UIM_DIRTY_ALL;
    return desk_present(d);
}

static void desk_toast(desk* d, const char* text) {
    snprintf(d->toast_text, sizeof d->toast_text, "%s", text);
    d->toast_on = 1;
    d->toast_out = 0;
    d->toast_until = d->now_ms + TOAST_MS;
    d->toasts++;
    desk_tween(d, ANIM_KEY_TOAST, SEL_GUI_ANIM_FAST_MS, SEL_GUI_EASE_OUT);
    overlay_refresh(d);
}

static uint32_t desk_toast_wait(const desk* d) {
    return d->toast_on && d->toast_until > d->now_ms ? d->toast_until - d->now_ms : 0u;
}

/* A toast that has been up TOAST_MS fades out over the fast tween, then goes (with no motion it goes at once). Returns 1
 * when the toast left the screen, so the caller repaints the whole canvas. */
static int desk_toast_tick(desk* d) {
    if (!d->toast_on || d->now_ms < d->toast_until) {
        return 0;
    }
    if (!d->toast_out && desk_tween(d, ANIM_KEY_TOAST, SEL_GUI_ANIM_FAST_MS, SEL_GUI_EASE_OUT)) {
        d->toast_out = 1;
        d->toast_until = d->now_ms + SEL_GUI_ANIM_FAST_MS;
        return 0;
    }
    d->toast_on = 0;
    d->toast_out = 0;
    d->repaint = 1;
    d->uim.dirty |= SEL_UIM_DIRTY_ALL;
    return 1;
}

/* A verb that needs an argument cannot run bare: its signature has no trailing '?' or '*' covering every letter. */
static int verb_needs_args(const char* verb) {
    uint32_t id;
    const char* sig;
    size_t n;
    if (!sel_cmd_by_name(verb, &id)) {
        return 0;
    }
    sig = sel_cmd_args(id);
    n = strlen(sig);
    if (n > 0u && sig[n - 1u] == '?') {
        return n > 2u;
    }
    if (n > 0u && sig[n - 1u] == '*') {
        return n > 1u;
    }
    return n > 0u;
}

static void overlay_close(desk* d) {
    d->overlay = OV_NONE;
    overlay_refresh(d);
}

/* Enter on a palette row: a bare verb runs, a verb with arguments opens the command line with the verb typed. */
static void palette_pick(desk* d) {
    const char* verb = txt_palette_picked(&d->pal);
    char line[64];
    if (verb == NULL) {
        return;
    }
    if (verb_needs_args(verb)) {
        txt_cmdline_open(&d->cmd);
        snprintf(d->cmd.text, sizeof d->cmd.text, "%s ", verb);
        d->cmd.len = (uint32_t)strlen(d->cmd.text);
        d->overlay = OV_CMDLINE;
        d->cmdline_opens++;
        overlay_refresh(d);
        return;
    }
    snprintf(line, sizeof line, "%s", verb);
    d->palette_runs++;
    snprintf(d->last_verb, sizeof d->last_verb, "%s", verb);
    d->overlay = OV_NONE;
    if (desk_send(d, line)) {
        d->overlay_sends_ok++;
        desk_toast(d, "Command sent");
    } else {
        desk_toast(d, "Not a command");
    }
}

/* A key while an overlay is open: it owns the keyboard. */
static int overlay_key(desk* d, const sel_gui_input* ev) {
    if (d->overlay == OV_CMDLINE) {
        txt_cmd_result r = txt_cmdline_key(&d->cmd, ev->code, ev->mods);
        if (r == TXT_CMD_SUBMIT) {
            const char* line = txt_cmdline_text(&d->cmd);
            d->overlay = OV_NONE;
            if (line[0] != '\0') {
                char copy[TXT_CMD_LINE];
                snprintf(copy, sizeof copy, "%s", line);
                d->cmdline_runs++;
                snprintf(d->last_verb, sizeof d->last_verb, "%s", copy);
                if (desk_send(d, copy)) {
                    d->overlay_sends_ok++;
                    desk_toast(d, "Command sent");
                    return 1;
                }
                desk_toast(d, "Not a command");
                return 1;
            }
            return overlay_refresh(d);
        }
        if (r == TXT_CMD_CANCEL) {
            overlay_close(d);
            return 1;
        }
        return overlay_refresh(d);
    } else {
        txt_pal_result r = txt_palette_key(&d->pal, ev->code, ev->mods);
        if (r == TXT_PAL_CANCEL || (r == TXT_PAL_RUN && d->overlay == OV_HELP)) {
            overlay_close(d);
            return 1;
        }
        if (r == TXT_PAL_RUN) {
            palette_pick(d);
            return 1;
        }
        return overlay_refresh(d);
    }
}

/* A click while an overlay is open: a palette row picks, anything else closes. */
static int overlay_click(desk* d, const sel_gui_input* ev) {
    if (d->overlay == OV_PALETTE) {
        uint32_t i, first = overlay_first(d);
        for (i = 0; i < (uint32_t)OV_ROWS && first + i < d->pal.count; i++) {
            if (in_rect(overlay_row(d, i), ev->x, ev->y)) {
                d->pal.cursor = first + i;
                palette_pick(d);
                return 1;
            }
        }
    }
    overlay_close(d);
    return 1;
}

/* A key event (DUI-P3-3). Order: the search field takes printable keys while focused; Tab moves the focus; `/` focuses
 * the search; a bound key fires its action; Up and Down move the list cursor. Returns 1 when the picture changed. */
/* Ctrl+Plus, Ctrl+Minus and Ctrl+0 step the UI Zoom through 75, 100, 125, 150 and back to the backend default (I458).
 * The change lasts for the session (there is no host pref write path yet, ledger row I459). Returns 1 when the key was
 * a zoom key. */
static int desk_zoom_key(desk* d, const sel_gui_input* ev) {
    static const int32_t steps[] = {75, 100, 125, 150};
    int32_t cur = d->zoom_pct == 0 ? 100 : d->zoom_pct, next = d->zoom_pct;
    int i, at = 1;
    if ((ev->mods & TXT_MOD_CTRL) == 0u || (ev->code != '+' && ev->code != '=' && ev->code != '-' && ev->code != '0')) {
        return 0;
    }
    for (i = 0; i < 4; i++) {
        if (steps[i] == cur) {
            at = i;
        }
    }
    if (ev->code == '0') {
        next = d->zoom_default;
    } else if (ev->code == '-') {
        next = steps[at > 0 ? at - 1 : 0];
    } else {
        next = steps[at < (d->baked_zoom ? 1 : 3) ? at + 1 : (d->baked_zoom ? 1 : 3)]; /* I587: 75 and 100 are the baked scales */
    }
    d->zoom_pct = next;
    if (desk_set_scale(d, desk_scale_for(d, d->canvas->w, cvh(d)))) {
        desk_resize(d, d->canvas->w, d->canvas->h);
    }
    return 1;
}

/* Shared focus movement of the desktop lists (DUI-P3-3, I423, I498). */
enum { LIST_NAV_NONE = -2 };
/* The focus row after a list key: Down, Up, Page Down, Page Up, Home, End, clamped to 0..n-1. With no focus row
 * (cur < 0) the first Down lands on row 0 and the first Up stays on it (I505). LIST_NAV_NONE when the key is not a list key. */
static int32_t list_nav(uint32_t code, int32_t cur, int32_t n, int32_t page) {
    int32_t to = cur < 0 ? -1 : cur;
    if (code == TXT_KEY_DOWN) {
        to += 1;
    } else if (code == TXT_KEY_UP) {
        to -= 1;
    } else if (code == TXT_KEY_PAGE_DOWN) {
        to += page;
    } else if (code == TXT_KEY_PAGE_UP) {
        to -= page;
    } else if (code == TXT_KEY_HOME) {
        to = 0;
    } else if (code == TXT_KEY_END) {
        to = n - 1;
    } else {
        return LIST_NAV_NONE;
    }
    return to < 0 ? 0 : (to > n - 1 ? n - 1 : to);
}

/* The scroll offset (Q16 logical pixels) that brings the row at top..top+h inside a view of view_h pixels. */
static int32_t list_scroll_into(int32_t q16, int32_t top, int32_t h, int32_t view_h) {
    if (top < q16 >> 16) {
        return top << 16;
    }
    if (top + h > (q16 >> 16) + view_h) {
        return (top + h - view_h) << 16;
    }
    return q16;
}

static int songs_play_row(desk* d, int32_t row);

/* List keys of the songs page (DUI-P3-3, I423): Up and Down move the focus row, Page Up and Page Down a page, Home and End
 * the ends; Enter plays the focus row through songs_play_row, as a double-click on it does. The row scrolls into view.
 * Returns 1 when the key was a list key. */
static int songs_list_key(desk* d, const sel_gui_input* ev) {
    sel_gui_rect mn = region_rect(d, "main");
    int32_t view_h = px_to_logical(d, mn.y + mn.h - songs_top(d) - lpx(d, SONG_HEADER_H));
    int32_t page = view_h / SONG_ROW_H > 1 ? view_h / SONG_ROW_H - 1 : 1, last = (int32_t)d->order_n - 1, to, max_q16;
    if (ev->mods != 0u || d->order_n == 0u) {
        return 0;
    }
    if (ev->code == 13u) {
        if (d->row_focus < 0 || d->row_focus > last) {
            return 0;
        }
        d->kb_activates++;
        d->row_plays++;
        songs_play_row(d, d->row_focus);
        return 1;
    }
    to = list_nav(ev->code, d->row_focus, (int32_t)d->order_n, page);
    if (to == LIST_NAV_NONE) {
        return 0;
    }
    d->row_focus = to;
    max_q16 = (last + 1) * SONG_ROW_H - view_h;
    max_q16 = max_q16 < 0 ? 0 : max_q16 << 16;
    d->scroll_q16 = list_scroll_into(d->scroll_q16, d->row_focus * SONG_ROW_H, SONG_ROW_H, view_h);
    d->scroll_q16 = d->scroll_q16 > max_q16 ? max_q16 : d->scroll_q16;
    d->arrows++;
    draw_home(d);
    return 1;
}

/* Queue page list keys (I498): the focus row skips the album group dividers; Enter plays it as a double-click on it does. */
static int queue_list_key(desk* d, const sel_gui_input* ev) {
    const sel_state* st = sel_engine_state(d->engine);
    sel_gui_rect mn = panel_rect(d, "main");
    int32_t view_h = px_to_logical(d, queue_list_rect(d, mn).h), n = (int32_t)st->queue_count, to, step;
    int32_t page = view_h / QUEUE_ROW_H > 1 ? view_h / QUEUE_ROW_H - 1 : 1;
    if (ev->mods != 0u || n == 0) {
        return 0;
    }
    if (ev->code == 13u) {
        if (d->q_focus < 0 || d->q_focus >= n || (st->queue[d->q_focus].flags & SEL_QUEUE_FLAG_DIVIDER) != 0u) {
            return 0;
        }
        d->kb_activates++;
        d->row_plays++;
        desk_play_path(d, st->queue_text + st->queue[d->q_focus].path_offset);
        return 1;
    }
    to = list_nav(ev->code, d->q_focus, n, page);
    if (to == LIST_NAV_NONE) {
        return 0;
    }
    step = to >= d->q_focus ? 1 : -1;
    while (to >= 0 && to < n && (st->queue[to].flags & SEL_QUEUE_FLAG_DIVIDER) != 0u) {
        to += step;
    }
    if (to < 0 || to >= n) {
        return 1; /* only dividers beyond: the focus stays */
    }
    d->q_focus = to;
    queue_groups_sync(d);
    d->queue_q16 = list_scroll_into(d->queue_q16, (int32_t)queue_slot(d, (uint32_t)to) * QUEUE_ROW_H, QUEUE_ROW_H, view_h);
    d->arrows++;
    draw_home(d);
    return 1;
}

/* The entry idx of the UI-drawn Library list (a composer: canon, display, count; a drilldown song: key, name), NULL past the
 * end; *total gets the entry count. */
static const char* lib_ui_row(const sel_state* st, int32_t idx, int32_t* total) {
    const char* p = lib_rows_first(st), *hit = NULL;
    int32_t i = 0;
    for (; p != NULL && *p != '\0'; i++) {
        if (i == idx) {
            hit = p;
        }
        p = lib_rows_next(st, p);
    }
    *total = i;
    return hit;
}

/* Library composer list and drilldown list keys (I498): Enter opens the composer or plays the song, as a click on it does. */
static int lib_ui_key(desk* d, const sel_gui_input* ev) {
    const sel_state* st = sel_engine_state(d->engine);
    sel_gui_rect mn = panel_rect(d, "main");
    int32_t view_h = px_to_logical(d, mn.y + mn.h - lib_list_top(d, mn, st)), n, to, max_q16;
    const char* p;
    (void)lib_ui_row(st, 0, &n);
    if (ev->mods != 0u || n == 0) {
        return 0;
    }
    if (ev->code == 13u) {
        p = d->lib_focus >= 0 ? lib_ui_row(st, d->lib_focus, &n) : NULL;
        if (p == NULL) {
            return 0;
        }
        d->kb_activates++;
        desk_composer_open(d, p, lib_next(p)); /* I678f: Enter opens the composer's collection page */
        return 1;
    }
    to = list_nav(ev->code, d->lib_focus, n, view_h / LIB_ROW_H > 1 ? view_h / LIB_ROW_H - 1 : 1);
    if (to == LIST_NAV_NONE) {
        return 0;
    }
    d->lib_focus = to;
    max_q16 = n * LIB_ROW_H - view_h;
    max_q16 = max_q16 < 0 ? 0 : max_q16 << 16;
    d->lib_q16 = list_scroll_into(d->lib_q16, to * LIB_ROW_H, LIB_ROW_H, view_h);
    d->lib_q16 = d->lib_q16 > max_q16 ? max_q16 : d->lib_q16;
    d->arrows++;
    draw_home(d);
    return 1;
}

/* Playlists page list keys (I498): the focus row is the selected row; Enter opens the selected playlist as the Open button does. */
static int pl_list_key(desk* d, const sel_gui_input* ev) {
    const sel_view_state* v = pl_view(sel_engine_state(d->engine));
    int32_t sel = pl_sel_row(d, v), to;
    if (ev->mods != 0u || v == NULL || v->row_n == 0u || d->dlg_open || d->pls_id != 0u) {
        return 0;
    }
    if (ev->code == 13u) {
        if (sel < 0) {
            return 0;
        }
        d->kb_activates++;
        return pl_button_click(d, v, 2u);
    }
    to = list_nav(ev->code, sel, (int32_t)v->row_n, 5);
    if (to == LIST_NAV_NONE) {
        return 0;
    }
    d->pl_sel_kind = v->rows[to].kind;
    d->pl_sel_id = v->rows[to].id;
    d->arrows++;
    draw_home(d);
    return 1;
}

/* Collection page list keys (I678f2): the song rows of an album, artist, composer, Liked Songs, Recently Played, Local Files or source
 * page. Up and Down move the focus, Home and End the ends, Enter plays the focus row through pls_play_pos, as a click on it does.
 * The row scrolls into view (pls_scroll is a slot index). Returns 1 when the key was a list key. */
static int col_list_key(desk* d, const sel_gui_input* ev) {
    sel_gui_rect mn = region_rect(d, "main");
    int32_t n = (int32_t)col_song_rows(d), to, vis = 0;
    if (ev->mods != 0u || d->col_kind == 0u || d->pls_id == 0u || d->dlg_open || d->col_search || n == 0) {
        return 0;
    }
    while (pls_row_visible(d, mn, (uint32_t)vis)) {
        vis++;
    }
    if (ev->code == 13u) {
        if (d->col_focus < 0 || d->col_focus >= n) {
            return 0;
        }
        d->kb_activates++;
        return pls_play_pos(d, (uint32_t)d->col_focus);
    }
    to = list_nav(ev->code, d->col_focus, n, vis > 1 ? vis - 1 : 1);
    if (to == LIST_NAV_NONE) {
        return 0;
    }
    d->col_focus = to;
    if (to < (int32_t)d->pls_scroll) {
        d->pls_scroll = (uint32_t)to;
    } else if (vis > 0 && to >= (int32_t)d->pls_scroll + vis) {
        d->pls_scroll = (uint32_t)(to - vis + 1);
    }
    d->arrows++;
    draw_home(d);
    return 1;
}

/* The first Down on an engine-cursor list (Library tree, search results) focuses the absolute row 0 instead of moving the
 * cursor on from wherever it stood (I505). Returns 1 when it sent that request. */
static int view_first_down(desk* d, const sel_gui_input* ev) {
    if (d->kb_list || ev->code != TXT_KEY_DOWN) {
        return 0;
    }
    d->kb_list = 1;
    desk_send(d, "VIEW_CURSOR 0");
    d->arrows++;
    return 1;
}

static int desk_key(desk* d, const sel_gui_input* ev) {
    const char* act;
    d->keys_in++;
    if (d->quit_req) { /* the Quit Selecta dialog was confirmed: the loop is ending, later keys do nothing (I623) */
        return 1;
    }
    if (desk_zoom_key(d, ev)) {
        return 1;
    }
    if (d->dlg_open) { /* the confirm dialog owns the keyboard */
        return dlg_key(d, ev);
    }
    if (d->stats_open && ev->code == 27u) { /* Escape collapses the Library statistics panel (I455) */
        lib_stats_toggle(d, 0);
        return 1;
    }
    if (d->overlay != OV_NONE) { /* an open overlay owns the keyboard */
        return overlay_key(d, ev);
    }
    if (ev->code == TXT_KEY_F1 + 9u && (ev->mods & TXT_MOD_SHIFT) != 0u && song_menu_key(d)) {
        return 1;
    }
    if (d->lf_edit) { /* I611: the Your Library filter field has the keyboard */
        return lf_key(d, ev);
    }
    if (d->pl_page && d->col_search) { /* I678e: the collection's search field has the keyboard */
        return col_key(d, ev);
    }
    if (d->pl_page && d->pl_edit) { /* the name field of the Playlists page has the keyboard */
        return pl_key(d, ev);
    }
    if (select_key(d, ev)) {
        return 1;
    }
    if (d->pl_page && d->pls_id != 0u && ev->code == 27u && ev->mods == 0u && !d->sm_open) { /* I633: Escape leaves the playlist page */
        pls_close(d);
        return 1;
    }
    if (d->si_page && d->si_edit) { /* the note field of the Song Information page has the keyboard */
        return si_key(d, ev);
    }
    if (ev->code == 27u && ev->mods == 0u && d->sm_open) { /* Escape closes the open song menu */
        d->sm_open = 0;
        draw_home(d);
        return 1;
    }
    if (ev->code == 27u && ev->mods == 0u && !(d->pl_page && d->pl_edit) && !(d->si_page && d->si_edit) &&
        d->focus != (int32_t)d->layout.count) { /* Escape with nothing open asks whether to quit (I623) */
        dlg_open_quit(d);
        draw_home(d);
        return 1;
    }
    if (d->focus == (int32_t)d->layout.count) { /* the search field is focused */
        if (ev->code == 27u) {
            d->search_pending = 0; /* Escape leaves without a search */
            d->focus = 0;
        } else if (ev->code == 13u) { /* Enter searches at once, unless the debounce already sent this query */
            if (d->search_pending) {
                desk_search_fire(d);
            }
            d->focus = 0;
        } else if (ev->code == 127u) {
            if (d->qlen > 0u) {
                d->query[--d->qlen] = '\0';
                d->search_pending = 1;
                d->search_since = d->now_ms;
            }
        } else if (ev->code >= 32u && ev->code < 127u && (ev->mods & (TXT_MOD_CTRL | TXT_MOD_ALT)) == 0u) {
            if (d->qlen + 1u < sizeof d->query) {
                d->query[d->qlen++] = (char)ev->code;
                d->query[d->qlen < sizeof d->query ? d->qlen : sizeof d->query - 1u] = '\0';
                d->search_pending = 1;
                d->search_since = d->now_ms;
            }
        } else {
            return 0;
        }
        d->repaint = 1;
        d->uim.dirty |= SEL_UIM_DIRTY_ALL;
        return desk_present(d);
    }
    if (ev->code == 9u && (ev->mods & ~TXT_MOD_SHIFT) == 0u) {
        desk_focus_move(d, (ev->mods & TXT_MOD_SHIFT) != 0u ? -1 : 1);
        return desk_present(d);
    }
    if (ev->code == '/' && ev->mods == 0u) {
        return desk_page_goto(d, PG_SEARCH);
    }
    if (ev->code == 'o' && ev->mods == 0u && desk_bitmap_text(d)) { /* I611: the bitmap tier has no Sort by menu: O cycles the Your Library sort like a click on the label */
        lsort_prefs_load(d);
        d->lib_sort_keys++;
        side_sort_pick(d, (d->side_sort + 1u) % SEL_UIM_LSORTS);
        d->repaint = 1;
        d->uim.dirty |= SEL_UIM_DIRTY_ALL;
        return desk_present(d);
    }
    if (d->songs && songs_list_key(d, ev)) {
        return 1;
    }
    if ((d->queue_page && queue_list_key(d, ev)) || (d->pl_page && (col_list_key(d, ev) || pl_list_key(d, ev))) ||
        (d->page == PG_FOLDERS && sel_engine_state(d->engine)->composers != NULL && lib_ui_list(d, sel_engine_state(d->engine)) && !lib_grid_on(sel_engine_state(d->engine)) && lib_ui_key(d, ev))) {
        return 1;
    }
    if (d->page == PG_HOME && ev->mods == 0u && d->open_tile == 0 && !d->songs && home_shelf_mode(sel_engine_state(d->engine)) &&
        (ev->code == 13u || ev->code == TXT_KEY_UP || ev->code == TXT_KEY_DOWN)) { /* I617: the engine cursor walks the Home rows; Enter on a shelf header opens the shelf, the bitmap tier's Show all */
        if (ev->code == 13u && shelf_header_enter(d)) {
            return 1;
        }
        if (view_first_down(d, ev)) {
            return 0;
        }
        d->kb_list = 1;
    }
    if (d->page == PG_SEARCH && ev->mods == 0u && sel_engine_state(d->engine)->view != NULL &&
        sel_engine_state(d->engine)->view->source == SEL_VIEW_SEARCH && (ev->code == TXT_KEY_UP || ev->code == TXT_KEY_DOWN || ev->code == 13u)) {
        const sel_view_state* sv = sel_engine_state(d->engine)->view;
        const sel_view_row* sr = sv->cursor >= sv->window_start && sv->cursor - sv->window_start < sv->row_n ? &sv->rows[sv->cursor - sv->window_start] : NULL;
        if (view_first_down(d, ev)) {
            return 0;
        }
        d->kb_list = 1; /* the engine cursor is the focus row (I498) */
        if (ev->code == 13u && sr != NULL && sr->kind != (uint32_t)SEL_ROW_HEADING && !sr->album && sr->key != NULL) {
            d->kb_activates++;
            d->row_plays++;
            return desk_play_path(d, sr->key);
        }
        d->kb_activates += ev->code == 13u;
    }
    if (d->page == PG_FOLDERS && ev->mods == 0u && lib_view_on(d, sel_engine_state(d->engine)) &&
        (ev->code == TXT_KEY_UP || ev->code == TXT_KEY_DOWN || ev->code == 13u)) { /* the engine cursor is the focus row (I423) */
        if (view_first_down(d, ev)) {
            return 0;
        }
        d->kb_list = 1;
        d->kb_activates += ev->code == 13u;
    }
    if (ev->mods == 0u && lib_grid_on(sel_engine_state(d->engine)) &&
        (ev->code == TXT_KEY_LEFT || ev->code == TXT_KEY_RIGHT || ev->code == TXT_KEY_UP || ev->code == TXT_KEY_DOWN)) {
        const sel_view_state* v = sel_engine_state(d->engine)->view;
        int flow = v->style == (uint32_t)SEL_VIEW_STYLE_COVERFLOW;
        uint32_t by = 1u;
        char line[32];
        if (flow && (ev->code == TXT_KEY_UP || ev->code == TXT_KEY_DOWN)) {
            return 0;
        }
        d->kb_list = 1; /* the engine cursor tile is the focus tile (I498) */
        if (!flow && (ev->code == TXT_KEY_UP || ev->code == TXT_KEY_DOWN)) {
            by = lib_grid_cols(d, region_rect(d, "main")); /* a row of tiles */
        }
        snprintf(line, sizeof line, "VIEW_CURSOR %c%u", ev->code == TXT_KEY_LEFT || ev->code == TXT_KEY_UP ? '-' : '+', (unsigned)by);
        d->flow_from = flow_centre_fx(d, v->cursor, sel_gui_tween_value(&d->tl, ANIM_KEY_FLOW, 65536)); /* a key mid-glide starts where the covers are (I400) */
        if (flow) {
            desk_tween(d, ANIM_KEY_FLOW, SEL_GUI_ANIM_NORMAL_MS, SEL_GUI_EASE_OUT);
        }
        desk_send(d, line);
        d->arrows++;
        return 0;
    }
    if (ev->mods == 0u && ev->code == 13u && lib_grid_on(sel_engine_state(d->engine))) { /* Enter opens the cursor tile through the bound VIEW_ACTIVATE (I498) */
        d->kb_list = 1;
        d->kb_activates++;
    }
    act = txt_bindings_lookup(&d->binds, TXT_CTX_ANY, ev->code, ev->mods);
    if (act != NULL && strcmp(act, "@command") == 0) {
        txt_cmdline_open(&d->cmd);
        d->overlay = OV_CMDLINE;
        d->cmdline_opens++;
        return overlay_refresh(d);
    }
    if (act != NULL && (strcmp(act, "@palette") == 0 || strcmp(act, "@help") == 0)) {
        txt_palette_open(&d->pal, &d->binds);
        d->overlay = act[1] == 'p' ? OV_PALETTE : OV_HELP;
        d->palette_opens += d->overlay == OV_PALETTE;
        d->help_opens += d->overlay == OV_HELP;
        return overlay_refresh(d);
    }
    if (act != NULL && (strcmp(act, "@back") == 0 || strcmp(act, "@forward") == 0)) { /* page history (I266) */
        d->nav_keys++;
        return desk_page_walk(d, act[1] == 'b' ? -1 : 1);
    }
    if (act != NULL) { /* the text UI's page keys open the matching page (I262, DUI-P4-11) */
        uint32_t k;
        for (k = 0; k < sizeof k_page_keys / sizeof k_page_keys[0]; k++) {
            if (strcmp(act, k_page_keys[k].action) == 0) {
                d->nav_keys++;
                return desk_page_goto(d, k_page_keys[k].page);
            }
        }
    }
    if (act != NULL) {
        uint32_t i, n = sel_uim_action_count();
        for (i = 0; i < n; i++) {
            const sel_uim_action* a = sel_uim_action_at(i);
            if (strcmp(a->line, act) == 0) {
                if (desk_send(d, a->line)) {
                    d->actions_fired++;
                    if ((d->fired_seen[i / 32u] & (1u << (i % 32u))) == 0u) {
                        d->fired_seen[i / 32u] |= 1u << (i % 32u);
                        d->actions_distinct++;
                    }
                }
                break;
            }
        }
        return 0;
    }
    if (ev->code == TXT_KEY_UP || ev->code == TXT_KEY_DOWN) { /* arrow navigation in lists */
        desk_send(d, ev->code == TXT_KEY_UP ? "VIEW_CURSOR -1" : "VIEW_CURSOR +1");
        d->arrows++;
    }
    return 0;
}

/* Replays the --input script once:`resize W H`, `click X Y`, `tile N` (the centre of tile N, 0-based), `#` comments. */
/* The display row under the canvas point, or -1. */
static int32_t songs_row_at(desk* d, int32_t x, int32_t y) {
    uint32_t i;
    for (i = 0; i < d->order_n; i++) {
        if (in_rect(songs_row_rect(d, i), x, y)) {
            return (int32_t)i;
        }
    }
    return -1;
}

/* Sends `<verb> "<path>"` for a display row. */
static int songs_send_path(desk* d, const char* verb, int32_t row) {
    const sel_state* st = sel_engine_state(d->engine);
    return desk_send_path(d, verb, st->queue_text + st->queue[d->order[row]].path_offset);
}

/* Plays a display row: OPEN only loads, so the play intent sends OPEN then PLAY. */
static int songs_play_row(desk* d, int32_t row) {
    const sel_state* st = sel_engine_state(d->engine);
    return desk_play_path(d, st->queue_text + st->queue[d->order[row]].path_offset);
}

/* One pointer event (DUI-P3-2): the region is found on the layout tree first, then the part inside it. Returns 1 when
 * the picture changed. */
/* A press on a footer bar (I312): a click sets the value under the pointer, a drag that starts on the bar sets the
 * value where it is released. The bars are 4 pixels tall, so the hit band is 16 logical pixels high around the bar.
 * Returns 1 when the event was on a bar, 0 otherwise (the caller keeps routing it). */
static int footer_bar_pointer(desk* d, const sel_gui_input* ev) {
    sel_gui_rect bars[2] = {vol_bar_rect(d), seek_bar_rect(d)};
    int32_t x = ev->kind == SEL_GUI_IN_DRAG ? ev->x2 : ev->x;
    char line[32];
    if (!(ev->kind == SEL_GUI_IN_CLICK && ev->button == 0u) && ev->kind != SEL_GUI_IN_DRAG) {
        return 0;
    }
    for (uint32_t k = 0; k < 2u; k++) {
        sel_gui_rect b = bars[k];
        int32_t band = lpx(d, 16);
        int64_t at;
        if (b.w <= 0 || !in_rect(rect_of(b.x, b.y + b.h / 2 - band / 2, b.w, band), ev->x, ev->y)) {
            continue;
        }
        at = x < b.x ? 0 : x > b.x + b.w ? b.w : x - b.x;
        if (k == 0u) {
            snprintf(line, sizeof line, "VOLUME %u", (unsigned)(at * 100 / b.w));
            d->vol_clicks++;
        } else {
            uint32_t dur = sel_engine_state(d->engine)->song.duration_ms;
            if (dur == 0u) { /* no length known: nothing to seek to, but the press stays on the bar */
                return 1;
            }
            snprintf(line, sizeof line, "SEEK %u", (unsigned)(at * dur / b.w));
            d->seek_clicks++;
        }
        d->clicks++;
        desk_send(d, line);
        return 1;
    }
    return 0;
}

/* ---- I632d: multi-select. The model is d->sel (src/uimodel/select.c); the registry rows carry list kind, id, version and index. */
static int select_active(const desk* d, sel_uim_list_id* l, uint32_t* rows) {
    const sel_state* st = sel_engine_state(d->engine);
    uint32_t i;
    for (i = 0; i < d->sm_n; i++) {
        const song_ref* r = &d->sm_rows[i].ref;
        if (r->list_kind == 0u) {
            continue;
        }
        l->kind = r->list_kind;
        l->id = r->list_id;
        l->version = r->list_version;
        *rows = r->list_kind == (uint32_t)SEL_UIM_LIST_QUEUE ? st->queue_count
              : r->list_kind == (uint32_t)SEL_UIM_LIST_PLAYLIST ? d->pls_n
              : r->list_id >= 1000u ? lib_list_n(st) : (st->view != NULL ? st->view->row_count : 0u);
        return 1;
    }
    return 0;
}

/* After each paint: the list on screen is the list of the selection. A scroll keeps its version; a sort, filter, search, drill
 * or page change moves it, and the model clears. */
static void select_sync(desk* d) {
    sel_uim_list_id l;
    uint32_t rows;
    if (select_active(d, &l, &rows)) {
        sel_uim_selset_bind(&d->sel, l);
    } else {
        sel_uim_selset_clear(&d->sel);
    }
}

static int select_pointer(desk* d, const sel_gui_input* ev) {
    uint32_t i;
    if (ev->kind != SEL_GUI_IN_CLICK || ev->button != 0u || add_mode(d)) {
        return 0;
    }
    for (i = 0; i < d->sm_n; i++) {
        const song_ref* r = &d->sm_rows[i].ref;
        sel_uim_list_id l;
        if (r->list_kind == 0u || !in_rect(d->sm_rows[i].r, ev->x, ev->y)) {
            continue;
        }
        l.kind = r->list_kind;
        l.id = r->list_id;
        l.version = r->list_version;
        sel_uim_selset_bind(&d->sel, l);
        d->sel_clicks++;
        d->sel_cur = r->list_index;
        if ((ev->mods & TXT_MOD_CTRL) != 0u) {
            sel_uim_selset_toggle(&d->sel, r->list_index);
        } else if ((ev->mods & TXT_MOD_SHIFT) != 0u) {
            sel_uim_selset_extend(&d->sel, r->list_index);
        } else {
            sel_uim_selset_select_one(&d->sel, r->list_index);
            return 0; /* a plain click goes on to focus and play as before */
        }
        draw_home(d);
        return 1;
    }
    return 0;
}

static int desk_key(desk* d, const sel_gui_input* ev);

static int select_key(desk* d, const sel_gui_input* ev) {
    sel_uim_list_id l;
    uint32_t rows;
    const sel_state* st = sel_engine_state(d->engine);
    if (ev->kind != SEL_GUI_IN_KEY || d->sm_open) {
        return 0;
    }
    if (ev->code == 27u && ev->mods == 0u && sel_uim_selset_count(&d->sel) > 0u) { /* Escape clears the selection before it leaves a page */
        sel_uim_selset_clear(&d->sel);
        d->sel_keys++;
        draw_home(d);
        return 1;
    }
    if (d->focus == (int32_t)d->layout.count || (d->pl_page && d->pl_edit) || (d->si_page && d->si_edit) || !select_active(d, &l, &rows) || rows == 0u) {
        return 0;
    }
    if ((ev->code == 'a' || ev->code == 'A') && ev->mods == TXT_MOD_CTRL) { /* Select All (Command on macOS) */
        sel_uim_selset_bind(&d->sel, l);
        sel_uim_selset_select_all(&d->sel, rows);
        d->sel_cur = rows - 1u;
        d->sel_keys++;
        draw_home(d);
        return 1;
    }
    if (ev->mods == TXT_MOD_SHIFT && (ev->code == TXT_KEY_UP || ev->code == TXT_KEY_DOWN)) { /* Shift+arrow extends from the anchor */
        sel_gui_input plain = *ev;
        uint32_t next;
        sel_uim_selset_bind(&d->sel, l);
        if (sel_uim_selset_count(&d->sel) == 0u) {
            int32_t f = l.kind == (uint32_t)SEL_UIM_LIST_QUEUE ? d->q_focus : l.kind == (uint32_t)SEL_UIM_LIST_PLAYLIST ? 0
                      : l.id >= 1000u ? d->lib_focus : (st->view != NULL ? (int32_t)st->view->cursor : 0);
            d->sel_cur = f < 0 ? 0u : (uint32_t)f;
            sel_uim_selset_select_one(&d->sel, d->sel_cur);
        }
        next = ev->code == TXT_KEY_DOWN ? (d->sel_cur + 1u < rows ? d->sel_cur + 1u : rows - 1u) : (d->sel_cur > 0u ? d->sel_cur - 1u : 0u);
        sel_uim_selset_extend(&d->sel, next);
        d->sel_cur = next;
        d->sel_keys++;
        plain.mods = 0u; /* the list moves its focus and scrolls as for a plain arrow */
        desk_key(d, &plain);
        return 1;
    }
    return 0;
}

static int desk_pointer_view(desk* d, const sel_gui_input* ev) {
    int region = sel_uim_hit(&d->layout, px_to_logical(d, ev->x), px_to_logical(d, ev->y));
    const char* rname = region >= 0 ? d->layout.regions[region].name : "";
    int in_main = strcmp(rname, "main") == 0;
    if (dlg_pointer(d, ev)) {
        return 1;
    }
    if (d->overlay != OV_NONE) { /* a click with an overlay open picks a palette row or closes it */
        return ev->kind == SEL_GUI_IN_CLICK ? overlay_click(d, ev) : 0;
    }
    if (song_menu_pointer(d, ev)) {
        return 1;
    }
    if (select_pointer(d, ev)) {
        return 1;
    }
    if (plmenu_pointer(d, ev)) {
        return 1;
    }
    if (footer_bar_pointer(d, ev)) {
        return 1;
    }
    if (ev->kind == SEL_GUI_IN_CLICK && ev->button == 0u) {
        int32_t btn = desk_button_at(d, ev->x, ev->y);
        if (btn > 0) {
            d->clicks++;
            return desk_transport_click(d, (uint32_t)btn - 1u);
        }
    }
    if (ev->kind == SEL_GUI_IN_CLICK && ev->button == 0u && in_main &&
        (in_rect(d->nav_back, ev->x, ev->y) || in_rect(d->nav_fwd, ev->x, ev->y))) { /* the history arrows */
        d->nav_arrow_clicks++;
        return desk_page_walk(d, in_rect(d->nav_back, ev->x, ev->y) ? -1 : 1);
    }
    if (ev->kind == SEL_GUI_IN_CLICK && ev->button == 0u && topbar_click(d, ev->x, ev->y)) { /* I615 */
        return 1;
    }
    if (ev->kind == SEL_GUI_IN_CLICK && ev->button == 0u) { /* a sidebar door opens its page */
        int32_t door = sidebar_door_at(d, ev->x, ev->y);
        if (d->lf_edit && side_tools_at(d, ev->x, ev->y) != 2 && tl_at(d, lib_tools_rect(d, region_rect(d, "main")), ev->x, ev->y) != 2) { /* I611: a click elsewhere takes the keyboard from the filter field */
            d->lf_edit = 0;
        }
        if (door >= 0) {
            d->nav_clicks++;
            return desk_route_door(d, door_page((uint32_t)door));
        }
        if (in_rect(side_hdr_title_rect(d), ev->x, ev->y)) { /* I678h (D1): the "Your Library" title opens nothing; the route table counts the click */
            d->nav_clicks++;
            desk_route(d, SEL_ROUTE_HEADER_TITLE, 0u, 0u, "", "", "");
            return 1;
        }
        door = side_hdr_at(d, ev->x, ev->y);
        if (door == SIDE_HDR_PLUS) { /* I609: the header plus */
            d->side_plus_clicks++;
            return side_create_playlist(d);
        }
        if (door == SIDE_HDR_WIDE) { /* I609: widen or narrow the sidebar through the layout */
            d->side_wide_clicks++;
            side_wide_toggle(d);
            d->repaint = 1;
            d->uim.dirty |= SEL_UIM_DIRTY_ALL;
            return 1;
        }
        if (d->side_empty_btn.w > 0 && in_rect(d->side_empty_btn, ev->x, ev->y)) { /* I627: the empty-state button */
            d->side_create_clicks++;
            return side_create_playlist(d);
        }
        door = side_tools_at(d, ev->x, ev->y); /* I611: magnifier, filter field, sort label */
        if (door >= 0) {
            tl_click(d, side_tools_rect(d), door);
            return 1;
        }
        if (d->side_sub_btn.w > 0 && in_rect(d->side_sub_btn, ev->x, ev->y)) { /* I678d: the second-level chip */
            d->side_sub = d->side_sub == 1u ? 0u : 1u;
            d->side_sub_clicks++;
            d->side_y_q16 = 0;
            d->side_stale = 1u;
            d->repaint = 1;
            d->uim.dirty |= SEL_UIM_DIRTY_ALL;
            return 1;
        }
        door = side_chip_at(d, ev->x, ev->y); /* I610: a library chip or chevron */
        if (door >= 0) {
            side_chip_click(d, door);
            desk_route(d, SEL_ROUTE_CHIP, 0u, 0u, "", "", ""); /* I678h2: a chip opens nothing; the route table counts the click (section 9) */
            return 1;
        }
        door = side_lib_at(d, ev->x, ev->y); /* I608: a library row opens that item in the main region */
        if (door >= 0) {
            d->side_opens++;
            if (d->side_items[door].kind == (uint32_t)SEL_LIB_ITEM_PLAYLIST) {
                char touch[160];
                char head[32];
                snprintf(head, sizeof head, "COLLECTION_OPEN %u", (unsigned)d->side_items[door].kind); /* I678c/e: opening a playlist feeds Recents; the engine stores it so every UI shares the order */
                desk_quoted_request(touch, sizeof touch, head, d->side_items[door].key);
                desk_send(d, touch);
            }
            /* I678h: the route table opens the page (a playlist's page of songs, I633; every other kind's collection page, I678f/I678g) */
            desk_route(d, SEL_ROUTE_SIDEBAR_ROW, d->side_items[door].kind, d->side_items[door].id, d->side_items[door].key, d->side_items[door].name,
                       d->side_items[door].sub);
            d->repaint = 1;
            d->uim.dirty |= SEL_UIM_DIRTY_ALL;
            return 1;
        }
    }
    if ((in_main || (ev->kind == SEL_GUI_IN_MOVE && ev->code == 1u)) && lib_bar_pointer(d, ev)) { /* I541: the Library scrollbar */
        return 1;
    }
    if (ev->kind == SEL_GUI_IN_CLICK && ev->button == 0u && in_main && library_click(d, ev->x, ev->y)) { /* the Library page */
        d->clicks++;
        return 1;
    }
    if (ev->kind == SEL_GUI_IN_CLICK && ev->button == 0u) { /* the visualizer switch */
        int32_t vb = viz_switch_at(d, ev->x, ev->y);
        if (vb >= 0) {
            return viz_switch_click(d, vb);
        }
    }
    if (ev->kind == SEL_GUI_IN_CLICK && ev->button == 0u && d->np_draws > 0u && d->np_add.w > 0 && in_rect(d->np_add, ev->x, ev->y)) {
        d->clicks++; /* I619: the add button works on every page, the Playlists page included */
        d->np_add_clicks++;
        return desk_send(d, "LIKE");
    }
    if (d->queue_page) {
        return in_main ? queue_pointer(d, ev) : 0;
    }
    if (d->pl_page) {
        return in_main ? pl_pointer(d, ev) : 0;
    }
    if (d->si_page) {
        return in_main ? si_pointer(d, ev) : 0;
    }
    if (d->tr_page) {
        return in_main ? tr_pointer(d, ev) : 0;
    }
    if (!d->songs) {
        if (ev->kind == SEL_GUI_IN_CLICK && ev->button == 0u) {
            d->clicks++;
            return desk_click(d, ev->x, ev->y);
        }
        return 0;
    }
    if (d->order_n != sel_engine_state(d->engine)->queue_count) {
        songs_resort(d);
    }
    if (ev->kind == SEL_GUI_IN_MOVE) { /* I448: a held primary button over a song row shows the drop targets */
        int was = d->songs_drag >= 0;
        d->songs_drag = ev->code == 1u ? songs_row_at(d, ev->x2, ev->y2) : -1;
        if (d->songs_drag >= 0 || was) {
            draw_home(d);
            return 1;
        }
        return 0;
    }
    if (ev->kind == SEL_GUI_IN_DRAG || ev->kind == SEL_GUI_IN_CLICK) {
        int was = d->songs_drag >= 0;
        d->songs_drag = -1; /* the release hides the targets */
        if (was) {
            draw_home(d);
        }
    }
    if (ev->kind == SEL_GUI_IN_DRAG) {
        int32_t row = in_main ? songs_row_at(d, ev->x, ev->y) : -1;
        int end = sel_uim_hit(&d->layout, px_to_logical(d, ev->x2), px_to_logical(d, ev->y2));
        if (row < 0 || end < 0 || strcmp(d->layout.regions[end].name, "main") != 0) {
            return 0;
        }
        if (in_rect(songs_target_rect(d, 0), ev->x2, ev->y2)) { /* drop on Queue: the dragged song is queued again */
            d->drags++;
            return songs_send_path(d, "QUEUE_ADD", row);
        }
        for (uint32_t t = 1; t <= SONG_PLAYLIST_TARGETS; t++) { /* drop on a saved playlist: the song is appended to it (I241) */
            const char* name;
            uint32_t pid = playlist_nth(sel_engine_state(d->engine), t - 1u, &name);
            if (pid != 0u && in_rect(songs_target_rect(d, t), ev->x2, ev->y2)) {
                const sel_state* st = sel_engine_state(d->engine);
                char line[340];
                d->drags++;
                snprintf(line, sizeof line, "PLAYLIST_ADD_SONG %u \"%s\"", (unsigned)pid, st->queue_text + st->queue[d->order[row]].path_offset);
                return desk_send(d, line);
            }
        }
        return 0;
    }
    if (ev->kind != SEL_GUI_IN_CLICK) {
        return 0;
    }
    d->clicks++;
    if (in_main) {
        uint32_t c;
        int32_t row = songs_row_at(d, ev->x, ev->y);
        for (c = 0; c < 2u; c++) {
            if (in_rect(songs_header_col(d, c), ev->x, ev->y)) { /* header: ascending first, then toggles */
                if (d->sort_col == c && d->sort_dir == 1u) {
                    d->sort_dir = 2u;
                } else {
                    d->sort_col = c;
                    d->sort_dir = 1u;
                }
                d->sorts++;
                songs_resort(d);
                return 1;
            }
        }
        if (row >= 0 && ev->clicks >= 2u) { /* double-click plays the row */
            d->double_clicks++;
            d->row_plays++;
            songs_play_row(d, row);
            return 1;
        }
    }
    return 0;
}

/* The pointer moved to (x, y) (I272): finds the listed control under it (button, chip, tab, sidebar door) and the Home tile
 * under it, and repaints at once when either changed (no tween: I356, I359). Returns 1 when the picture changed. */
static int desk_hover_move(desk* d, int32_t x, int32_t y) {
    sel_gui_rect hit = rect_of(0, 0, 0, 0);
    int32_t tile = 0;
    const sel_state* st = sel_engine_state(d->engine);
    uint32_t i;
    int changed = 0, tile_moved = 0;
    uint32_t shelf1 = 0u;
    if (d->overlay == OV_NONE) {
        for (i = 0; i < d->hbox_n; i++) {
            if (in_rect(d->hbox[i], x, y)) {
                hit = d->hbox[i];
            }
        }
        if ((d->page == PG_HOME || d->page == PG_SHELF) && d->home && d->open_tile == 0 && !d->songs && !home_empty(st)) {
            sel_gui_rect mn = region_rect(d, "main");
            if (!in_rect(mn, x, y)) {
                tile = 0;
            } else if (home_shelf_mode(st)) {
                shelf sh[SHELF_MAX];
                uint32_t s, k, cnt = home_shelves(st, sh), flat = 0;
                shelf1 = shelf_page_on(d) ? 0u : shelf_hover_of(d, x, y); /* I607 */
                if (quick_hit(d, x, y, &k)) { /* I490 */
                    tile = (int32_t)quick_hover_index(st, k) + 1;
                } else if (shelf_hit(d, x, y, &s, &k)) {
                    for (i = 0; i < s && i < cnt; i++) {
                        flat += sh[i].tiles;
                    }
                    tile = (int32_t)(flat + k) + 1;
                }
            } else {
                for (i = 0; i < st->queue_count; i++) {
                    if (in_rect(tile_rect(d, i), x, y)) {
                        tile = (int32_t)i + 1;
                        break;
                    }
                }
            }
        }
    }
    if (!rect_same(hit, d->hover_box) && !(hit.w <= 0 && d->hover_box.w <= 0)) {
        d->hover_prev = d->hover_box;
        d->hover_box = hit;
        changed = 1;
    }
    if (tile != d->hover_tile) {
        tile_moved = 1;
        if (!rect_same(d->hover_prev, d->hover_box)) { /* a tile-only change restarts the tween: no box is fading out */
            d->hover_prev = rect_of(0, 0, 0, 0);
        }
        d->hover_tile = tile;
        changed = 1;
    }
    if (shelf_nav_pointer(d, x, y, shelf1)) { /* I607, I630: the buttons follow the cover row under the pointer */
        tile_moved = 1;
        changed = 1;
    }
    if (!changed) {
        return 0;
    }
    d->hovers++; /* no tween: hover switches at once, so the move frame is the only frame (I359) */
    d->hover_frame = 1;
    if (d->home && (tile_moved || hover_main_painted(d))) {
        draw_home(d);
    }
    hover_repaint(d);
    return 1;
}

/* A pointer event: the view handles it, and the tooltip tracker follows the transport buttons (DUI-P3-6). */
static int desk_pointer(desk* d, const sel_gui_input* ev) {
    int tip;
    int hover = 0;
    if (ev->kind == SEL_GUI_IN_MOVE) {
        d->ptr_x = ev->x;
        d->ptr_y = ev->y;
        hover = desk_hover_move(d, ev->x, ev->y);
    }
    tip = desk_tip_pointer(d, ev->kind == SEL_GUI_IN_MOVE ? ev->x : -1, ev->y);
    return desk_pointer_view(d, ev) | tip | hover;
}

/* Touchpad events (DUI-P3-4). Wheel scrolls the songs list by Q16 logical pixels (negative dy scrolls down), clamped to
 * the list; pinch in the tile grid moves the tile size one token step (clamped); swipe is Back and Forward. Returns 1
 * when the picture changed. */
static int desk_gesture(desk* d, const sel_gui_input* ev) {
    if (ev->kind == SEL_GUI_IN_WHEEL) {
        if (side_lib_max_q16(d) > 0 && in_rect(side_lib_view_rect(d), d->ptr_x, d->ptr_y)) { /* I608: a sidebar list longer than its view scrolls under the pointer */
            int32_t max_q16 = side_lib_max_q16(d);
            int64_t v = (int64_t)d->side_y_q16 - (int64_t)ev->dy * SCROLL_NOTCH;
            d->side_y_q16 = v < 0 ? 0 : (v > max_q16 ? max_q16 : (int32_t)v);
            d->repaint = 1;
            d->uim.dirty |= SEL_UIM_DIRTY_ALL;
            return 1;
        }
        if (d->pl_page && d->pls_id != 0u && ev->dy != 0) { /* I633: the playlist's songs scroll a row a notch */
            if (ev->dy < 0 && d->pls_scroll + 1u < col_rows(d)) {
                d->pls_scroll++;
            } else if (ev->dy > 0 && d->pls_scroll > 0u) {
                d->pls_scroll--;
            }
            d->repaint = 1;
            d->uim.dirty |= SEL_UIM_DIRTY_ALL;
            return 1;
        }
        if (d->si_page) { /* I298: the Song Information list scrolls when its rows overflow the panel */
            int32_t max_q16 = si_max_q16(d);
            int64_t v = (int64_t)d->si_y_q16 - (int64_t)ev->dy * SCROLL_NOTCH;
            d->si_y_q16 = v < 0 ? 0 : (v > max_q16 ? max_q16 : (int32_t)v);
            d->uim.dirty |= SEL_UIM_DIRTY_ALL;
            return 1;
        }
        if (d->page == PG_FOLDERS && sel_engine_state(d->engine)->composers != NULL) {
            return lib_wheel(d, ev->dy);
        }
        if (d->page == PG_QUEUE && d->queue_page) { /* I258: the Queue page scrolls as the songs list does */
            sel_gui_rect mn = region_rect(d, "main");
            int32_t max_q16;
            int64_t v;
            queue_groups_sync(d);
            max_q16 = queue_max_q16(d, mn, sel_engine_state(d->engine));
            v = (int64_t)d->queue_q16 - (int64_t)ev->dy * SCROLL_NOTCH;
            d->queue_q16 = v < 0 ? 0 : (v > max_q16 ? max_q16 : (int32_t)v);
            draw_home(d);
            return 1;
        }
        if (d->songs) {
            sel_gui_rect mn = region_rect(d, "main");
            int32_t view_h = px_to_logical(d, mn.y + mn.h - songs_top(d) - lpx(d, SONG_HEADER_H));
            int32_t max_q16 = ((int32_t)d->order_n * SONG_ROW_H - view_h) << 16;
            int64_t v = (int64_t)d->scroll_q16 - (int64_t)ev->dy * SCROLL_NOTCH;
            if (max_q16 < 0) {
                max_q16 = 0;
            }
            d->scroll_q16 = v < 0 ? 0 : (v > max_q16 ? max_q16 : (int32_t)v);
            draw_home(d);
            return 1;
        }
        if (d->home && d->open_tile == 0 && home_shelf_mode(sel_engine_state(d->engine))) { /* the shelf under the pointer */
            /* Rule (I607): the wheel never scrolls a shelf sideways, a horizontal swipe included; the vertical wheel
             * scrolls the whole Home content wherever the pointer is. A shelf moves only by its previous/next buttons. */
            home_scroll_y(d, -ev->dy);
            shelf_nav_pointer(d, d->ptr_x, d->ptr_y, shelf_hover_of(d, d->ptr_x, d->ptr_y)); /* the content moved under a still pointer */
            d->sm_open = 0;
            draw_home(d);
            return 1;
        }
        return 0;
    }
    if (ev->kind == SEL_GUI_IN_PINCH) {
        if (!d->home || d->songs || d->open_tile > 0) {
            return 0;
        }
        d->gestures++;
        if (ev->scale > SEL_GUI_Q16_ONE && d->tile_step + 1u < TILE_STEPS) {
            d->tile_step++;
        } else if (ev->scale < SEL_GUI_Q16_ONE && d->tile_step > 0u) {
            d->tile_step--;
        }
        draw_home(d);
        return 1;
    }
    if (ev->kind == SEL_GUI_IN_SWIPE) {
        d->gestures++;
        if (view_swipe(d, ev->x)) {
            desk_tween(d, ANIM_KEY_VIEW, SEL_GUI_ANIM_NORMAL_MS, SEL_GUI_EASE_OUT);
            draw_home(d);
            return 1;
        }
        return desk_page_walk(d, ev->x > 0 ? -1 : 1); /* no tile view to go back to: the page history */
    }
    return 0;
}

/* Sends `<verb> "<path>"` with the codec's quoting: a path holds spaces, and a backslash escapes the next character. */
static int desk_send_path(desk* d, const char* verb, const char* path) {
    char line[512];
    size_t o = (size_t)snprintf(line, sizeof line, "%s \"", verb);
    for (const char* c = path; *c != '\0' && o + 4u < sizeof line; c++) {
        if (*c == '"' || *c == '\\') {
            line[o++] = '\\';
        }
        line[o++] = *c;
    }
    line[o++] = '"';
    line[o] = '\0';
    return desk_send(d, line);
}

/* The play intent for one song: OPEN loads it (the engine leaves the transport stopped), PLAY starts it. */
static int desk_play_path(desk* d, const char* path) {
    desk_send_path(d, "OPEN", path);
    return desk_send(d, "PLAY");
}

/* A dropped file or folder (DUI-P5-11). A folder runs the LIBRARY_SCAN that Add Folder runs; a lone file
 * plays (OPEN) and each file of a drop of several joins the queue (QUEUE_ADD). A path that is neither gives a toast. */
static int desk_drop_path(desk* d, const char* path, int more) {
    struct stat sb;
    d->drops++;    if (path == NULL || stat(path, &sb) != 0) {
        desk_toast(d, "Cannot find that file");
        return 1;
    }
    if (S_ISDIR(sb.st_mode)) {
        d->scan_pending++; /* folders dropped during a scan queue in the engine (I506) */
        d->scan_done = d->scan_total = 0u;
        d->scan_file[0] = '\0';
        if (d->page == PG_FOLDERS) { /* the Add Folder button turns into the progress bar */
            d->repaint = 1;
            d->uim.dirty |= SEL_UIM_DIRTY_ALL;
        } else {
            desk_toast(d, "Adding folder");
        }
        desk_send_path(d, "LIBRARY_SCAN", path);
        return 1;
    }
    if (more) {
        desk_send_path(d, "QUEUE_ADD", path);
        desk_toast(d, "Added to queue");
    } else {
        d->drop_playing = 1;
        desk_play_path(d, path);
    }
    return 1;
}

/* Add Folder: the native folder dialog; a test answers with --dialog-answer. The chosen folder comes back as a drop. */
/* I315: reads the engine's live scan progress; when it moved, redraws just the Add Folder area of the Library page.
 * Returns 1 when something was drawn. The finish handler calls it once more, so the last state is seen. */
/* 1 while the engine runs a LIBRARY_SCAN, whoever sent it. */
static int desk_scan_active(const desk* d) {
    sel_scan_progress sp;
    sel_engine_scan_progress(d->engine, &sp);
    return sp.active != 0u;
}

/* I348: the engine's library serial moved (a scan, sync or fetch finished, whoever asked): the page lists what the library holds now.
 * LIBRARY_RESULT is no signal for this: searches and browses answer with the same event. */
static void desk_library_follow(desk* d) {
    uint32_t serial = sel_engine_state(d->engine)->library_serial;

    if (d->lib_serial_seen == serial) {
        return;
    }
    if (d->lib_serial_seen != UINT32_MAX) {
        d->lib_reloads++;
        art_forget_misses(d->art); /* I689: a title that found no album before the scan made it must be looked up again */
        desk_page_load(d, d->page); /* I316: a drilldown stays (I302) */
        if (d->page == PG_FOLDERS && (d->lib_mode == 1u || d->lib_mode == 2u)) {
            desk_send(d, "LIBRARY_SEARCH \"\"");
        }
        d->repaint = 1;
        d->uim.dirty |= SEL_UIM_DIRTY_ALL; /* I689: the bar, the panel and the lists draw the covers the scan just made findable */
    }
    d->lib_serial_seen = serial;
}

static int desk_scan_sync(desk* d) {
    sel_scan_progress sp;
    int shown;
    if (!d->scan_pending) {
        return 0;
    }
    sel_engine_scan_progress(d->engine, &sp);
    if (sp.done == d->scan_done && sp.total == d->scan_total && strcmp(sp.file, d->scan_file) == 0) {
        return 0;
    }
    if (sp.active && sp.total > 0u && sp.done < sp.total) {
        d->scan_mid++;
    }
    d->scan_done = sp.done;
    d->scan_total = sp.total;
    snprintf(d->scan_file, sizeof d->scan_file, "%s", sp.file);
    shown = d->page == PG_FOLDERS && d->frames > 0u && !d->repaint;
    if (shown) {
        lib_scan_draw(d, panel_rect(d, "main"));
        d->scan_draws++;
        desk_event_frame(d, 1);
    }
    return shown;
}

/* A headless turn that waits for a running scan gives the scan thread the core for a moment instead of spinning. */
static void desk_scan_nap(void) {
#if defined(__unix__) || defined(__APPLE__)
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 1000000L;
    nanosleep(&ts, NULL);
#endif
}

/* I652c: Import playlist file. The native file dialog (a test answers with --dialog-answer) hands back a path that arrives as a
 * drop; with imp_wait set that drop goes to PLAYLIST_IMPORT for the open playlist instead of playing. */
static int desk_pls_import(desk* d, const char* path) {
    char line[640];
    size_t o = (size_t)snprintf(line, sizeof line, "PLAYLIST_IMPORT %u \"", (unsigned)d->pls_id);
    for (const char* c = path; *c != '\0' && o + 4u < sizeof line; c++) {
        if (*c == '"' || *c == '\\') {
            line[o++] = '\\';
        }
        line[o++] = *c;
    }
    line[o++] = '"';
    line[o] = '\0';
    d->imp_wait = 0;
    d->imp_sent++;
    return desk_send(d, line);
}

static int desk_pls_import_pick(desk* d) {
    if (d->dialog_answer != NULL) {
        return desk_pls_import(d, d->dialog_answer);
    }
#ifdef SELECTA_DESKTOP_SDL
    if (d->sdl != NULL && desk_sdl_open_file_dialog(d->sdl)) {
        d->imp_wait = 1;
        return 1;
    }
#endif
    desk_toast(d, "No file dialog here");
    return 1;
}

/* I652d: Add Folder on the empty playlist page. The folder dialog (a test answers with --dialog-answer) hands back a folder as a
 * drop; with imp_wait == 2 that drop goes to PLAYLIST_ADD_FOLDER for the open playlist instead of the library scan. */
static int desk_pls_add_folder(desk* d, const char* path) {
    char line[640];
    size_t o = (size_t)snprintf(line, sizeof line, "PLAYLIST_ADD_FOLDER %u \"", (unsigned)d->pls_id);
    for (const char* c = path; *c != '\0' && o + 4u < sizeof line; c++) {
        if (*c == '"' || *c == '\\') {
            line[o++] = '\\';
        }
        line[o++] = *c;
    }
    line[o++] = '"';
    line[o] = '\0';
    d->imp_wait = 0;
    d->addf_sent++;
    desk_toast(d, "Adding folder");
    return desk_send(d, line);
}

static int desk_pls_folder_pick(desk* d) {
    if (d->dialog_answer != NULL) {
        return desk_pls_add_folder(d, d->dialog_answer);
    }
#ifdef SELECTA_DESKTOP_SDL
    if (d->sdl != NULL && desk_sdl_open_folder_dialog(d->sdl)) {
        d->imp_wait = 2;
        return 1;
    }
#endif
    desk_toast(d, "No folder dialog here");
    return 1;
}

/* I652e: a PLAYLIST_IMPORT or PLAYLIST_ADD_FOLDER failed: say why instead of "0 songs added". Returns 1 when it toasted. */
static int desk_pl_fail_follow(desk* d, const sel_engine_stats* es) {
    if (es->pl_fail_runs == d->pl_fail_seen) {
        return 0;
    }
    d->pl_fail_seen = es->pl_fail_runs;
    if (es->pl_fail_reason == SEL_PLFAIL_BUSY) {
        desk_toast(d, "A library scan is running");
    } else if (es->pl_fail_reason == SEL_PLFAIL_UNREADABLE) {
        desk_toast(d, "Could not read that folder or file");
    } else {
        desk_toast(d, "Could not add the songs");
    }
    return 1;
}

/* The engine finished a PLAYLIST_ADD_FOLDER: report the count and reload the page (the sidebar count too). */
static void desk_addf_follow(desk* d) {
    sel_engine_stats es;
    char text[48];
    sel_engine_stats_get(d->engine, &es);
    if (es.addf_runs == d->addf_seen) {
        return;
    }
    d->addf_seen = es.addf_runs;
    snprintf(text, sizeof text, "%u songs added", (unsigned)es.addf_added);
    desk_toast(d, text);
    d->pls_stale = 1u;
    d->side_stale = 1u;
    d->repaint = 1;
    d->uim.dirty |= SEL_UIM_DIRTY_ALL;
}

/* The engine finished a PLAYLIST_IMPORT: report the counts and reload the page (the sidebar count too). */
static void desk_import_follow(desk* d) {
    sel_engine_stats es;
    const sel_engine_stats* st = &es;
    char text[64];
    sel_engine_stats_get(d->engine, &es);
    if (st->import_runs == d->imp_seen) {
        return;
    }
    d->imp_seen = st->import_runs;
    snprintf(text, sizeof text, "%u songs added, %u not found", (unsigned)st->import_added, (unsigned)st->import_missing);
    desk_toast(d, text);
    d->pls_stale = 1u;
    d->side_stale = 1u;
    d->repaint = 1;
    d->uim.dirty |= SEL_UIM_DIRTY_ALL;
}

/* Both playlist jobs report through here: a failure toast replaces the count toast of the same run. */
static void desk_playlist_job_follow(desk* d) {
    sel_engine_stats es;
    sel_engine_stats_get(d->engine, &es);
    if (desk_pl_fail_follow(d, &es)) {
        d->imp_seen = es.import_runs;
        d->addf_seen = es.addf_runs;
        d->pls_stale = 1u;
        d->repaint = 1;
        return;
    }
    desk_import_follow(d);
    desk_addf_follow(d);
}

static int desk_add_folder(desk* d) {
    if (d->dialog_answer != NULL) {
        return desk_drop_path(d, d->dialog_answer, 0);
    }
#ifdef SELECTA_DESKTOP_SDL
    if (d->sdl != NULL && desk_sdl_open_folder_dialog(d->sdl)) {
        return 1;
    }
#endif
    desk_toast(d, "No folder dialog here");
    return 1;
}

/* one pointer or gesture event, from the --input script or the live window */
static int desk_route_input(desk* d, const sel_gui_input* ev_in) {
    sel_gui_input tall = *ev_in; /* I468: pointer rows are canvas rows; the layout is in square pixels, so y and y2 scale by ydiv */
    const sel_gui_input* ev = &tall;
    if (d->canvas->ydiv > 1 && ev_in->kind != SEL_GUI_IN_DROP && ev_in->kind != SEL_GUI_IN_KEY) {
        tall.y *= d->canvas->ydiv;
        tall.y2 *= d->canvas->ydiv;
    }
    d->frame_open = 0; /* an input that changes nothing never flushes: its stamp must not leak into the next frame */
    desk_frame_begin(d);
    if (ev->kind == SEL_GUI_IN_DROP) {
        if (d->imp_wait == 2) {
            return desk_event_frame(d, desk_pls_add_folder(d, ev->path));
        }
        if (d->imp_wait) {
            return desk_event_frame(d, desk_pls_import(d, ev->path));
        }
        return desk_event_frame(d, desk_drop_path(d, ev->path, ev->x != 0));
    }
    if (ev->kind == SEL_GUI_IN_CLICK || ev->kind == SEL_GUI_IN_MOVE || ev->kind == SEL_GUI_IN_DRAG) {
        int changed = desk_pointer(d, ev);
        if (changed && d->songs) {
            draw_home(d);
        }
        return desk_event_frame(d, changed);
    }
    return desk_event_frame(d, desk_gesture(d, ev));
}

#ifdef SELECTA_DESKTOP_SDL
/* the pointer, gesture and key events the window queued since the last poll */
/* I341: the live UI clock. A tween starts at the clock the desktop holds when the input that starts it is routed, so the
 * clock must be current there: poll can wait up to a second, and a hover tween started on the clock of the turn before
 * would already be over on the next turn and its end state never drawn. A tween that ends between two ticks sets
 * tween_ended, so the next live turn draws the end state once. */
static void desk_live_tick(desk* d) {
    uint32_t before = sel_gui_timeline_running(&d->tl);
    d->now_ms = desk_sdl_ticks();
    sel_gui_timeline_advance(&d->tl, d->now_ms);
    desk_side_step(d);
    if (before > 0u && sel_gui_timeline_running(&d->tl) == 0u) {
        d->tween_ended = 1;
    }
}

static void desk_sdl_route(desk* d) {
    sel_gui_input ev;
    if (d->live) {
        desk_live_tick(d);
    }
    while (desk_sdl_next_input(d->sdl, &ev)) {
        if (ev.kind == SEL_GUI_IN_KEY) {
            (void)desk_key(d, &ev);
            continue;
        }
        if (ev.kind == SEL_GUI_IN_WHEEL) { /* I291: what one wheel notch costs end to end */
            uint64_t t0 = desk_sdl_now_ns();
            (void)desk_route_input(d, &ev);
            desk_sdl_note_cost(d->sdl, t0);
        } else {
            int hit = desk_route_input(d, &ev);
            if (d->report && ev.kind != SEL_GUI_IN_MOVE) { /* I525: one line per pointer event, for the click latency measure */
                fprintf(stderr, "input t=%u kind=%d x=%d y=%d hit=%d\n", (unsigned)desk_sdl_ticks(), (int)ev.kind, (int)ev.x,
                        (int)ev.y, hit);
            }
        }
    }
}
#endif

static int desk_replay_input(desk* d) {
    char* line = d->input_at != NULL ? d->input_at : d->input;
    uint32_t sent = d->sends;
    int opened = 0, pointer_sent = 0;
    d->input_done = 1;
    d->input_at = NULL;
    if (d->input_consumed != NULL) {
        *d->input_consumed = 1;
    }
    while (line != NULL && *line != '\0') {
        char* nl = strchr(line, '\n');
        int x, y;
        unsigned n;
        sel_gui_input ev;
        if (nl != NULL) {
            *nl = '\0';
        }
        if (strncmp(line, "drop ", 5) == 0) { /* `drop /a /b`: one drop of several files, all queue */
            char* sp = line + 5;
            int more = strchr(sp, ' ') != NULL; /* a lone file plays; each file of several queues */
            while (*sp != '\0') {
                char* e = strchr(sp, ' ');
                memset(&ev, 0, sizeof ev);
                ev.kind = SEL_GUI_IN_DROP;
                ev.x = more;
                snprintf(ev.path, sizeof ev.path, "%.*s", e != NULL ? (int)(e - sp) : (int)strlen(sp), sp);
                opened |= desk_route_input(d, &ev);
                sp = e != NULL ? e + 1 : sp + strlen(sp);
            }
            pointer_sent = d->sends != sent;
        } else if (sel_gui_input_parse(line, &ev) && ev.kind == SEL_GUI_IN_KEY) {
            opened |= desk_key(d, &ev);
            /* a grid or cover flow move sent VIEW_CURSOR: the engine answers on the next pump, so resume the script then */
            pointer_sent = d->sends != sent && (lib_grid_on(sel_engine_state(d->engine)) || (d->page == PG_HOME && home_shelf_mode(sel_engine_state(d->engine)))); /* I617: a Home row move too */
        } else if (sel_gui_input_parse(line, &ev) && ev.kind == SEL_GUI_IN_WAIT) {
            desk_advance(d, ev.ms);
        } else if (sel_gui_input_parse(line, &ev) &&
                   (ev.kind == SEL_GUI_IN_CLICK || ev.kind == SEL_GUI_IN_MOVE || ev.kind == SEL_GUI_IN_DRAG)) {
            opened |= desk_route_input(d, &ev);
            pointer_sent = d->sends != sent;
        } else if (sel_gui_input_parse(line, &ev) &&
                   (ev.kind == SEL_GUI_IN_WHEEL || ev.kind == SEL_GUI_IN_PINCH || ev.kind == SEL_GUI_IN_SWIPE)) {
            opened |= desk_route_input(d, &ev);
        } else if (sscanf(line, "resize %d %d", &x, &y) == 2) { /* the window was resized: relayout and repaint */
            opened |= desk_resize(d, x, y);
        } else if (sscanf(line, "scale %d", &x) == 1) { /* the window moved to a screen of another scale */
            if (desk_set_density(d, x)) {
                desk_resize(d, d->canvas->w, d->canvas->h);
            }
        } else if (sscanf(line, "tile %u", &n) == 1) {
            sel_gui_rect t = tile_rect(d, n);
            if (home_shelf_mode(sel_engine_state(d->engine))) { /* n counts the tiles over all shelves in order */
                shelf sh[SHELF_MAX];
                uint32_t s, cnt = home_shelves(sel_engine_state(d->engine), sh), left = n;
                for (s = 0; s < cnt && left >= sh[s].tiles; s++) {
                    left -= sh[s].tiles;
                }
                t = s < cnt ? shelf_tile_rect(d, s, left) : rect_of(-1, -1, 0, 0);
            }
            opened |= desk_event_frame(d, desk_click(d, t.x + t.w / 2, t.y + t.h / 2));
        } else if (sscanf(line, "quicktile %u", &n) == 1) { /* I478: a click on the centre of quick-pick tile n */
            sel_gui_rect t = quick_tile_rect(d, n);
            opened |= desk_event_frame(d, desk_click(d, t.x + t.w / 2, t.y + t.h / 2));
        } else if (sscanf(line, "quickhover %u", &n) == 1) { /* I490: the pointer moves to the centre of quick-pick tile n */
            sel_gui_rect t = quick_tile_rect(d, n);
            sel_gui_input mv;
            memset(&mv, 0, sizeof mv);
            mv.kind = SEL_GUI_IN_MOVE;
            mv.x = t.x + t.w / 2;
            mv.y = t.y + t.h / 2;
            opened |= desk_route_input(d, &mv);
        } else if (sscanf(line, "door %u", &n) == 1 && n < DOOR_COUNT) { /* a click on the centre of door n */
            sel_gui_rect r = sidebar_door_rect(d, n);
            sel_gui_input ck;
            memset(&ck, 0, sizeof ck);
            ck.kind = SEL_GUI_IN_CLICK;
            ck.x = r.x + r.w / 2;
            ck.y = r.y + r.h / 2;
            opened |= desk_route_input(d, &ck);
            pointer_sent = d->sends != sent;
        } else if (sscanf(line, "showall %u", &n) == 1 && n < 8u && d->show_all_rect[n].w > 0) { /* I617: a click on the centre of shelf n's Show all link */
            sel_gui_rect r = d->show_all_rect[n];
            sel_gui_input ck;
            memset(&ck, 0, sizeof ck);
            ck.kind = SEL_GUI_IN_CLICK;
            ck.x = r.x + r.w / 2;
            ck.y = r.y + r.h / 2;
            opened |= desk_route_input(d, &ck);
            pointer_sent = d->sends != sent;
        } else if (sscanf(line, "topbar %u", &n) == 1 && n <= TOPBAR_BROWSE && topbar_shown(d)) { /* I615: a click on the centre of top bar part n (0 Home, 1 field, 2 Browse) */
            sel_gui_rect r = topbar_part(d, (int)n);
            sel_gui_input ck;
            memset(&ck, 0, sizeof ck);
            ck.kind = SEL_GUI_IN_CLICK;
            ck.x = r.x + r.w / 2;
            ck.y = r.y + r.h / 2;
            opened |= desk_route_input(d, &ck);
            pointer_sent = d->sends != sent;
        } else if (sscanf(line, "browse %u", &n) == 1 && n < d->browse_n) { /* I615c: a click on the centre of Browse tile n of the last frame */
            sel_gui_rect r = d->browse_rect[n];
            sel_gui_input ck;
            memset(&ck, 0, sizeof ck);
            ck.kind = SEL_GUI_IN_CLICK;
            ck.x = r.x + r.w / 2;
            ck.y = r.y + r.h / 2;
            opened |= desk_route_input(d, &ck);
            pointer_sent = d->sends != sent;
        } else if (sscanf(line, "viz %u", &n) == 1 && n < 3u) { /* a click on the centre of visualizer switch button n (the page's, else the strip's) */
            sel_gui_rect r = d->viz_btn[d->viz_btn[1][n].w > 0 ? 1 : 0][n];
            sel_gui_input ck;
            memset(&ck, 0, sizeof ck);
            ck.kind = SEL_GUI_IN_CLICK;
            ck.x = r.x + r.w / 2;
            ck.y = r.y + r.h / 2;
            opened |= desk_route_input(d, &ck);
            pointer_sent = d->sends != sent;
        } else if (strncmp(line, "exec ", 5) == 0) { /* an engine line after the first page entry, so a test can set the source the page asked for */
            desk_send(d, line + 5);
            d->repaint = 1;
            opened = 1;
            pointer_sent = 1; /* the engine answers on the next pump: the script resumes then */
        } else if (sscanf(line, "hover %u", &n) == 1 && d->home && d->open_tile == 0) {
            d->hover_tile = (int32_t)n + 1;
            d->hover_frame = 1;
            draw_home(d);
            opened |= desk_event_frame(d, 1);
        } else if (strncmp(line, "navsnap", 7) == 0) { /* I630: records shelf 0's button opacity (alpha1= shelf 1's), both button scales and scroll as of now, mid-tween, for the dump */
            size_t at = strlen(d->flow_snaps);
            if (at + 128u < sizeof d->flow_snaps) {
                snprintf(d->flow_snaps + at, sizeof d->flow_snaps - at, "nav snap alpha=%d scale_prev=%d scale_next=%d scroll=%d alpha1=%d painted=%u\n",
                         d->nav_shelf1 == 1u ? (int)nav_fade_now(d) : 0, (int)nav_scale_now(d, 0u), (int)nav_scale_now(d, 1u),
                         (int)(shelf_pos_q16(d, 0u) >> 16), d->nav_shelf1 == 2u ? (int)nav_fade_now(d) : 0,
                         (unsigned)d->shelf_nav_alpha[0]);
            }
        } else if (strncmp(line, "flowsnap", 8) == 0) { /* records the covers of the last cover flow frame, mid-glide, for the dump */
            size_t at = strlen(d->flow_snaps), m;
            for (m = 0; m < d->flow_rec_n && at + 64u < sizeof d->flow_snaps; m++) {
                at += (size_t)snprintf(d->flow_snaps + at, sizeof d->flow_snaps - at, "%s%d:%d:%d:%d", m == 0 ? "flow snap " : " ",
                                       (int)d->flow_rec[m].g, (int)d->flow_rec[m].x, (int)d->flow_rec[m].w, (int)d->flow_rec[m].a);
            }
            if (at + 2u < sizeof d->flow_snaps) {
                d->flow_snaps[at++] = '\n';
                d->flow_snaps[at] = '\0';
            }
        } else if (sscanf(line, "wait %u", &n) == 1) {
            desk_advance(d, n);
        }
        line = nl != NULL ? nl + 1 : NULL;
        if (pointer_sent && line != NULL && *line != '\0') { /* the engine answers on the shell's next pump: resume then */
            d->input_at = line;
            d->input_done = 0;
            return 1;
        }
        sent = d->sends;
        pointer_sent = 0;
    }
    return opened;
}

/* Reads --input <file> (at most 4096 bytes) into a heap copy; 0 when unreadable. */
static int desk_load_input(desk* d, const char* path) {
    FILE* f = fopen(path, "rb");
    size_t n;
    if (f == NULL) {
        return 0;
    }
    d->input = (char*)d->host->mem_alloc(d->host, 4097u, SEL_MEM_FAST);
    if (d->input == NULL) {
        fclose(f);
        return 0;
    }
    n = fread(d->input, 1, 4096u, f);
    d->input[n] = '\0';
    fclose(f);
    return 1;
}

/* Writes the canvas as a binary PPM (P6, 0xRRGGBB per pixel). */
static void desk_write_frame(const desk* d, const char* path) {
    FILE* f = fopen(path, "wb");
    int32_t x, y;
    if (f == NULL) {
        fprintf(stderr, "selecta: cannot write frame dump '%s'\n", path);
        return;
    }
    fprintf(f, "P6\n%d %d\n255\n", (int)d->canvas->w, (int)d->canvas->h);
    for (y = 0; y < d->canvas->h; y++) {
        for (x = 0; x < d->canvas->w; x++) {
            uint32_t rgb = sel_gui_pixel(d->canvas, x, y);
            fputc((int)((rgb >> 16) & 0xFFu), f);
            fputc((int)((rgb >> 8) & 0xFFu), f);
            fputc((int)(rgb & 0xFFu), f);
        }
    }
    fclose(f);
}

/* The primitive test card (DUI-P2-1): every rasterizer primitive in fixed places, for the golden hash. */
static void draw_testcard(desk* d) {
    sel_gui_canvas* c = d->canvas;
    uint32_t src[32 * 32], i, j;
    uint8_t mask[16 * 16];
    uint32_t accent = ARGB(d->theme.token[SEL_GUI_TOKEN_ACCENT]), text = ARGB(d->theme.token[SEL_GUI_TOKEN_TEXT]);
    sel_gui_fill_rect(c, rect_of(0, 0, c->w, c->h), ARGB(d->theme.token[SEL_GUI_TOKEN_SURFACE]));
    sel_gui_fill_rect(c, rect_of(10, 10, 60, 40), 0xFFCC3333u);
    sel_gui_fill_rect(c, rect_of(40, 30, 60, 40), 0x8033CC33u);
    sel_gui_round_rect(c, rect_of(120, 10, 120, 60), (int32_t)d->theme.token[SEL_GUI_TOKEN_RADIUS], accent);
    sel_gui_round_rect(c, rect_of(250, 10, 40, 40), 20, text);
    sel_gui_gradient(c, rect_of(10, 90, 200, 30), 0, 0xFF000000u, 0xFFFFFFFFu);
    sel_gui_gradient(c, rect_of(220, 90, 60, 90), 1, accent, 0xFFFFD040u);
    for (i = 0; i < 8u; i++) {
        sel_gui_line(c, 330, 50, 330 + (int32_t)i * 12, 10, text);
        sel_gui_line(c, 330, 50, 330 + (int32_t)i * 12, 90, accent);
    }
    sel_gui_line(c, 420, 90, 420, 10, text);
    for (j = 0; j < 16u; j++) {
        for (i = 0; i < 16u; i++) {
            mask[j * 16u + i] = (uint8_t)(((i * 17u) ^ (j * 17u)) & 255u);
        }
    }
    sel_gui_blit_mask(c, 440, 10, mask, 16, 16, 16, text);
    for (j = 0; j < 32u; j++) {
        for (i = 0; i < 32u; i++) {
            src[j * 32u + i] = 0xFF000000u | ((i * 8u) << 16) | ((j * 8u) << 8) | (((i ^ j) & 1u) * 120u);
        }
    }
    sel_gui_blit_image(c, rect_of(10, 140, 64, 64), src, 32, 32, 32, 0);
    sel_gui_blit_image(c, rect_of(90, 140, 16, 16), src, 32, 32, 32, 1);
    if (d->shadow_ready) {
        sel_gui_shadow_draw(c, &d->shadow, rect_of(320, 140, 100, 60), 8);
    }
    sel_gui_round_rect(c, rect_of(320, 140, 100, 60), 8, accent);
    sel_gui_scrim(c, rect_of(0, 220, c->w, 60), 0x000000u, 128u);
    sel_gui_clip_push(c, rect_of(450, 140, 30, 30));
    sel_gui_fill_rect(c, rect_of(440, 130, 80, 80), 0xFFFFFFFFu);
    sel_gui_clip_pop(c);
    sel_gui_dirty_add(c, rect_of(0, 0, c->w, c->h));
}

/* Redraw the dirty regions; the mem backend records what a draw would show in the frame hash. */
static int desk_present(desk* d) {
    uint32_t dirty = sel_uim_take_dirty(&d->uim);
    const sel_uim_frame* f = &d->uim.frame;
    uint32_t bit;
    if (dirty == 0u) {
        return 0;
    }
    desk_frame_begin(d);
    d->pt_t0 = d->report ? desk_prof_clock(d) : 0u;
    if (d->frames == 0u || d->repaint) { /* the first frame and a resize paint the whole canvas */
        d->repaint = 0;
        d->seek_fill = -1;
        d->vol_fill = -1;
        d->seek_length[0] = '\0';
        sel_gui_fill_rect(d->canvas, rect_of(0, 0, d->canvas->w, cvh(d)),
                          ARGB(d->theme.token[SEL_GUI_TOKEN_FRAME]));
        sel_gui_dirty_add(d->canvas, rect_of(0, 0, d->canvas->w, cvh(d)));
        desk_pt_lap(d, PT_CANVAS);
    }
    if (d->uim.layout == SEL_UIM_LAYOUT_VISUAL) { /* the visual layout: the visualizer and the signal path fill the canvas */
        draw_visual(d);
        desk_pt_lap(d, PT_VISUAL);
        d->hash = (d->hash ^ 0x56495355u) * 16777619u;
        d->dirty_pixels = desk_flush(d);
        d->dirty_total += d->dirty_pixels;
        d->frames++;
        return 1;
    }
    for (bit = 1u; bit <= SEL_UIM_DIRTY_STATUS; bit <<= 1) {
        if ((dirty & bit) == 0u) {
            continue;
        }
        draw_widget(d, bit);
        desk_pt_lap(d, bit == SEL_UIM_DIRTY_SONG ? PT_SONG : bit == SEL_UIM_DIRTY_QUEUE ? PT_MAIN :
                    bit == SEL_UIM_DIRTY_TRANSPORT ? PT_TRANSPORT : bit == SEL_UIM_DIRTY_PROGRESS ? PT_PROGRESS :
                    bit == SEL_UIM_DIRTY_VOLUME ? PT_VOLUME : PT_STATUS);
        d->widget_draws++;
        d->hash = (d->hash ^ bit) * 16777619u;
        if (bit == SEL_UIM_DIRTY_SONG) {
            desk_hash(d, f->title);
            desk_hash(d, f->artist);
            desk_hash(d, f->format);
            /* Every effect asks the governor first (DUI-P1-9): the cover backdrop blur, the panel shadow and, from
             * the second frame on, a title cross-fade. */
            if (sel_uim_budget_allows(&d->budget, SEL_UIM_FX_BLUR)) {
                d->effect_prims++;
            }
            if (sel_uim_budget_allows(&d->budget, SEL_UIM_FX_SHADOW)) {
                d->effect_prims++;
            }
            if (d->frames > 0u && desk_motion(d)) {
                d->tweens++;
            }
        } else if (bit == SEL_UIM_DIRTY_PROGRESS) {
            desk_hash(d, f->clock);
            desk_hash(d, f->remaining);
        } else if (bit == SEL_UIM_DIRTY_STATUS) {
            desk_hash(d, f->status);
        }
    }
    desk_draw_focus(d);
    desk_pt_lap(d, PT_FOCUS);
    desk_draw_tip(d);
    desk_pt_lap(d, PT_TIP);
    desk_draw_overlay(d);
    desk_pt_lap(d, PT_OVERLAY);
    d->dirty_pixels = desk_flush(d);
    d->dirty_total += d->dirty_pixels;
    d->frames++;
    return 1;
}

static void desk_print_actions(void) {
    uint32_t i, n = sel_uim_action_count();
    char row[96], msg[160];
    for (i = 0; i < n; i++) {
        const sel_uim_action* a = sel_uim_action_at(i);
        printf("action id=%s label=%s icon=%s menu=%s key=%s line=%s tooltip=%s\n", a->id, a->label, a->icon, a->menu,
               a->key[0] != '\0' ? a->key : "-", a->line, a->tooltip);
        sel_uim_help_row(i, row, sizeof row);
        printf("help %s\n", row);
    }
    printf("actions count=%u exemptions=%u check=%s\n", (unsigned)n, (unsigned)sel_uim_exemption_count(),
           sel_uim_actions_check(msg, sizeof msg) == 0 ? "ok" : msg);
}

/* Each WxH is one resize event: the layout is recomputed and the result printed, regions with their rectangles. */
static void desk_replay_resizes(desk* d) {
    const char* p = d->resize;
    unsigned w, h;
    int used;
    char msg[96];
    while (sscanf(p, "%ux%u%n", &w, &h, &used) == 2) {
        uint32_t i;
        sel_uim_layout_compute_bar(&d->layout, w, h, desk_bitmap_text(d) ? SEL_UIM_TIER_BITMAP : SEL_UIM_TIER_DEFAULT, desk_rail_w(d),
                                   sel_uim_footer_height(h));
        printf("layout width=%u height=%u id=%s regions=%u check=%s\n", w, h,
               sel_uim_breakpoint_name(d->layout.breakpoint), (unsigned)d->layout.count,
               sel_uim_layout_check(&d->layout, msg, sizeof msg) == 0 ? "ok" : msg);
        for (i = 0; i < d->layout.count; i++) {
            const sel_uim_region* r = &d->layout.regions[i];
            printf("region name=%s x=%d y=%d w=%u h=%u\n", r->name, (int)r->x, (int)r->y, (unsigned)r->w,
                   (unsigned)r->h);
        }
        for (i = 0; i < TRANSPORT_COUNT; i++) {
            sel_gui_rect b = desk_button_rect(d, i);
            if (desk_slot_action(d, i) != NULL) {
                printf("button id=%s x=%d y=%d w=%d h=%d\n", desk_slot_action(d, i), (int)b.x, (int)b.y, (int)b.w,
                       (int)b.h);
            }
        }
        p += used;
        if (*p != ',') {
            break;
        }
        p++;
    }
}

/* What the mem backend paints: every widget takes its colour and size from a token, never from a literal. */
static void desk_print_tokens(const desk* d) {
    static const uint32_t none[SEL_GUI_TOKEN_COUNT] = {
#define X(id, name, def, lo, hi) def,
        SEL_GUI_TOKENS(X)
#undef X
    };
    uint64_t changed = sel_gui_tokens_diff(d->theme.token, none);
    uint32_t i, n = 0u;
    printf("pixel widget=play_button rgb=0x%06x\n", (unsigned)d->theme.token[SEL_GUI_TOKEN_ACCENT]);
    printf("pixel widget=panel rgb=0x%06x\n", (unsigned)d->theme.token[SEL_GUI_TOKEN_SURFACE]);
    printf("pixel widget=label rgb=0x%06x\n", (unsigned)d->theme.token[SEL_GUI_TOKEN_TEXT]);
    printf("metric name=corner_radius px=%u\n", (unsigned)d->theme.token[SEL_GUI_TOKEN_RADIUS]);
    printf("metric name=gap px=%u\n", (unsigned)d->theme.token[SEL_GUI_TOKEN_SPACING]);
    for (i = 0; i < SEL_GUI_TOKEN_COUNT; i++) {
        n += (uint32_t)((changed >> i) & 1u);
    }
    printf("tokens count=%u changed=%u\n", (unsigned)SEL_GUI_TOKEN_COUNT, (unsigned)n);
    for (i = 0; i < SEL_GUI_TOKEN_COUNT; i++) {
        if (((changed >> i) & 1u) != 0u) {
            printf("token changed=%s\n", sel_gui_token_name((sel_gui_token)i));
        }
    }
}

/* Reads the --theme file over the defaults; 0 with a message on a missing file or a bad line. */
static int desk_load_theme(desk* d, const char* path) {
    char buf[4096];
    size_t n;
    int bad = 0;
    FILE* f = fopen(path, "rb");
    if (f == NULL) {
        fprintf(stderr, "selecta: desktop theme '%s' cannot be read\n", path);
        return 0;
    }
    n = fread(buf, 1, sizeof buf - 1, f);
    fclose(f);
    buf[n] = '\0';
    if (txt_theme_parse(&d->theme, buf, &bad) < 0) {
        if (d->theme.bad_token[0] != '\0') {
            if (d->theme.bad_range) {
                fprintf(stderr, "selecta: desktop theme '%s' line %d: token '%s' resolves to a value outside its range\n",
                        path, bad, d->theme.bad_token);
            } else {
                fprintf(stderr, "selecta: desktop theme '%s' line %d: token '%s' has a missing or circular reference\n",
                        path, bad, d->theme.bad_token);
            }
        } else {
            fprintf(stderr, "selecta: desktop theme '%s' line %d is not a role or token line in range\n", path, bad);
        }
        return 0;
    }
    return 1;
}

static void desk_free_icons(desk* d);

static void desk_close(void* state) {
    desk* d = (desk*)state;
    int i;
    if (d != NULL) {
        if (d->report) {
            printf("desktop_ui closed frames=%u title=%s clock=%s status=%s\n", (unsigned)d->frames, d->uim.frame.title,
                   d->uim.frame.clock, d->uim.frame.status);
        }
        if (d->report && d->widgets) {
            for (i = 0; i < (int)SEL_GUI_WIDGET_KIND_COUNT; i++) {
                printf("widget_kind name=%s draws=%u\n", sel_gui_widget_kind_name((uint32_t)i),
                       (unsigned)d->wstats.draws[i]);
            }
        }
        if (d->tokens) {
            desk_print_tokens(d);
        }
        if (d->resize != NULL) {
            desk_replay_resizes(d);
        }
        if (d->actions) {
            desk_print_actions();
        }
        if (d->frame_dump != NULL && d->canvas != NULL) {
            desk_write_frame(d, d->frame_dump);
        }
        if (d->input != NULL) {
            d->host->mem_free(d->host, d->input);
        }
        free(d->qg.before);
        free(d->qg.name);
        free(d->qg.pool);
        art_close(d->art);
        desk_free_icons(d);
        for (i = 0; i < 2 * DESK_SIZES; i++) {
            desk_atlas_free(d->atlas[i / DESK_SIZES][i % DESK_SIZES]);
        }
#ifdef SELECTA_DESKTOP_SDL
        desk_sdl_close(d->sdl);
#endif
        if (d->canvas != NULL) {
            if (d->viz_px != NULL) {
                d->host->mem_free(d->host, d->viz_px);
            }
            d->host->mem_free(d->host, d->canvas->pixels);
            d->host->mem_free(d->host, d->canvas);
        }
        sel_uim_selset_free(&d->sel);
        d->host->mem_free(d->host, d);
    }
}

/* Allocates the canvas from --canvas WxH in the --format; 0 on a bad size or no memory. Without --canvas the size is
 * the backend's own (def_w x def_h: the sdl window default 1280x800, the Workbench screen on rtg, I340), and the mem
 * backend keeps 1280x800. */
static int desk_make_canvas(desk* d, const char* size, int32_t def_w, int32_t def_h) {
    unsigned w = (unsigned)def_w, h = (unsigned)def_h;
    uint32_t pal[256];
    void* pixels;
    uint32_t format = d->budget.format == SEL_UIM_FMT_INDEX8 ? SEL_GUI_INDEX8 : SEL_GUI_RGBA32;
    if (size != NULL && (sscanf(size, "%ux%u", &w, &h) != 2 || w < 64u || h < 64u || w > 4096u || h > 4096u)) {
        fprintf(stderr, "selecta: desktop canvas '%s' is not WxH between 64x64 and 4096x4096\n", size);
        return 0;
    }
    d->canvas = (sel_gui_canvas*)d->host->mem_alloc(d->host, sizeof *d->canvas, SEL_MEM_FAST);
    pixels = d->host->mem_alloc(d->host, sel_gui_pixel_bytes(format, (int32_t)w, (int32_t)h), SEL_MEM_FAST);
    if (d->canvas == NULL || pixels == NULL) {
        if (d->canvas != NULL) {
            d->host->mem_free(d->host, d->canvas);
            d->canvas = NULL;
        }
        if (pixels != NULL) {
            d->host->mem_free(d->host, pixels);
        }
        return 0;
    }
    sel_gui_canvas_init(d->canvas, format, (int32_t)w, (int32_t)h, pixels);
    d->canvas->ydiv = d->ydiv_opt > 0 ? d->ydiv_opt : (d->is_aga && h <= 300u ? 2 : 1); /* I468: the AGA 640x256 pixels are twice as tall as wide */
    desk_prof_attach(d);
    if ((d->is_aga || d->copper_opt > 0) && format == SEL_GUI_INDEX8) {
        sel_gui_copper_enable(d->canvas, d->copper_opt > 0 ? (uint32_t)d->copper_opt : 4u); /* I441: the aga backend draws vertical gradients with the copper */
    }
    if (format == SEL_GUI_INDEX8) {
        static const int ui_tokens[] = {SEL_GUI_TOKEN_ACCENT, SEL_GUI_TOKEN_SURFACE, SEL_GUI_TOKEN_TEXT,
            SEL_GUI_TOKEN_BACKGROUND, SEL_GUI_TOKEN_SURFACE_RAISED, SEL_GUI_TOKEN_SURFACE_HOVER,
            SEL_GUI_TOKEN_SURFACE_PRESSED, SEL_GUI_TOKEN_TEXT_MUTED, SEL_GUI_TOKEN_TEXT_DISABLED,
            SEL_GUI_TOKEN_ACCENT_TEXT, SEL_GUI_TOKEN_DIVIDER, SEL_GUI_TOKEN_FOCUS_RING, SEL_GUI_TOKEN_ERROR,
            SEL_GUI_TOKEN_WARNING, SEL_GUI_TOKEN_SUCCESS, SEL_GUI_TOKEN_SCRIM, SEL_GUI_TOKEN_FRAME,
            SEL_GUI_TOKEN_PANEL, SEL_GUI_TOKEN_HEADER_TINT, SEL_GUI_TOKEN_TRACK, SEL_GUI_TOKEN_HOVER_LIGHT};
        uint32_t ui[32], k;
        for (k = 0; k < sizeof ui_tokens / sizeof ui_tokens[0]; k++) { /* the 32 UI colours: the theme's colour tokens, then steps */
            ui[k] = d->theme.token[ui_tokens[k]];
        }
        sel_gui_palette_budget(pal, ui, k);
        sel_gui_cover_palette(pal + 32, d->theme.token); /* I691e: entries 32..63 hold the platform cover tones */
        sel_gui_palette_headline_shades(pal, d->theme.token[SEL_GUI_TOKEN_TEXT], d->theme.token[SEL_GUI_TOKEN_BACKGROUND],
            d->theme.token[SEL_GUI_TOKEN_TEXT], d->theme.token[SEL_GUI_TOKEN_SURFACE]); /* I526-P2b */
        sel_gui_set_palette(d->canvas, pal, 0u, 256u);
    }
    if (sel_uim_budget_allows(&d->budget, SEL_UIM_FX_SHADOW)) {
        sel_gui_shadow_bake(&d->shadow, (uint32_t)lpx(d, HOME_SHADOW_R), 0x000000u, 96u);
        d->shadow_ready = 1;
    }
    desk_relayout(d);
    return 1;
}

/* The fit zoom (design 10.3), Q16: the canvas in logical units of the density against the 1024x640 reference, never
 * enlarging and never below 0.625 (the compact layout takes over below that). */
static int32_t desk_fit_q16(const desk* d, int32_t w, int32_t h) {
    if (desk_bitmap_text(d)) {
        return 65536; /* I526: the native screen, one layout unit is one column */
    }
    int64_t fw = (int64_t)w * 65536 / (1024 * (int64_t)d->scale), fh = (int64_t)h * 65536 / (640 * (int64_t)d->scale);
    int64_t f = fw < fh ? fw : fh;
    f = f > 65536 ? 65536 : f < 40960 ? 40960 : f;
    return d->baked_zoom ? desk_baked_scale_fit_q16((int32_t)f) : (int32_t)f;
}

/* The scale (density times zoom, Q16) for a canvas size. Integer for density times 100 percent: pixel-identical to the
 * old integer scale. */
static int32_t desk_scale_for(const desk* d, int32_t w, int32_t h) {
    int32_t zoom = d->zoom_pct == 0 ? desk_fit_q16(d, w, h) : (int32_t)((int64_t)d->zoom_pct * 65536 / 100);
    if (d->baked_zoom) {
        zoom = desk_baked_scale_snap_q16(zoom); /* I587: only the baked scales exist on the Amiga */
    }
    return (int32_t)((int64_t)d->scale * zoom);
}

/* The layout runs at the logical size: the canvas divided by the scale. */
static void desk_layout_into(desk* d, sel_uim_layout* l) {
    uint32_t lh = (uint32_t)px_to_logical(d, cvh(d));
    sel_uim_layout_compute_panel(l, (uint32_t)px_to_logical(d, d->canvas->w), lh,
                                 desk_bitmap_text(d) ? SEL_UIM_TIER_BITMAP : SEL_UIM_TIER_DEFAULT, desk_rail_w(d), sel_uim_footer_height(lh),
                                 d->side_wide, d->detail_hidden);
}

/* I668: while the width tween runs the sidebar is between its two ends and the main region takes the rest. */
static void desk_relayout(desk* d) {
    desk_layout_into(d, &d->layout);
    if (d->side_anim != 0u) {
        int32_t v = sel_gui_tween_value(&d->tl, ANIM_KEY_SIDE, 65536);
        int32_t from = (int32_t)d->side_from, to = (int32_t)d->side_to;
        sel_uim_layout_set_side_width(&d->layout, (uint32_t)(from + (int32_t)(((int64_t)(to - from) * v) >> 16)));
    }
}

/* I668: the sidebar width in logical px (0 when the layout has none). */
static uint32_t desk_side_w(const sel_uim_layout* l) {
    uint32_t i;
    for (i = 0; i < l->count; i++) {
        if (strcmp(l->regions[i].name, "sidebar") == 0) {
            return l->regions[i].w;
        }
    }
    return 0u;
}

/* I609: widen or narrow the sidebar through the layout. I668: the width eases from where it is now to the new target through the
 * shared tween path, so a click mid-tween reverses from the current width. */
static void side_wide_toggle(desk* d) {
    uint32_t cur = desk_side_w(&d->layout);
    sel_uim_layout target;
    d->side_wide = !d->side_wide;
    desk_layout_into(d, &target);
    d->side_from = cur;
    d->side_to = desk_side_w(&target);
    d->side_anim = d->side_from != d->side_to && desk_tween(d, ANIM_KEY_SIDE, SEL_GUI_ANIM_FAST_MS, SEL_GUI_EASE_OUT) ? 1u : 0u;
    desk_relayout(d);
}

/* The backend density changed (a window moved to another screen): the scale follows, the zoom stays. */
static int desk_set_density(desk* d, int32_t density) {
    if (density < 1 || density > 3) {
        return 0;
    }
    d->scale = density;
    return desk_set_scale(d, desk_scale_for(d, d->canvas->w, cvh(d)));
}

/* The icon atlas for the current scale (DUI-P2-8): each logical icon size at k times the size, picked from the
 * baked masks (tools/gui_iconbake.c bakes every logical size times 1, 2 and 3). Counts one icon rebuild. */
static void desk_free_icons(desk* d) {
    int i;
    for (i = 0; i < DESK_ICON_SIZES; i++) {
        if (d->icon_buf[i] != NULL) {
            d->host->mem_free(d->host, d->icon_buf[i]);
            d->icon_buf[i] = NULL;
        }
    }
}

/* I584: the baked masks are packed (gui_icons.h); every size the UI shows is unpacked here, icon by icon, into the
 * cache d->icon_buf: an exact baked size is unpacked as is, any other size is area-resampled from the smallest baked
 * size above it (I460), so the pixels equal those of the unpacked-table version. */
static const uint8_t g_icon_none[1] = {0};

static void desk_build_icons(desk* d) {
    int i;
    uint32_t s;
    int32_t yd = d->canvas != NULL && d->canvas->ydiv > 1 ? d->canvas->ydiv : 1; /* I468: tall pixels draw px / yd rows */
    uint8_t* tmp = NULL;
    desk_free_icons(d);
    d->icon_resampled = 0u;
    for (i = 0; i < DESK_ICON_SIZES; i++) {
        int32_t px = lpx(d, g_icon_sizes[i]);
        int exact = 0;
        uint32_t from = 0u;
        int have = 0;
        int32_t n;
        size_t dst_sz;
        uint32_t k;
        d->icon_px[i] = px;
        for (s = 0; s < sel_gui_icons_baked.size_n; s++) {
            int32_t bs = sel_gui_icons_baked.sizes[s];
            if (bs == px) {
                /* I588: the exact bake is unpacked 1:1 on square pixels; on tall pixels it is still the source, so the
                 * only resample is the 2:1 row merge (no 6:5 / 12:5 weights from the next larger size) */
                from = s;
                exact = yd == 1;
                have = 1;
                break;
            }
            /* no exact size: the smallest baked size above the target (sizes ascend), else the largest */
            if (bs > px && !have) {
                from = s;
                have = 1;
            }
        }
        if (!have) {
            from = sel_gui_icons_baked.size_n - 1u;
        }
        n = sel_gui_icons_baked.sizes[from];
        dst_sz = exact ? (size_t)n * (size_t)n : (size_t)px * (size_t)(px / yd);
        d->icon_buf[i] = (uint8_t*)d->host->mem_alloc(d->host, dst_sz * sel_gui_icons_baked.count, SEL_MEM_FAST);
        if (!exact && d->icon_buf[i] != NULL && tmp == NULL) {
            tmp = (uint8_t*)d->host->mem_alloc(d->host, (size_t)sel_gui_icons_baked.sizes[sel_gui_icons_baked.size_n - 1u] *
                                                           (size_t)sel_gui_icons_baked.sizes[sel_gui_icons_baked.size_n - 1u], SEL_MEM_FAST);
            if (tmp == NULL) {
                d->host->mem_free(d->host, d->icon_buf[i]);
                d->icon_buf[i] = NULL;
            }
        }
        if (d->icon_buf[i] == NULL) { /* out of memory: this size draws nothing */
            d->icon_alpha[i] = g_icon_none;
            d->icon_px[i] = 0;
            continue;
        }
        for (k = 0; k < sel_gui_icons_baked.count; k++) {
            if (exact) {
                sel_gui_icon_unpack(&sel_gui_icons_baked, from, k, d->icon_buf[i] + k * dst_sz);
            } else {
                sel_gui_icon_unpack(&sel_gui_icons_baked, from, k, tmp);
                sel_gui_icon_resample_tall(tmp, n, d->icon_buf[i] + k * dst_sz, px, px / yd);
            }
        }
        d->icon_alpha[i] = d->icon_buf[i];
        if (!exact) {
            d->icon_resampled++;
        }
    }
    if (tmp != NULL) {
        d->host->mem_free(d->host, tmp);
    }
    d->icons.count = sel_gui_icons_baked.count;
    d->icons.size_n = DESK_ICON_SIZES;
    d->icons.sizes = d->icon_px;
    d->icons.alpha = d->icon_alpha;
    d->icon_rebuilds++;
}

/* A new integer pixel scale (DUI-P2-8): the text atlases are dropped (desk_font rebuilds them at the new pixel
 * size on first use), the icon atlas and the shadow are rebuilt, and the next resize relays out and repaints. The
 * same scale again does nothing. */
static int desk_set_scale(desk* d, int32_t scale_q16) {
    int i;
    if (scale_q16 < 32768 || scale_q16 > 196608 || scale_q16 == d->scale_q16) {
        return 0;
    }
    d->scale_q16 = scale_q16;
    for (i = 0; i < 2 * DESK_SIZES; i++) {
        desk_atlas_free(d->atlas[i / DESK_SIZES][i % DESK_SIZES]);
        d->atlas[i / DESK_SIZES][i % DESK_SIZES] = NULL;
    }
    d->atlas_rebuilds++;
    desk_build_icons(d);
    if (d->shadow_ready) {
        sel_gui_shadow_bake(&d->shadow, (uint32_t)lpx(d, HOME_SHADOW_R), 0x000000u, 96u);
    }
    return 1;
}

/* The window changed size (a scripted `resize W H`, or the user or a scale change on the sdl backend): the canvas
 * takes the new pixel size, the layout is recomputed and the whole canvas is repainted and presented. Returns 0: the
 * repaint is already presented, so the caller has nothing left to flush. */
static int desk_resize(desk* d, int32_t w, int32_t h) {
    void* pixels;
    if (w < 64 || h < 64 || w > 4096 || h > 4096 || d->canvas->format != SEL_GUI_RGBA32) {
        return 0;
    }
    pixels = d->host->mem_alloc(d->host, sel_gui_pixel_bytes(SEL_GUI_RGBA32, w, h), SEL_MEM_FAST);
    if (pixels == NULL) {
        return 0;
    }
    d->host->mem_free(d->host, d->canvas->pixels);
    {
        int32_t yd = d->canvas->ydiv; /* the pixel aspect belongs to the screen, not to the size */
        sel_gui_canvas_init(d->canvas, SEL_GUI_RGBA32, w, h, pixels);
        d->canvas->ydiv = yd;
    }
    desk_prof_attach(d);
#ifdef SELECTA_DESKTOP_SDL
    if (d->sdl != NULL && !desk_sdl_resize(d->sdl, w, h)) {
        return 0;
    }
#endif
    desk_set_scale(d, desk_scale_for(d, w, cvh(d))); /* a fit zoom follows the new size before the layout runs */
    desk_relayout(d);
    d->repaint = 1;
    d->uim.dirty |= SEL_UIM_DIRTY_ALL;
    desk_present(d);
    return 0;
}

static void* desk_open(const sel_ui_open_args* a) {
    const char* backend = desk_opt(a, "--backend");
    desk* d;
    int i;
    int want_sdl = backend != NULL && (strcmp(backend, "sdl") == 0 || strcmp(backend, "rtg") == 0);
#ifdef SELECTA_DESKTOP_AGA
    want_sdl = want_sdl || (backend != NULL && strcmp(backend, "aga") == 0);
#endif
    for (i = 0; i < a->argc; i++) {
        g_desk_trace = g_desk_trace || strcmp(a->argv[i], "--stats") == 0;
    }
    desk_trace(a->host, "desk_open begin");
    if (backend != NULL && strcmp(backend, "mem") != 0 && !want_sdl) {
        fprintf(stderr, "selecta: desktop backend '%s' is not mem or sdl\n", backend);
        return NULL;
    }
#ifndef SELECTA_DESKTOP_SDL
    if (want_sdl) {
        fprintf(stderr, "selecta: desktop backend 'sdl' is not available in this build (configure with -DSELECTA_UI_DESKTOP=ON)\n");
        return NULL;
    }
#endif
    d = (desk*)a->host->mem_alloc(a->host, sizeof *d, SEL_MEM_FAST);
    if (d == NULL) {
        return NULL;
    }
    memset(d, 0, sizeof *d);
    sel_uim_selset_init(&d->sel);
    d->side_stale = 1u; /* I608: the first sidebar draw reads the saved playlists */
    d->side_view = SEL_UIM_LVIEW_LIST; /* I611: the Your Library list keeps its two-line rows by default */
    d->lib_serial_seen = UINT32_MAX;
    d->songs_drag = -1;
    d->qdrag_from = -1;
    d->qdrag_gap = -1;
    d->tile_step = TILE_STEP_DEFAULT;
    d->engine = a->engine;
    d->host = a->host;
    d->find_plugin = a->find_plugin;
    viz_load(d);
    d->input_consumed = a->input_consumed;
    d->hash = 2166136261u;
    if (!sel_uim_budget_for(&d->budget, desk_opt(a, "--format") != NULL ? desk_opt(a, "--format") : sel_uim_default_format(backend))) {
        fprintf(stderr, "selecta: desktop format '%s' is not rgba32 or index8\n", desk_opt(a, "--format"));
        a->host->mem_free(a->host, d);
        return NULL;
    }
    txt_theme_default(&d->theme);
    if (desk_opt(a, "--theme") != NULL && !desk_load_theme(d, desk_opt(a, "--theme"))) {
        a->host->mem_free(a->host, d);
        return NULL;
    }
    if (desk_opt(a, "--view") != NULL) {
        const char* view = desk_opt(a, "--view");
        if (strcmp(view, "testcard") == 0) {
            d->testcard = 1;
        } else if (strcmp(view, "text") == 0) {
            d->textsheet = 1;
        } else if (strcmp(view, "widgets") == 0) {
            d->widgets = 1;
        } else if ((d->view_pg = desk_page_named(d, view)) != PG_COUNT) { /* the pages the doors open: entered by desk_page_goto before the first frame (I266) */
            d->view_pending = 1;
        } else {
            fprintf(stderr, "selecta: desktop view '%s' is not testcard, text, home, library (opens Browse Folders), folders, songs, queue, playlists, songinfo or widgets\n", view);
            a->host->mem_free(a->host, d);
            return NULL;
        }
    } else { /* no --view: the sidebar marks Home at start, so enter Home through the door's path (I594); else the main area stays empty */
        d->view_pending = 1;
        d->view_pg = PG_HOME;
    }
    d->scale = 1;
    d->scale_q16 = 65536;
    d->is_aga = backend != NULL && strcmp(backend, "aga") == 0;
    d->copper_opt = desk_opt(a, "--copper") != NULL ? atoi(desk_opt(a, "--copper")) : 0;
    d->ydiv_opt = desk_opt(a, "--ydiv") != NULL ? atoi(desk_opt(a, "--ydiv")) : 0;
    d->baked_zoom = (backend != NULL && (strcmp(backend, "rtg") == 0 || strcmp(backend, "aga") == 0)) || desk_opt(a, "--baked-zoom") != NULL;
    d->zoom_default = backend != NULL && (strcmp(backend, "rtg") == 0 || strcmp(backend, "aga") == 0) ? 0 : 100;
    d->zoom_pct = d->zoom_default;
    {
        /* UI Zoom (I458): the pref ui.desktop.zoom, overridden by --zoom; fit, 75, 100, 125 or 150. */
        const char* zv = desk_pref(d, "ui.desktop.zoom", "");
        if (desk_opt(a, "--zoom") != NULL) {
            zv = desk_opt(a, "--zoom");
        }
        if (zv != NULL && zv[0] != '\0') {
            if (strcmp(zv, "fit") == 0) {
                d->zoom_pct = 0;
            } else if (strcmp(zv, "75") == 0 || strcmp(zv, "100") == 0 || strcmp(zv, "125") == 0 || strcmp(zv, "150") == 0) {
                d->zoom_pct = atoi(zv);
            } else if (desk_opt(a, "--zoom") != NULL) {
                fprintf(stderr, "selecta: desktop zoom '%s' is not fit, 75, 100, 125 or 150\n", zv);
                a->host->mem_free(a->host, d);
                return NULL;
            }
        }
    }
    d->focus = -1;
    desk_bind_actions(d);
    if (desk_opt(a, "--scale") != NULL) {
        int sc = atoi(desk_opt(a, "--scale"));
        if (sc < 1 || sc > 3) {
            fprintf(stderr, "selecta: desktop scale '%s' is not 1, 2 or 3\n", desk_opt(a, "--scale"));
            a->host->mem_free(a->host, d);
            return NULL;
        }
        d->scale = sc;
        d->scale_q16 = sc * 65536;
    }
    desk_trace(a->host, "desk_open before icons");
    desk_build_icons(d);
    desk_trace(a->host, "desk_open icons built");
    d->icon_rebuilds = 0u; /* the first build is not a rebuild */
#ifdef SELECTA_DESKTOP_AGA
    if (want_sdl) { /* I566: Fill Screen (Overscan), read once at open */
        const char* ov = desk_pref(d, "ui.desktop.overscan", "0");
        desk_aga_set_overscan(ov != NULL && strcmp(ov, "1") == 0);
    }
#endif
    {
        int32_t def_w = 1280, def_h = 800;
#ifdef SELECTA_DESKTOP_SDL
        if (want_sdl) {
            desk_sdl_default_size(&def_w, &def_h);
        }
#endif
        if (!desk_make_canvas(d, desk_opt(a, "--canvas"), def_w, def_h)) {
            desk_close(d);
            return NULL;
        }
        if (d->canvas->ydiv > 1) { /* the icon masks of tall pixels are px / ydiv rows: rebuilt now the canvas knows its aspect */
            desk_build_icons(d);
            d->icon_rebuilds = 0u;
        }
    }
    for (i = 0; i < a->argc; i++) {
        if (strcmp(a->argv[i], "--tokens") == 0) {
            d->tokens = 1;
        }
        if (strcmp(a->argv[i], "--stats") == 0) {
            d->report = 1;
            desk_prof_attach(d);
#ifdef SELECTA_DESKTOP_AGA
            desk_amiga_input_trace = 1; /* I525: stamped/taken times and down/up counts of every button */
#endif
        }
        if (strcmp(a->argv[i], "--resize") == 0 && i + 1 < a->argc) {
            d->resize = a->argv[i + 1];
        }
        if (strcmp(a->argv[i], "--dialog-answer") == 0 && i + 1 < a->argc) {
            d->dialog_answer = a->argv[i + 1];
        }
        if (strcmp(a->argv[i], "--ticks") == 0 && i + 1 < a->argc) {
            d->ticks = (uint32_t)atoi(a->argv[i + 1]);
            d->ticks_given = 1;
        }
        if (strcmp(a->argv[i], "--frame-dump") == 0 && i + 1 < a->argc) {
            d->frame_dump = a->argv[i + 1];
        }
        if (strcmp(a->argv[i], "--input") == 0 && i + 1 < a->argc && !(a->input_consumed != NULL && *a->input_consumed) &&
            !desk_load_input(d, a->argv[i + 1])) {
            fprintf(stderr, "selecta: cannot read input script '%s'\n", a->argv[i + 1]);
            desk_close(d);
            return NULL;
        }
        if (strcmp(a->argv[i], "--clock") == 0 && i + 1 < a->argc && strcmp(a->argv[i + 1], "fixed") != 0) {
            fprintf(stderr, "selecta: desktop clock '%s' is not fixed\n", a->argv[i + 1]);
            desk_close(d);
            return NULL;
        }
        if (strcmp(a->argv[i], "--actions") == 0) {
            d->actions = 1;
        }
    }
#ifdef SELECTA_DESKTOP_SDL
    if (want_sdl) {
        if (d->canvas->format != SEL_GUI_RGBA32 && !DESK_BACKEND_INDEX8) {
            fprintf(stderr, "selecta: the %s backend shows an rgba32 canvas, not index8\n", DESK_BACKEND_NAME);
            desk_close(d);
            return NULL;
        }
        desk_trace(a->host, "desk_open canvas made, backend (screen) open begin");
        d->sdl = desk_backend_open(d->canvas->w, d->canvas->h, d->canvas->format == SEL_GUI_INDEX8);
        desk_trace(a->host, "desk_open backend (screen) open done");
        if (d->sdl == NULL) {
            desk_close(d);
            return NULL;
        }
        d->live = d->input == NULL && !desk_sdl_headless(d->sdl);
    }
#endif
    if (desk_set_scale(d, desk_scale_for(d, d->canvas->w, cvh(d)))) { /* the zoom (fit on rtg and aga) applies before the first frame */
        d->atlas_rebuilds = 0u;
        d->icon_rebuilds = 0u;
        desk_relayout(d);
    }
    desk_trace(a->host, "desk_open scale applied");
    d->art = art_open(a->art);
    sel_gui_timeline_init(&d->tl);
    sel_uim_init(&d->uim);
    sel_uim_update(&d->uim, sel_engine_state(d->engine));
    if (d->pl_page || d->songs) { /* the folders source holds the saved playlists: the page lists them, the songs view drops onto them */
        desk_send(d, "VIEW_SOURCE folders");
        desk_send(d, "VIEW_WINDOW 64");
        d->folder_opens++;
    }
    if (d->view_pending) { /* --view named a page: entered here, before the shell's --exec lines, so their setup (source, rows, cursor) survives (I398) */
        d->view_pending = 0;
        desk_page_apply(d, d->view_pg);
    }
    if (d->testcard) {
        draw_testcard(d);
        d->dirty_pixels = desk_flush(d);
        d->dirty_total = d->dirty_pixels;
    } else if (d->textsheet || d->widgets) {
        d->dirty_total = 0; /* drawn on the first turn, after the --exec commands queued their names */
    } else {
        desk_trace(a->host, "desk_open first present begin");
        desk_present(d);
        desk_trace(a->host, "desk_open first present done");
    }
    return d;
}

/* The snapshot is refreshed at the end of a pump, after the events, so the model reads the state the previous
 * pump left: an event here and the wait below together never miss a change. */
static void desk_on_event(void* state, const sel_message* evt) {
    desk* d = (desk*)state;
    int src_row = 0;
    if (evt->id == SEL_EVT_PREFS_CHANGED && strcmp(evt->args[0].v.s, "ui.desktop.mode") == 0) {
        uint32_t before = d->uim.layout;
        sel_uim_set_layout(&d->uim, evt->args[1].v.s);
        d->repaint |= before != d->uim.layout; /* a layout change repaints the whole canvas */
    }
    if (evt->id == SEL_EVT_PREFS_CHANGED && strcmp(evt->args[0].v.s, "ui.desktop.viz") == 0) {
        viz_select(d, evt->args[1].v.s);
    }
    if (evt->id == SEL_EVT_LIBRARY_RESULT && evt->argc > 0 && evt->args[0].type == SEL_ARG_TAGS && sel_tag_find(evt->args[0].v.t, SEL_SRC_KEY, 0) != 0) {
        const sel_tag* t = evt->args[0].v.t; /* a SOURCES row (I265): not the end of a scan */
        if (d->src_wait) {
            d->src_wait = 0;
            d->src_n = 0;
        }
        int changed = 1;
        if (d->src_n < sizeof d->src / sizeof d->src[0]) {
            uint32_t n = d->src_n++;
            unsigned char before[sizeof d->src[0]]; /* the row held from the last SOURCES answer: an identical answer paints nothing new */
            memcpy(before, &d->src[n], sizeof before);
            snprintf(d->src[n].key, sizeof d->src[n].key, "%s", (const char*)sel_tag_find(t, SEL_SRC_KEY, (uintptr_t)""));
            snprintf(d->src[n].kind, sizeof d->src[n].kind, "%s", (const char*)sel_tag_find(t, SEL_SRC_KIND, (uintptr_t)""));
            snprintf(d->src[n].name, sizeof d->src[n].name, "%s", (const char*)sel_tag_find(t, SEL_SRC_NAME, (uintptr_t)""));
            snprintf(d->src[n].path, sizeof d->src[n].path, "%s", (const char*)sel_tag_find(t, SEL_SRC_PATH, (uintptr_t)""));
            d->src[n].enabled = sel_tag_find(t, SEL_SRC_ENABLED, 1) != 0;
            d->src[n].count = (uint32_t)sel_tag_find(t, SEL_SRC_COUNT, 0);
            d->src[n].synced = (int64_t)sel_tag_find(t, SEL_SRC_SYNCED, 0);
            d->src[n].due = sel_tag_find(t, SEL_SRC_DUE, 0) != 0;
            d->src[n].snooze = (int64_t)sel_tag_find(t, SEL_SRC_SNOOZED, 0);
            changed = memcmp(before, &d->src[n], sizeof before) != 0;
        }
        d->repaint |= changed && d->page == PG_FOLDERS; /* only the Library page draws these rows (banner, Sources tab); a page change repaints on its own */
        src_row = 1;
    }
    if (evt->id == SEL_EVT_LIBRARY_RESULT && d->scan_pending && !src_row) { /* a LIBRARY_SCAN finished: tell the user and refresh what the Library page lists */
        sel_scan_info si;
        char msg[64];
        sel_engine_scan_info(d->engine, &si);
        desk_scan_sync(d); /* the last progress state, so the bar was seen full */
        d->scan_pending--;
        d->scans++;
        snprintf(msg, sizeof msg, "Added %u songs to the library", (unsigned)si.scanned_files);
        desk_toast(d, msg); /* the page reload follows the library serial (desk_library_follow) */
        d->repaint = 1;
    }
    /* I550: the SOURCES reply that follows a LIBRARY_SYNC only means the request was queued, never that the sync worked.
     * The outcome is the engine's own completion: a path LIBRARY_RESULT (success) or ERROR sync_failed/sync_busy (failure). */
    if (evt->id == SEL_EVT_LIBRARY_RESULT && !src_row && d->sync_pending != 0u && evt->argc > 0 && evt->args[0].type == SEL_ARG_TAGS) {
        int row = sync_row_find(d, d->sync_pending, (const char*)sel_tag_find(evt->args[0].v.t, SEL_SRC_PATH, (uintptr_t)""));
        if (row >= 0) {
            sync_row_done(d, row, 0);
        }
    }
    if (evt->id == SEL_EVT_ERROR && d->sync_pending != 0u && evt->argc > 1 && evt->args[0].type == SEL_ARG_STR && evt->args[1].type == SEL_ARG_STR
        && (strcmp(evt->args[0].v.s, "sync_failed") == 0 || strcmp(evt->args[0].v.s, "sync_busy") == 0)) {
        int row = sync_row_find(d, d->sync_pending, evt->args[1].v.s);
        if (row >= 0) {
            sync_row_done(d, row, 1);
        }
    }
    if (evt->id == SEL_EVT_LIBRARY_RESULT && !src_row && !d->scan_pending && d->src_syncing[0] != '\0') {
        char msg[96];
        snprintf(msg, sizeof msg, "Synced %s", d->src_syncing);
        desk_toast(d, msg);
        d->src_syncing[0] = '\0';
        d->repaint = 1;
    }
    if (evt->id == SEL_EVT_ERROR && d->src_syncing[0] != '\0' && evt->argc > 0 && evt->args[0].type == SEL_ARG_STR
        && (strcmp(evt->args[0].v.s, "sync_failed") == 0 || strcmp(evt->args[0].v.s, "sync_busy") == 0)) {
        char msg[160];
        snprintf(msg, sizeof msg, "Cannot sync %s: %s", d->src_syncing, strcmp(evt->args[0].v.s, "sync_busy") == 0 ? "another sync is running" : "the source could not be fetched");
        desk_toast(d, msg);
        d->src_syncing[0] = '\0';
        d->repaint = 1;
    }
    if (evt->id == SEL_EVT_ERROR && d->scan_pending) { /* no_library, scan_failed or scan_busy */
        d->scan_pending--;
        d->repaint = 1;
        d->uim.dirty |= SEL_UIM_DIRTY_ALL; /* the progress bar turns back into the button */
        desk_toast(d, "Cannot add that folder");
        d->repaint = 1;
    }
    if (evt->id == SEL_EVT_ERROR && d->drop_playing && strcmp(evt->args[0].v.s, "open_failed") == 0) {
        d->drop_playing = 0;
        desk_toast(d, "That file cannot be played");
        d->repaint = 1;
    }
    sel_uim_update(&d->uim, sel_engine_state(d->engine));
}

/* The mem backend has no input source: a turn with something to draw is handled, a quiet turn ends the run. */
static int desk_wait_turn(desk* d, uint32_t timeout_ms);
static int desk_wait(void* state, uint32_t timeout_ms) {
    desk* d = (desk*)state;
    const sel_host* ch = sel_engine_plugin_host(d->engine);
    uint32_t t0 = ch->clock_ms(ch), dt;
    int r;
    d->poll_ms = 0u;
    r = desk_wait_turn(d, timeout_ms);
    dt = ch->clock_ms(ch) - t0;
    dt = dt > d->poll_ms ? dt - d->poll_ms : 0u;
    if (dt > d->turn_ms_max) {
        d->turn_ms_max = dt;
    }
    return r;
}

static int desk_wait_turn(desk* d, uint32_t timeout_ms) {
    int vz;
#ifndef SELECTA_DESKTOP_SDL
    (void)timeout_ms;
#endif
    if (d->testcard) {
        return -1; /* the test card is one frame */
    }
    if (d->quit_req) {
        return -1; /* the Quit Selecta dialog was confirmed: end the loop as a closed window does (I623) */
    }
    desk_library_follow(d);
    desk_playlist_job_follow(d);
    if (!d->nag_asked) { /* sync on launch (I527): the engine marks remote sources never synced or stale in its SOURCES rows; asked on the first turn, when the engine answers UI events */
        d->nag_asked = 1;
        d->src_wait = 1;
        desk_send(d, "SOURCES");
    }
    if (!d->nag_done && !d->dlg_open && d->page == PG_FOLDERS && !d->src_wait && d->frames > 0u &&
        nag_count(d) > 0u) { /* I602: the sync offer opens once per launch, on the first Library visit with a due source */
        nagdlg_open(d);
    }
    if (d->textsheet) { /* the text sheet is one frame, drawn on the first turn */
        if (d->frames > 0u) {
            return -1;
        }
        draw_textsheet(d);
        d->dirty_pixels = desk_flush(d);
        d->dirty_total = d->dirty_pixels;
        d->frames++;
        return 1;
    }
#ifdef SELECTA_DESKTOP_SDL
    if (d->sdl != NULL) {
        int32_t w = 0, h = 0;
        int ev = desk_sdl_poll(d->sdl, 0u, &w, &h);
        if (ev == DESK_SDL_QUIT) {
            return -1;
        }
        if (ev == DESK_SDL_RESIZE) {
            desk_set_density(d, desk_sdl_scale(d->sdl));
            desk_resize(d, w, h);
        }
        desk_sdl_route(d);
        /* I442: the UI timers (toast, tooltip delay, search debounce) run here for the same reason as the tweens below: a
         * turn that returns early during playback would otherwise never reach them, and a toast stayed on screen for good. */
        if (d->live && desk_timers_tick(d)) {
            overlay_refresh(d);
        }
        /* I341: a running tween is drawn on every turn, and the end state of one that ended is drawn once, whatever else the
         * turn does. During playback with a visualizer the turn returns early (new engine block, or a presented frame), so
         * this must not live in the idle branch below: the hover fade would never get a step frame and stayed at weight 0.
         * desk_sdl_route has just brought the clock up to date (desk_live_tick). */
        if (d->live && (sel_gui_timeline_running(&d->tl) > 0u || d->tween_ended)) {
            int ended = d->tween_ended;
            d->tween_ended = 0;
            /* The main region is repainted only for a tween that changes it (DUI-P3-8); the end state of any tween once. */
            if (d->home && (ended || hover_main_painted(d))) {
                draw_home(d);
            }
            hover_repaint(d);
            desk_event_frame(d, 1);
            if (d->toast_on) { /* the toast animates over the page: a full frame draws the overlay */
                overlay_refresh(d);
            }
        }
    }
#endif
    if (d->widgets) { /* the gallery is one frame, drawn on the first turn */
        if (d->frames > 0u) {
            return -1;
        }
        draw_widgets(d);
        d->dirty_pixels = desk_flush(d);
        d->dirty_total = d->dirty_pixels;
        d->frames++;
        return 1;
    }
    desk_scan_sync(d); /* I315: a running scan moves the Add Folder progress bar */
    if (lib_hash(d, sel_engine_state(d->engine)) != d->library_sig && (sel_engine_state(d->engine)->composers != NULL || d->page == PG_FOLDERS)) { /* I390: the style chooser shows the style even on an empty library */
        d->library_sig = lib_hash(d, sel_engine_state(d->engine)); /* the Library page changed: redraw */
        d->uim.dirty |= SEL_UIM_DIRTY_ALL;
    }
    if (d->tr_page) {
        tr_tick(d);
    }
    if (d->si_page) { /* the metadata and panel rows arrive after the song opens: redraw when they change */
        d->si_acc = 2166136261u;
        si_walk_from_top(d, sel_engine_state(d->engine), si_hash_row);
        if (d->si_acc != d->si_sig) {
            d->si_sig = d->si_acc;
            if (d->si_y_q16 > si_max_q16(d)) { /* a shorter list (a new song): keep the scroll inside it */
                d->si_y_q16 = si_max_q16(d);
            }
            d->uim.dirty |= SEL_UIM_DIRTY_ALL;
        }
    }
    if (d->pls_stale && d->pls_id != 0u) { /* I633: a song left the open playlist: redraw so the page reads the list again */
        d->uim.dirty |= SEL_UIM_DIRTY_ALL;
    }
    if (d->pls_id != 0u && d->search_on && sel_engine_state(d->engine)->view != NULL) { /* I652a: the empty playlist's search result arrived or changed: redraw */
        const sel_view_state* fv = sel_engine_state(d->engine)->view;
        uint32_t fsig = fv->source == SEL_VIEW_SEARCH ? 1u + fv->row_count * 64u + fv->row_n : 0u;
        if (fsig != d->pls_find_sig) {
            d->pls_find_sig = fsig;
            d->repaint = 1;
            d->uim.dirty |= SEL_UIM_DIRTY_ALL;
        }
    }
    if (d->home && sel_engine_state(d->engine)->view != NULL) { /* HOME rows arrived or changed: redraw */
        const sel_view_state* hv = sel_engine_state(d->engine)->view;
        uint32_t sig = hv->source == SEL_VIEW_HOME ? 1u + hv->row_n : d->search_on && hv->source == SEL_VIEW_SEARCH ? 0x10000u + hv->row_count * 64u + hv->row_n : 0u;
        if (hv->source == SEL_VIEW_FOLDERS && (d->pl_page || hv->row_n > 0u)) { /* the saved playlists arrived or changed */
            sig = 0x20000u + pl_hash(hv);
        }
        if (hv->source == SEL_VIEW_FOLDERS) { /* I608: the sidebar list follows the saved playlists */
            d->side_stale = 1u;
        }
        if (sig != d->home_sig) {
            d->home_sig = sig;
            d->uim.dirty |= SEL_UIM_DIRTY_ALL;
        }
    }
    sel_uim_update(&d->uim, sel_engine_state(d->engine));
    if (d->ticks > 0u && d->uim.dirty == 0u) { /* --ticks N: the mem backend has no clock, so it replays N seconds */
        d->ticks--;
        d->uim.dirty |= SEL_UIM_DIRTY_PROGRESS;
    }
    vz = viz_tick(d); /* one visualizer frame per new engine block; the turn goes on, so ticks and the script still advance */
    {
        uint32_t pend = d->uim.dirty, k;
        if (desk_present(d)) {
            d->turn_why[1]++;
            for (k = 0u; k < 6u; k++) {
                d->turn_bits[k] += (pend >> k) & 1u;
            }
            return 1;
        }
    }
    if (!d->live && (d->scan_pending || desk_scan_active(d))) { /* I336: the scan runs on a job thread; a scripted or headless run keeps turning (and drawing the
                                        * progress) until its LIBRARY_RESULT arrives, so the script's next line sees the finished scan */
        desk_scan_nap();
        d->turn_why[2]++;
        return 1;
    }
    if (d->input != NULL && !d->input_done && d->ticks == 0u && desk_replay_input(d)) { /* scripted input, once */
        d->turn_why[3]++;
        return 1; /* every event flushed its own frame */
    }
    if (vz && !d->live) { /* I525: a live window goes on to the poll below, which waits up to the next engine block (20 ms)
                           * for input; returning here skipped every wait during playback, so on AGA each turn ran c2p and
                           * WaitTOF back to back at full CPU, clicks queued behind it and the other tasks starved */
        d->turn_why[4]++;
        return 1;
    }
#ifdef SELECTA_DESKTOP_SDL
    if (d->live) { /* a window on a real display: idle until an event or the next second boundary */
        int32_t w = 0, h = 0;
        int ev;
        uint32_t wait_ms;
        wait_ms = 1000u - d->now_ms % 1000u;
        if (sel_gui_timeline_running(&d->tl) > 0u) { /* a tween runs: the next step is due in 16 ms (the frame itself is drawn at the top of the turn) */
            wait_ms = 16u;
        }
        if (viz_visible(d) != 0 && wait_ms > 20u) { /* a visualizer on screen: one frame per engine block */
            wait_ms = 20u;
        }
        if (desk_timers_wait(d) > 0u && desk_timers_wait(d) < wait_ms) {
            wait_ms = desk_timers_wait(d);
        }
        (void)timeout_ms;
        d->turn_why[0]++;
        {
            uint32_t p0 = desk_sdl_ticks();
            ev = desk_sdl_poll(d->sdl, wait_ms, &w, &h);
            d->poll_ms += desk_sdl_ticks() - p0;
        }
        if (ev == DESK_SDL_QUIT) {
            return -1;
        }
        if (ev == DESK_SDL_RESIZE) {
            desk_set_density(d, desk_sdl_scale(d->sdl));
            desk_resize(d, w, h);
        }
        desk_sdl_route(d);
        return ev == DESK_SDL_NONE ? 0 : 1;
    }
#endif
    if (sel_gui_timeline_running(&d->tl) > 0u) { /* the run ends while a tween runs: show the end state once */
        sel_gui_timeline_finish(&d->tl);
        if (d->home) {
            draw_home(d);
        }
        desk_event_frame(d, 1);
        return 1;
    }
    return -1;
}

static const sel_tag* desk_stats(void* state) {
    desk* d = (desk*)state;
    d->stats[0].tag = SEL_UI_STAT_FRAMES;
    d->stats[0].value = d->frames;
    d->stats[1].tag = SEL_UI_STAT_HASH;
    d->stats[1].value = d->hash;
    d->stats[2].tag = SEL_UI_STAT_WIDGET_DRAWS;
    d->stats[2].value = d->widget_draws;
    d->stats[3].tag = SEL_UI_STAT_UIM_UPDATES;
    d->stats[3].value = d->uim.updates;
    d->stats[4].tag = SEL_UI_STAT_UIM_DIRTY;
    d->stats[4].value = d->uim.last_taken;
    d->stats[5].tag = SEL_UI_STAT_UIM_LAYOUT;
    d->stats[5].value = d->uim.layout;
    d->stats[6].tag = SEL_UI_STAT_TWEENS;
    d->stats[6].value = d->tweens;
    d->stats[7].tag = SEL_UI_STAT_EFFECT_PRIMS;
    d->stats[7].value = d->effect_prims;
    d->stats[8].tag = SEL_UI_STAT_DIRTY_PIXELS;
    d->stats[8].value = d->dirty_pixels;
    d->stats[9].tag = SEL_UI_STAT_DIRTY_TOTAL;
    d->stats[9].value = d->dirty_total;
    d->stats[10].tag = SEL_UI_STAT_CANVAS_HASH;
    sel_gui_hash_hex(sel_gui_hash(d->canvas), d->canvas_hash_hex);
    d->stats[10].value = (uintptr_t)d->canvas_hash_hex;
    d->stats[11].tag = SEL_UI_STAT_DRAW_PRIMS;
    d->stats[11].value = d->canvas->prims;
    d->stats[12].tag = SEL_UI_STAT_MISSING_GLYPHS;
    d->stats[12].value = d->text.missing;
    d->stats[13].tag = SEL_UI_STAT_GLYPHS_DRAWN;
    d->stats[13].value = d->text.glyphs;
    d->stats[14].tag = SEL_UI_STAT_VIEW_OPENS;
    d->stats[14].value = d->view_opens;
    d->stats[15].tag = SEL_UI_STAT_ART_DECODES;
    d->stats[15].value = art_decodes(d->art);
    d->stats[16].tag = SEL_UI_STAT_ART_TILE_HASH;
    sel_gui_hash_hex(d->art_tile_hash, d->art_tile_hash_hex);
    d->stats[16].value = (uintptr_t)d->art_tile_hash_hex;
    sel_gui_hash_hex(0u, d->present_hash_hex);
    d->stats[17].value = (uintptr_t)d->present_hash_hex;
    d->stats[17].tag = SEL_UI_STAT_PRESENT_HASH;
    d->stats[18].tag = SEL_UI_STAT_UPLOAD_PIXELS;
    d->stats[19].tag = SEL_UI_STAT_PRESENTS;
#ifdef SELECTA_DESKTOP_SDL
    if (d->sdl != NULL) {
        sel_gui_hash_hex(desk_sdl_present_hash(d->sdl), d->present_hash_hex);
        d->stats[18].value = desk_sdl_upload_pixels(d->sdl);
        d->stats[19].value = desk_sdl_presents(d->sdl);
    }
#endif
    d->stats[20].tag = SEL_UI_STAT_SCALE;
    d->stats[20].value = (uintptr_t)d->scale;
    d->stats[21].tag = SEL_UI_STAT_ATLAS_REBUILDS;
    d->stats[21].value = d->atlas_rebuilds;
    d->stats[22].tag = SEL_UI_STAT_ICON_REBUILDS;
    d->stats[22].value = d->icon_rebuilds;
    d->stats[23].tag = SEL_UI_STAT_ATLAS_BUILDS;
    d->stats[23].value = d->atlas_builds;
    d->stats[24].tag = SEL_UI_STAT_ACTIONS_FIRED;
    d->stats[24].value = d->actions_fired;
    d->stats[25].tag = SEL_UI_STAT_ACTIONS_DISTINCT;
    d->stats[25].value = d->actions_distinct;
    d->stats[26].tag = SEL_UI_STAT_FOCUS_MOVES;
    d->stats[26].value = d->focus_moves;
    d->stats[27].tag = SEL_UI_STAT_SEARCHES;
    d->stats[27].value = d->searches;
    d->stats[28].tag = SEL_UI_STAT_ARROWS;
    d->stats[28].value = d->arrows;
    d->stats[29].tag = SEL_UI_STAT_FOCUS;
    d->stats[29].value = (uintptr_t)(d->focus + 1);
    d->stats[30].tag = SEL_UI_STAT_HOVERS;
    d->stats[30].value = d->hovers;
    d->stats[31].tag = SEL_UI_STAT_CLICKS;
    d->stats[31].value = d->clicks;
    d->stats[32].tag = SEL_UI_STAT_DOUBLE_CLICKS;
    d->stats[32].value = d->double_clicks;
    d->stats[33].tag = SEL_UI_STAT_DRAGS;
    d->stats[33].value = d->drags;
    d->stats[34].tag = SEL_UI_STAT_SORTS;
    d->stats[34].value = d->sorts;
    d->stats[35].tag = SEL_UI_STAT_SORT_DIR;
    d->stats[35].value = d->sort_dir;
    d->stats[36].tag = SEL_UI_STAT_TOP_ROW;
    d->stats[36].value = (d->order_n > 0u ? (uintptr_t)d->order[0] + 1u : 0u);
    d->stats[37].tag = SEL_UI_STAT_MENUS;
    d->stats[37].value = d->menus;
    d->stats[38].tag = SEL_UI_STAT_ROW_PLAYS;
    d->stats[38].value = d->row_plays;
    d->stats[39].tag = SEL_UI_STAT_TOOLTIPS;
    d->stats[39].value = d->tooltips;
    d->stats[40].tag = SEL_UI_STAT_TOOLTIP_TEXT;
    d->stats[40].value = (uintptr_t)(d->tip_shown > 0 ? d->tip_text : "");
    d->stats[41].tag = SEL_UI_STAT_GESTURES;
    d->stats[41].value = d->gestures;
    d->stats[42].tag = SEL_UI_STAT_TILE_SIZE;
    d->stats[42].value = (uintptr_t)sel_uim_tile_step(d->tile_step);
    d->stats[43].tag = SEL_UI_STAT_SCROLL_PX;
    d->stats[43].value = (uintptr_t)(d->scroll_q16 >> 16);
    d->stats[44].tag = SEL_UI_STAT_OPEN_TILE;
    d->stats[44].value = (uintptr_t)d->open_tile;
    d->stats[45].tag = SEL_UI_STAT_TRANSPORT_HITS;
    d->stats[45].value = d->transport_hits;
    d->stats[46].tag = SEL_UI_STAT_PLAY_STATE;
    d->stats[46].value = (uintptr_t)sel_engine_state(d->engine)->play;
    d->stats[47].tag = SEL_UI_STAT_REPEAT_MODE;
    d->stats[47].value = d->uim.frame.repeat_mode;
    d->stats[48].tag = SEL_UI_STAT_RANDOM_ON;
    d->stats[48].value = d->uim.frame.random_on;
    d->stats[49].tag = SEL_UI_STAT_SUBSONG;
    d->stats[49].value = d->uim.frame.subsong;
    d->stats[50].tag = SEL_UI_STAT_FRAME_PARTS;
    d->stats[50].value = d->frame_parts;
    d->stats[51].tag = SEL_UI_STAT_HOME_PARTS;
    d->stats[51].value = d->home_parts;
    d->stats[52].tag = SEL_UI_STAT_TILE_CARDS;
    d->stats[52].value = d->tile_cards;
    d->stats[53].tag = SEL_UI_STAT_HOME_SHELVES;
    d->stats[53].value = home_dump_build(d);
    d->stats[54].tag = SEL_UI_STAT_SHELF_SCROLL;
    d->stats[54].value = (uintptr_t)(((uint32_t)shelf_pos_q16(d, 0u) >> 16) | (((uint32_t)shelf_pos_q16(d, 1u) >> 16) << 16));
    d->stats[55].tag = SEL_UI_STAT_HOME_DUMP;
    d->stats[55].value = (uintptr_t)d->home_dump;
    d->stats[56].tag = SEL_UI_STAT_HOME_SCROLL_Y;
    d->stats[56].value = (uintptr_t)(uint32_t)(d->home_y_q16 >> 16);
    d->stats[57].tag = SEL_UI_STAT_TILE_QUEUED;
    d->stats[57].value = d->tile_queued;
    d->stats[58].tag = SEL_UI_STAT_LIBRARY_DUMP;
    d->stats[58].value = (uintptr_t)library_dump_build(d);
    d->stats[59].tag = SEL_UI_STAT_HOME_EMPTY;
    d->stats[59].value = (uintptr_t)home_empty(sel_engine_state(d->engine));
    d->stats[60].tag = SEL_UI_STAT_LIBRARY_SEARCHES;
    d->stats[60].value = d->lib_searches;
    d->stats[61].tag = SEL_UI_STAT_META_PANEL_ROWS;
    d->stats[61].value = d->np_rows;
    d->stats[62].tag = SEL_UI_STAT_FOLDER_OPENS;
    d->stats[62].value = d->folder_opens;
    d->stats[63].tag = SEL_UI_STAT_VIZ_FRAMES;
    d->stats[63].value = d->viz_frames;
    d->stats[64].tag = SEL_UI_STAT_DROPS;
    d->stats[64].value = d->drops;
    d->stats[65].tag = SEL_UI_STAT_SCANS;
    d->stats[65].value = d->scans;
    d->stats[66].tag = SEL_UI_STAT_BACKEND;
    d->stats[66].value = (uintptr_t)"mem";
    d->stats[67].tag = SEL_UI_STAT_SCREEN_DEPTH;
    d->stats[67].value = 0;
#ifdef SELECTA_DESKTOP_SDL
    if (d->sdl != NULL) {
        d->stats[66].value = (uintptr_t)DESK_BACKEND_NAME;
        d->stats[67].value = (uintptr_t)desk_backend_depth(d->sdl);
    }
#endif
    d->stats[68].tag = SEL_UI_STAT_KEYS; /* keys the window delivered, for the one-line check after Ctrl-C */
    d->stats[68].value = d->keys_in;
    d->stats[69].tag = SEL_UI_STAT_PALETTE_COLOURS_UI;
    d->stats[69].value = sel_gui_region_colours(d->canvas, SEL_GUI_RGN_UI);
    d->stats[70].tag = SEL_UI_STAT_PALETTE_COLOURS_ART;
    d->stats[70].value = sel_gui_region_colours(d->canvas, SEL_GUI_RGN_ART);
    d->stats[71].tag = SEL_UI_STAT_PALETTE_COLOURS_TILE;
    d->stats[71].value = sel_gui_region_colours(d->canvas, SEL_GUI_RGN_TILE);
    d->stats[72].tag = SEL_UI_STAT_PALETTE_COLOURS_PANEL;
    d->stats[72].value = sel_gui_region_colours(d->canvas, SEL_GUI_RGN_PANEL);
    d->stats[73].tag = SEL_UI_STAT_PALETTE_COLOURS_FLOW;
    d->stats[73].value = sel_gui_region_colours(d->canvas, SEL_GUI_RGN_FLOW);
    d->stats[74].tag = SEL_UI_STAT_QUANT_CALLS;
    d->stats[74].value = d->quant_calls;
    d->stats[75].tag = SEL_UI_STAT_ART_MS_MAX;
    d->stats[75].value = d->art_ms_max;
    d->stats[76].tag = SEL_UI_STAT_TURN_MS_MAX;
    d->stats[76].value = d->turn_ms_max;
    d->stats[77].tag = SEL_UI_STAT_FRAME_MS_MIN;
    d->stats[77].value = d->frame_ms_n != 0u ? d->frame_ms_min : 0u;
    d->stats[78].tag = SEL_UI_STAT_FRAME_MS_AVG;
    d->stats[78].value = d->frame_ms_n != 0u ? d->frame_ms_sum / d->frame_ms_n : 0u;
    d->stats[79].tag = SEL_UI_STAT_FRAME_MS_MAX;
    d->stats[79].value = d->frame_ms_max;
    d->stats[80].tag = SEL_UI_STAT_PAGE_DIRTY;
    d->stats[80].value = d->page_dirty;
    d->stats[81].tag = SEL_UI_STAT_PAINT_MS_MIN;
    d->stats[81].value = d->frame_ms_n != 0u ? d->paint_ms_min : 0u;
    d->stats[82].tag = SEL_UI_STAT_PAINT_MS_AVG;
    d->stats[82].value = d->frame_ms_n != 0u ? d->paint_ms_sum / d->frame_ms_n : 0u;
    d->stats[83].tag = SEL_UI_STAT_PAINT_MS_MAX;
    d->stats[83].value = d->paint_ms_max;
    d->stats[84].tag = SEL_UI_STAT_PRESENT_MS_MIN;
    d->stats[84].value = d->frame_ms_n != 0u ? d->present_ms_min : 0u;
    d->stats[85].tag = SEL_UI_STAT_PRESENT_MS_AVG;
    d->stats[85].value = d->frame_ms_n != 0u ? d->present_ms_sum / d->frame_ms_n : 0u;
    d->stats[86].tag = SEL_UI_STAT_PRESENT_MS_MAX;
    d->stats[86].value = d->present_ms_max;
    {
        uint32_t k, n = 87u;
        for (k = 0; k < SEL_GUI_PROF_COUNT; k++) {
            d->stats[n].tag = SEL_UI_STAT_PROF_MS_FILL + k;
            d->stats[n++].value = d->canvas != NULL ? d->canvas->prof_ms[k] : 0u;
            d->stats[n].tag = SEL_UI_STAT_PROF_N_FILL + k;
            d->stats[n++].value = d->canvas != NULL ? d->canvas->prof_n[k] : 0u;
        }
        for (k = 0; k < PT_COUNT; k++) {
            d->stats[n].tag = SEL_UI_STAT_PT_MS_CANVAS + k;
            d->stats[n++].value = d->pt_ms[k];
        }
        for (k = 0; k < SEL_GUI_PROF_COUNT; k++) {
            d->stats[n].tag = SEL_UI_STAT_FLOW_MS_REST + k;
            d->stats[n++].value = d->fl_prof[0][k];
            d->stats[n].tag = SEL_UI_STAT_FLOW_MS_SLIDE + k;
            d->stats[n++].value = d->fl_prof[1][k];
        }
        for (k = 0; k < 2u; k++) {
            d->stats[n].tag = SEL_UI_STAT_FLOW_N_REST + 3u * k;
            d->stats[n++].value = d->fl_n[k];
            d->stats[n].tag = SEL_UI_STAT_FLOW_PAINT_REST + 3u * k;
            d->stats[n++].value = d->fl_paint[k];
            d->stats[n].tag = SEL_UI_STAT_FLOW_MAX_REST + 3u * k;
            d->stats[n++].value = d->fl_max[k];
        }
        d->stats[n].tag = SEL_UI_STAT_SPAN_PIXELS;
        d->stats[n++].value = d->canvas != NULL ? d->canvas->span_px : 0u;
        d->stats[n].tag = SEL_UI_STAT_READBACKS;
        d->stats[n].value = 0u;
#ifdef SELECTA_DESKTOP_SDL
        if (d->sdl != NULL) {
            d->stats[n].value = desk_sdl_readbacks(d->sdl);
        }
#endif
        n++;
        d->stats[n].tag = SEL_UI_STAT_SCALE_Q16;
        d->stats[n++].value = (uintptr_t)d->scale_q16;
        if (d->zoom_pct == 0) {
            strcpy(d->zoom_str, "fit");
        } else {
            snprintf(d->zoom_str, sizeof d->zoom_str, "%d", (int)d->zoom_pct);
        }
        d->stats[n].tag = SEL_UI_STAT_LOGICAL_W;
        d->stats[n++].value = (uintptr_t)px_to_logical(d, d->canvas->w);
        d->stats[n].tag = SEL_UI_STAT_LOGICAL_H;
        d->stats[n++].value = (uintptr_t)px_to_logical(d, cvh(d));
        d->stats[n].tag = SEL_UI_STAT_ZOOM;
        d->stats[n++].value = (uintptr_t)d->zoom_str;
        d->stats[n].tag = SEL_UI_STAT_ICON_RESAMPLED;
        d->stats[n++].value = d->icon_resampled;
        d->stats[n].tag = SEL_UI_STAT_ATLAS_MS;
        d->stats[n++].value = d->atlas_ms;
        d->stats[n].tag = SEL_UI_STAT_ATLAS_BAKED_HITS;
        d->stats[n++].value = d->atlas_baked_hits;
        d->stats[n].tag = SEL_UI_STAT_BITMAP_HITS_MINI4;
        d->stats[n++].value = d->bitmap_hits[0];
        d->stats[n].tag = SEL_UI_STAT_BITMAP_HITS_MINI6;
        d->stats[n++].value = d->bitmap_hits[1];
        d->stats[n].tag = SEL_UI_STAT_BITMAP_HITS_MINI8;
        d->stats[n++].value = d->bitmap_hits[2];
        d->stats[n].tag = SEL_UI_STAT_HEADLINE_HITS_18;
        d->stats[n++].value = d->headline_hits[0];
        d->stats[n].tag = SEL_UI_STAT_HEADLINE_HITS_24;
        d->stats[n++].value = d->headline_hits[1];
        d->stats[n].tag = SEL_UI_STAT_SHELF_ALBUM_ART_DRAWS;
        d->stats[n++].value = d->shelf_album_art_draws;
        d->stats[n].tag = SEL_UI_STAT_TOOLTIP_TARGETS;
        d->stats[n++].value = desk_tip_targets(d);
        d->stats[n].tag = SEL_TAG_END;
    }

    return d->stats;
}

static sel_tag g_hooks[8];
static sel_tag g_desc[5];

static void set(sel_tag* t, uint32_t tag, uintptr_t value) {
    t->tag = tag;
    t->value = value;
}

const sel_tag* SEL_PLUGIN_ENTRY(uint32_t host_api) {
    if (host_api < 1u) {
        return NULL;
    }
    set(&g_hooks[0], SEL_UI_NAME, (uintptr_t) "Desktop");
    set(&g_hooks[1], SEL_UI_OPEN, SEL_TAG_FN(desk_open));
    set(&g_hooks[2], SEL_UI_ON_EVENT, SEL_TAG_FN(desk_on_event));
    set(&g_hooks[3], SEL_UI_WAIT, SEL_TAG_FN(desk_wait));
    set(&g_hooks[4], SEL_UI_CLOSE, SEL_TAG_FN(desk_close));
    set(&g_hooks[5], SEL_UI_STATS, SEL_TAG_FN(desk_stats));
    set(&g_hooks[6], SEL_UI_VIEW_STYLES, SEL_VIEW_STYLES_ALL);
    set(&g_hooks[7], SEL_TAG_END, 0);

    set(&g_desc[0], SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_UI);
    set(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "desktop");
    set(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    set(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    set(&g_desc[4], SEL_TAG_END, 0);
    return g_desc;
}
