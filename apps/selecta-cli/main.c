/* selecta-cli: the top-level entry point every M0 test drives (design doc
 * section 12). Subcommands exercise the core through the same calls a
 * front end makes. */
#ifdef __amigaos__
#include "amiga_start.h"
#endif
#include <selecta/api.h>
#include <selecta/engine.h>
#include <selecta/machine.h>
#include <selecta/protocol.h>
#include <selecta/registry.h>
#include <selecta/tags.h>

#include "catalogue.h"
#include "cli.h"
#include "db.h"
#include "integration_host.h"
#include "share.h"
#include "stream_open.h"
#ifndef SEL_NO_CONTROL_SOCKET
#include "ctlsock.h"
#endif

#ifndef __amigaos__
#include <pthread.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef __amigaos__
#include <time.h>
#include <unistd.h>
#endif

static int usage(FILE* to) {
    fputs("usage: selecta-cli <command> [args]\n"
          "  version                    print version and api level\n"
          "  commands                   list command verbs and signatures\n"
          "  events                     list event verbs and signatures\n"
          "  tagtypes                   list metadata tags and their types\n"
          "  roundtrip                  encode and decode every table row\n"
          "  commands --unimplemented   list verbs the engine answers not_implemented\n"
          "  script <file> [--integration <id>] [--catalogue <tsv> [--db <path>]]  feed command lines to the engine, print events\n"
          "  send <verb> [args]         send one command line to a running selecta (exit 0 OK, 1 ERROR, 2 none running)\n"
          "  abi [voices]               print sel_host (or sel_voices) size and field offsets\n"
          "  plugins [--lint]           list plugins; --lint counts bad descriptor tags\n"
          "  plugins --machine <name> --stats  list what a fake machine runs (a500, a1200, a1200-fpu, a060)\n"
          "  plugins --formats <id>     list the formats (player ids) a plugin reports\n"
          "  play-null --rate <hz> --frames <n>  pump the null output, print counters\n"
          "  play <song> --output <id> --seconds <n> [--record <dir>]  play through a real output, print counters\n"
          "  render <song> --output <wav> --seconds <n> --rate <hz> --resample <name>\n"
          "                             render a song to a WAV file, print counters\n"
          "  library browse <dir>       list a directory through LIBRARY_BROWSE, print dir_entries=\n"
          "  library search --catalogue <tsv> <q> run a library query through LIBRARY_SEARCH, print key= src_count= fts_queries= (T64)\n"
          "  analyse [--db <p>] <song>     loudness analysis (R128), stores the gain; --sine <hz>:<dbfs> for a reference (I187, T89)\n"
          "  library analyse --db <p> ['source:local']  analyse every found song (I187)\n"
          "  loudness [--db <p>] [--machine <name>] <song>  gain from store, DEViLBOX or analysis, one PUT after analysis (I188, T90)\n"
          "  library scan [--db <p>] <dir> scan a folder into the library DB, print scanned_files= db_songs= (T71)\n"
          "  library key <source> <path> print the song key and its URL round trip (T72)\n"
          "  library query --explain <q> compile a library query, print sql= params= query_terms= (T63)\n"
          "  library export-index --db <p> --out <f> <source>  copy a source into a read-only index file; search and stats take --index <f> (I197, T96)\n"
          "  library stats              open the library DB, print db_open= schema_version= sqlite_version= (T62)\n"
          "  userdata --db <p> [--catalogue <tsv>] [--note <song> <text>] [--tag <song> <field> <value>] [--wrong <song> <note>]  notes, tag edits, row counts (T78)\n"
          "  wrong [--db <p>] [--export <file>] [<song> [<note>]]  flag a song as sounding wrong, export sounds-wrong.txt (T78)\n"
          "  devilbox login <user> [--password-stdin] [--config-dir <dir>]  log in to DEViLBOX, store the token (password from stdin) (I173)\n"
          "  record <url> <file> [--bytes N] [--seconds S]  stream-to-disk: write the raw stream bytes as they arrive\n"
          "  fetch <url> [--method M] [--range a-b] [--out file]  one HTTP request, print http_status= and net counters\n"
          "  share make song|playlist|view ...  build a share link, print both forms and the parse round trip (T84)\n"
          "  share parse <link>         parse a selecta:// or DEViLBOX link, print its parts (exit 2: ERROR bad_link)\n"
          "  share song|position <song> [--at <ms>] [--catalogue <tsv>]  play a song, ask SHARE for its link\n"
          "  open <link> [--catalogue <tsv>]  OPEN a share link, render a little, print SHARE counters (T84)\n"
          "  machine [--tier <name>]    probe the machine, print the sel_machine tags\n",
          to);
    return to == stdout ? 0 : 2;
}

static int cmd_version(void) {
    printf("selecta-cli %s\n", sel_version_string());
    return 0;
}

static int cmd_commands(int unimplemented_only) {
    uint32_t i;
    for (i = 0; i < SEL_CMD_COUNT; i++) {
        if (unimplemented_only) {
            if (!sel_engine_cmd_implemented(i)) {
                printf("%s\n", sel_cmd_name(i));
            }
        } else {
            printf("%s %s area=%s\n", sel_cmd_name(i), sel_cmd_args(i), sel_cmd_area(i));
        }
    }
    return 0;
}

static int cmd_events(void) {
    uint32_t i;
    for (i = 0; i < SEL_EVT_COUNT; i++) {
        printf("%s %s\n", sel_evt_name(i), sel_evt_args(i));
    }
    return 0;
}

static const char* tag_type_name(sel_tag_type t) {
    switch (t) {
    case SEL_TT_INT:
        return "int";
    case SEL_TT_STR:
        return "string";
    case SEL_TT_PTR:
        return "pointer";
    case SEL_TT_LIST:
        return "list";
    default:
        return "unknown";
    }
}

/* config-dir: prints the directory sel_config_dir picks (I214 sentinel: on the Amiga it is
 * ENVARC:Selecta, or PROGDIR:Selecta when ENVARC: is unassigned). */
static int cmd_config_dir(void) {
    char dir[256];
    if (!sel_config_dir(dir, sizeof dir)) {
        fprintf(stderr, "selecta-cli: no config directory\n");
        return 1;
    }
    printf("config_dir=%s\n", dir);
    return 0;
}

static int cmd_tagtypes(void) {
    uint32_t count, i;
    const sel_tag_info* table = sel_tag_table(&count);
    for (i = 0; i < count; i++) {
        if ((table[i].tag & 0xF000u) == 0x8000u) {
            printf("%s %s\n", table[i].name, tag_type_name(table[i].type));
        }
    }
    return 0;
}

/* Builds a sample message for a signature, so every letter and the
 * optional and repeat markers are exercised. */
static const sel_tag k_sample_tags[] = {
    {SEL_META_TITLE, (uintptr_t) "Space Debris"},
    {SEL_META_AUTHOR, (uintptr_t) "Captain"},
    {SEL_META_YEAR, 1991},
    {SEL_META_MESSAGE, (uintptr_t) "say \"hi\", {ok}"},
    {SEL_TAG_END, 0},
};

static void fill_sample(sel_message* m, uint32_t kind, uint32_t id, const char* sig) {
    size_t n = strlen(sig);
    size_t i;
    int repeat = 0;
    memset(m, 0, sizeof *m);
    m->kind = kind;
    m->id = id;
    if (n && sig[n - 1] == '?') {
        n--;
    } else if (n && sig[n - 1] == '*') {
        n--;
        repeat = 1;
    }
    for (i = 0; i < n + (repeat ? 2u : 0u) && i < SEL_MAX_ARGS; i++) {
        char letter = i < n ? sig[i] : sig[n - 1];
        sel_arg* a = &m->args[i];
        a->type = (uint8_t)letter;
        switch (letter) {
        case 's':
            a->v.s = (i % 2) ? "plain" : "with space \"and\" quote";
            break;
        case 'i':
            a->v.i = (i % 2) ? 7 : -5;
            a->flags = (i % 2) ? 0 : SEL_ARG_RELATIVE;
            break;
        case 'u':
            a->v.u = 42u + (uint32_t)i;
            break;
        case 't':
            a->v.t = k_sample_tags;
            break;
        default:
            break;
        }
        m->argc++;
    }
}

static int tags_equal(const sel_tag* a, const sel_tag* b) {
    const sel_tag* ia = a;
    const sel_tag* ib = b;
    for (;;) {
        const sel_tag* x = sel_tag_next(&ia);
        const sel_tag* y = sel_tag_next(&ib);
        sel_tag_type type = SEL_TT_PTR;
        if (x == NULL || y == NULL) {
            return x == y;
        }
        if (x->tag != y->tag) {
            return 0;
        }
        sel_tag_type_of(x->tag, &type);
        if (type == SEL_TT_STR) {
            if (strcmp((const char*)x->value, (const char*)y->value) != 0) {
                return 0;
            }
        } else if (x->value != y->value) {
            return 0;
        }
    }
}

static int messages_equal(const sel_message* a, const sel_message* b) {
    uint32_t i;
    if (a->kind != b->kind || a->id != b->id || a->argc != b->argc) {
        return 0;
    }
    for (i = 0; i < a->argc; i++) {
        const sel_arg* x = &a->args[i];
        const sel_arg* y = &b->args[i];
        if (x->type != y->type) {
            return 0;
        }
        switch (x->type) {
        case SEL_ARG_STR:
            if (strcmp(x->v.s, y->v.s) != 0) {
                return 0;
            }
            break;
        case SEL_ARG_INT:
            if (x->v.i != y->v.i || x->flags != y->flags) {
                return 0;
            }
            break;
        case SEL_ARG_UINT:
            if (x->v.u != y->v.u) {
                return 0;
            }
            break;
        case SEL_ARG_TAGS:
            if (!tags_equal(x->v.t, y->v.t)) {
                return 0;
            }
            break;
        default:
            return 0;
        }
    }
    return 1;
}

static int roundtrip_one(uint32_t kind, uint32_t id, const char* name, const char* sig) {
    sel_message in, back;
    char line[512], again[512];
    char pool[1024];
    int n, rc;
    fill_sample(&in, kind, id, sig);
    n = sel_codec_encode(&in, line, sizeof line);
    if (n < 0) {
        fprintf(stderr, "[ERROR] %s: encode failed\n", name);
        return 0;
    }
    rc = sel_codec_decode(line, kind, &back, pool, sizeof pool);
    if (rc != SEL_CODEC_OK) {
        fprintf(stderr, "[ERROR] %s: decode %s on: %s\n", name, sel_codec_strerror(rc), line);
        return 0;
    }
    if (!messages_equal(&in, &back)) {
        fprintf(stderr, "[ERROR] %s: decoded message differs: %s\n", name, line);
        return 0;
    }
    if (sel_codec_encode(&back, again, sizeof again) < 0 || strcmp(line, again) != 0) {
        fprintf(stderr, "[ERROR] %s: re-encode differs:\n  %s\n  %s\n", name, line, again);
        return 0;
    }
    return 1;
}

static int cmd_roundtrip(void) {
    uint32_t i, ok = 0;
    for (i = 0; i < SEL_CMD_COUNT; i++) {
        ok += (uint32_t)roundtrip_one(SEL_MSG_CMD, i, sel_cmd_name(i), sel_cmd_args(i));
    }
    for (i = 0; i < SEL_EVT_COUNT; i++) {
        ok += (uint32_t)roundtrip_one(SEL_MSG_EVT, i, sel_evt_name(i), sel_evt_args(i));
    }
    printf("commands=%u events=%u ok=%u\n", (unsigned)SEL_CMD_COUNT, (unsigned)SEL_EVT_COUNT,
           (unsigned)ok);
    return ok == SEL_CMD_COUNT + SEL_EVT_COUNT ? 0 : 1;
}

/* The integration plugin of a script run (--integration), or NULL. */
static sel_int_session* g_int;

/* Prints one event as the wire line the codec produces, then hands it to the
 * integration plugin. */
static void print_event(void* ctx, const sel_message* evt) {
    char line[1024];
    (void)ctx;
    if (sel_codec_encode(evt, line, sizeof line) < 0) {
        fprintf(stderr, "selecta-cli: event %u does not encode\n", (unsigned)evt->id);
        return;
    }
    printf("%s\n", line);
    sel_int_session_event(g_int, evt);
}

/* @cells order row channels rows prints TRACKER_INFO and the CELL lines.
 * Script directives (lines starting with '@') are not protocol verbs; they let
 * a script observe and drive what a front end and an audio thread would:
 *   @queue              QUEUE version=V playing=ID ids=a,b,c
 *   @results            RESULTS n key,key,... (the last LIBRARY_SEARCH, PLAY_QUERY or QUEUE_ADD_QUERY result; T65)
 *   @library_stats      LIBRARY searches=N fts_queries=N songs_opened=N history_writes=N skip_excluded=N (T65, T78)
 *   @dup_stats          DUP redirects=N played=<key> (OPENs that played the best source of the same md5, D-M3-14, T86)
 *   @render N           render N frames on this thread, then pump and print
 *   @fade_mid           render until the end fade runs, then to its midpoint; print FADE gain_q16 pos total
 *   @render_until_end   render until SONG_ENDED is pumped; POSITION events
 *   @dump <file>|off    write every rendered block as raw s16 stereo to <file> (I189a)
 *   @stats              print STATS xfade_frames slots_mixed mixed_path (crossfade, T44)
 *                       before it are not printed (their count depends on
 *                       the song length)
 *   @position           SNAPSHOT position_ms=M order=O row=R pattern=P (song.pattern_pos)
 *   @user_stats         USERDATA cue_sets=N cue_gotos=N sticker_ops=N (T81, T82)
 *   @radio              RADIO radio_appends=N min_ahead=N then one `queue=<path>` line per queue row (T88)
 *   @playlist           pumps, then prints PLAYLIST rows=N and one `ROW <text>` line per row of the playlist view (VIEW_SOURCE playlist first), the text the UIs draw (I174a)
 *   @ratings_wait       the DEViLBOX push thread is idle (I173c; @ratings waits too)
 *   @ratings            RATINGS rating_pushes=N rating_batches=N rating_pending=N (T79)
 *   @votes              VOTES vote_posts=N status=ok|rate|bad|missing|down|none count=N yours=N m=<key>; waits for the vote request (I174, T80)
 *   @lyrics             LYRICS lyric_source=none|lrc|message|devilbox lyric_line=N lyric_lines=N lyric_fetches=N text=<current line or plain text>;
 *                       waits for the lyrics request and prints devilbox_missing=lyrics when the endpoint is missing (I193, T93)
 *   @ratings_read <source> <n> [<path>...]  batch-read n ratings from DEViLBOX (named paths first), store the own ones (T79)
 *   @share_stats        SHARE links_parsed=N links_made=N songs_opened=N (T84)
 *   @stream_stats       STREAM stream_reads=N stream_bytes=N (T94: OPEN of a URL or pipe path, I195)
 *   @stream_live        STREAMLIVE opens=N audio_bytes=N audio_done=0|1 received=N record_taps=N record_files=N (I195b: bytes downloaded when
 *                       the first audio frame played, and whether the download had finished by then)
 *   @title              TITLE text=<title tag of the song playing now, empty when none> (I195f: a chained Ogg stream changes it
 *                       mid-song)
 *   @folder_stats       FOLDERS folders=N playlists_in_folders=N playlists=N entries=N folder_ops=N (T83)
 *   @folder_tree        the folder tree, one \"F|P <id> <name>\" line per node (T83)
 *   @playlist_add <folder> <name> <key>...  adds a DB playlist (test plumbing; no engine verb creates one yet)
 *   @integration_poll   call INT_POLL of the --integration plugin once
 *   @alloc_guard on     write SEL_ALLOC_FREEZE/THAW to stderr around every
 *                       sel_engine_render call (pump-side work stays outside
 *                       the window), for the no-alloc shim (T26) */
#define SCRIPT_RENDER_CHUNK 4096u
#define SCRIPT_RENDER_LIMIT 100000u

typedef struct script_sink {
    int quiet_position; /* drop POSITION until SONG_ENDED */
    int ended;
} script_sink;

static void script_event(void* ctx, const sel_message* evt) {
    script_sink* s = ctx;
    if (s->quiet_position && !s->ended && evt->id == SEL_EVT_POSITION) {
        return;
    }
    if (evt->id == SEL_EVT_SONG_ENDED) {
        s->ended = 1;
    }
    print_event(NULL, evt);
}

static int g_alloc_guard;

/* --clock fake=HH:MM:SS: the host answers HOSTQ_LOCAL_TIME from a counter that
 * only the script directive @clock +ms advances (T45). */
static sel_host g_fake_host;
static uint32_t g_fake_ms;

static int fake_query(const sel_host* h, const sel_tag* ask, sel_tag* answer) {
    if (ask && ask->tag == SEL_HOSTQ_LOCAL_TIME) {
        answer->tag = SEL_HOSTQ_LOCAL_TIME;
        answer->value = g_fake_ms;
        return 1;
    }
    return sel_host_default()->query(h, ask, answer);
}

static FILE* g_dump; /* @dump: raw s16 stereo of every rendered block (I189a) */

static void script_render(sel_engine* e, uint32_t frames) {
    static int16_t buf[SCRIPT_RENDER_CHUNK * 2u];
    while (frames > 0) {
        uint32_t n = frames < SCRIPT_RENDER_CHUNK ? frames : SCRIPT_RENDER_CHUNK;
        if (g_alloc_guard) {
            fputs("SEL_ALLOC_FREEZE\n", stderr);
        }
        sel_engine_render(e, buf, n);
        if (g_alloc_guard) {
            fputs("SEL_ALLOC_THAW\n", stderr);
        }
        if (g_dump != NULL) {
            fwrite(buf, sizeof(int16_t) * 2u, n, g_dump);
        }
        frames -= n;
    }
}

#ifndef __amigaos__
typedef struct stop_job {
    sel_engine* e;
    unsigned ms;
    int started;
    pthread_t th;
} stop_job;

static void* stop_after_main(void* arg) {
    stop_job* j = (stop_job*)arg;
    sel_message m;
    usleep((useconds_t)j->ms * 1000u);
    memset(&m, 0, sizeof m);
    m.kind = SEL_MSG_CMD;
    m.id = SEL_CMD_STOP;
    sel_engine_send(j->e, &m);
    return NULL;
}
#endif

static void script_directive(sel_engine* e, const char* line) {
    unsigned long n = 0;
    script_sink sink = {0, 0};
    if (strcmp(line, "@queue") == 0) {
        uint32_t i;
        const sel_state* st = sel_engine_state(e);
        printf("QUEUE version=%u playing=%u ids=", (unsigned)st->queue_version, (unsigned)st->queue_playing);
        for (i = 0; i < st->queue_count; i++) {
            printf(i ? ",%u" : "%u", (unsigned)st->queue[i].id);
        }
        printf("\n");
    } else if (strcmp(line, "@playlist") == 0) {
        uint32_t i;
        const sel_view_state* vs;

        sel_engine_pump(e, NULL, NULL); /* stores a finished vote count, then the view reads it */
        vs = sel_engine_state(e)->view;
        printf("PLAYLIST rows=%u\n", (unsigned)(vs != NULL ? vs->row_n : 0u));
        for (i = 0; vs != NULL && i < vs->row_n; i++) {
            printf("ROW %s\n", vs->rows[i].text);
        }
    } else if (strcmp(line, "@results") == 0) {
        uint32_t i;
        sel_engine_stats es;
        sel_engine_stats_get(e, &es);
        printf("RESULTS %u ", (unsigned)es.result_count);
        for (i = 0; i < es.result_count; i++) {
            printf(i ? ",%s" : "%s", sel_engine_result_key(e, i));
        }
        printf("\n");
    } else if (strcmp(line, "@library_stats") == 0) {
        sel_engine_stats es;
        sel_engine_stats_get(e, &es);
        printf("LIBRARY searches=%u fts_queries=%u songs_opened=%u history_writes=%u skip_excluded=%u\n",
               (unsigned)es.library_searches, (unsigned)es.library_fts_queries, (unsigned)es.songs_opened,
               (unsigned)es.history_writes, (unsigned)es.skip_excluded);
    } else if (strcmp(line, "@dup_stats") == 0) {
        sel_engine_stats es;
        sel_engine_stats_get(e, &es);
        printf("DUP redirects=%u played=%s\n", (unsigned)es.dup_redirects, es.dup_played);
    } else if (strcmp(line, "@user_stats") == 0) {
        sel_engine_stats es;
        sel_engine_stats_get(e, &es);
        printf("USERDATA cue_sets=%u cue_gotos=%u sticker_ops=%u\n", (unsigned)es.cue_sets, (unsigned)es.cue_gotos,
               (unsigned)es.sticker_ops);
    } else if (strcmp(line, "@radio") == 0) {
        sel_engine_stats es;
        uint32_t i;
        const sel_state* st = sel_engine_state(e);
        sel_engine_stats_get(e, &es);
        printf("RADIO radio_appends=%u min_ahead=%u\n", (unsigned)es.radio_appends, (unsigned)es.radio_min_ahead);
        for (i = 0; i < st->queue_count; i++) {
            printf("queue=%s\n", st->queue_text + st->queue[i].path_offset);
        }
    } else if (strcmp(line, "@ratings") == 0) {
        sel_engine_stats es;
        sel_engine_stats_get(e, &es);
        printf("RATINGS rating_pushes=%u rating_batches=%u rating_pending=%u\n", (unsigned)es.rating_pushes,
               (unsigned)es.rating_batches, (unsigned)es.rating_pending);
    } else if (strcmp(line, "@share_stats") == 0) {
        sel_engine_stats es;
        sel_engine_stats_get(e, &es);
        printf("SHARE links_parsed=%u links_made=%u songs_opened=%u\n", (unsigned)es.links_parsed,
               (unsigned)es.links_made, (unsigned)es.songs_opened);
    } else if (strcmp(line, "@stream_stats") == 0) {
        printf("STREAM stream_reads=%u stream_bytes=%u\n", (unsigned)sel_stream_reads(), (unsigned)sel_stream_bytes());
    } else if (strcmp(line, "@stream_live") == 0) {
        printf("STREAMLIVE opens=%u audio_bytes=%u audio_done=%u received=%u record_taps=%u record_files=%u\n",
               (unsigned)sel_stream_live_opens(), (unsigned)sel_stream_audio_bytes(), (unsigned)sel_stream_audio_done(),
               (unsigned)sel_stream_received(), (unsigned)sel_stream_record_taps(), (unsigned)sel_stream_record_files());
    } else if (strcmp(line, "@cancel_stats") == 0) {
        sel_engine_stats es;
        sel_engine_stats_get(e, &es);
        printf("CANCEL net_cancels=%u\n", (unsigned)es.net_cancels);
    } else if (strcmp(line, "@title") == 0) {
        const sel_song_state* so = &sel_engine_state(e)->song;
        printf("TITLE text=%s\n", (const char*)sel_tag_find(so->meta, SEL_META_TITLE, (uintptr_t) ""));
    } else if (strcmp(line, "@position") == 0) {
        const sel_song_state* so = &sel_engine_state(e)->song;
        printf("SNAPSHOT position_ms=%u order=%u row=%u pattern=%u\n", (unsigned)so->position_ms,
               (unsigned)so->pattern_pos.order, (unsigned)so->pattern_pos.row, (unsigned)so->pattern_pos.pattern);
    } else if (strncmp(line, "@cells ", 7) == 0) {
        unsigned order, row, channels, rows, c, r, got;
        sel_rp_tracker_info ti;
        sel_rp_cell cell;
        if (sscanf(line, "@cells %u %u %u %u", &order, &row, &channels, &rows) != 4) {
            fprintf(stderr, "selecta-cli: bad directive %s\n", line);
        } else if (!sel_engine_tracker_info(e, order, &ti)) {
            printf("No pattern data order=%u\n", order);
        } else {
            printf("TRACKER_INFO order=%u orders=%u patterns=%u rows=%u channels=%u\n", order, (unsigned)ti.orders,
                   (unsigned)ti.patterns, (unsigned)ti.rows, (unsigned)ti.channels);
            for (r = 0; r < rows; r++) {
                for (c = 0; c < channels; c++) {
                    got = sel_engine_tracker_cells(e, order, c, row + r, row + r + 1u, &cell, 1u);
                    if (got == 1u) {
                        printf("CELL order=%u row=%u channel=%u note=%u instrument=%u volume=%u effect=%u param=%u\n",
                               order, row + r, c, (unsigned)cell.note, (unsigned)cell.instrument,
                               (unsigned)cell.volume, (unsigned)cell.effect, (unsigned)cell.param);
                    }
                }
            }
        }
    } else if (strcmp(line, "@stats") == 0) {
        sel_engine_stats st;
        sel_engine_stats_get(e, &st);
        printf("STATS xfade_frames=%u slots_mixed=%u mixed_path=%u\n", (unsigned)st.xfade_frames,
               (unsigned)st.slots_mixed, (unsigned)st.mixed_path);
    } else if (sscanf(line, "@clock +%lu", &n) == 1) {
        g_fake_ms = (uint32_t)(((unsigned long)g_fake_ms + n) % 86400000ul);
    } else if (strncmp(line, "@dump ", 6) == 0) {
        if (g_dump != NULL) {
            fclose(g_dump);
            g_dump = NULL;
        }
        if (strcmp(line + 6, "off") != 0) {
            g_dump = fopen(line + 6, "wb");
        }
    } else if (strcmp(line, "@integration_poll") == 0) {
        sel_int_session_poll(g_int);
    } else if (strcmp(line, "@alloc_guard on") == 0) {
        g_alloc_guard = 1;
    } else if (sscanf(line, "@render %lu", &n) == 1) {
        script_render(e, (uint32_t)n);
        sel_engine_pump(e, script_event, &sink);
    } else if (strcmp(line, "@fade_mid") == 0) {
        uint32_t i, pos = 0, total = 0;
        for (i = 0; i < SCRIPT_RENDER_LIMIT && !sink.ended && !sel_engine_fade(e, &pos, &total); i++) {
            script_render(e, 1024u);
            sel_engine_pump(e, script_event, &sink);
        }
        if (sel_engine_fade(e, &pos, &total) && pos < total / 2u) {
            script_render(e, total / 2u - pos);
            sel_engine_pump(e, script_event, &sink);
            sel_engine_fade(e, &pos, &total);
            printf("FADE gain_q16=%u pos=%u total=%u\n",
                   (unsigned)(((uint64_t)(total - pos) << 16) / total), (unsigned)pos, (unsigned)total);
        } else {
            printf("ERROR script_timeout @fade_mid\n");
        }
    } else if (strcmp(line, "@render_until_end") == 0) {
        uint32_t i;
        sink.quiet_position = 1;
        for (i = 0; i < SCRIPT_RENDER_LIMIT && !sink.ended; i++) {
            script_render(e, SCRIPT_RENDER_CHUNK);
            sel_engine_pump(e, script_event, &sink);
        }
        if (!sink.ended) {
            printf("ERROR script_timeout @render_until_end\n");
        }
    } else {
        fprintf(stderr, "selecta-cli: unknown directive %s\n", line);
    }
}

static int has_catalogue(int argc, char** argv) {
    int i;
    for (i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--catalogue") == 0) return 1;
    }
    return 0;
}

/* T3: every line of the file goes through codec, queue, dispatch and back
 * out as events. One pump per line so the decode pool can be reused. */
static int cmd_script_file(FILE* f, int argc, char** argv) {
    const char* int_id = NULL;
    const sel_host* host = sel_host_default();
    int i;
    char line[1024];
    char pool[2048];
    sel_engine_config cfg;
    sel_engine* e;
    sel_library* lib = NULL;
    memset(&cfg, 0, sizeof cfg);
    for (i = 0; i + 1 < argc; i++) {
        unsigned hh, mm, ss;
        if (strcmp(argv[i], "--clock") == 0) {
            if (sscanf(argv[i + 1], "fake=%2u:%2u:%2u", &hh, &mm, &ss) != 3 || hh > 23u || mm > 59u || ss > 59u) {
                fprintf(stderr, "selecta-cli: --clock wants fake=HH:MM:SS\n");
                fclose(f);
                return 1;
            }
            g_fake_ms = (hh * 3600u + mm * 60u + ss) * 1000u;
            g_fake_host = *sel_host_default();
            g_fake_host.query = fake_query;
            host = &g_fake_host;
        } else if (strcmp(argv[i], "--config-dir") == 0) {
            cfg.config_dir = argv[i + 1]; /* prefs.conf and devilbox.token live here (I173) */
        } else if (strcmp(argv[i], "--db") == 0 && lib == NULL && !has_catalogue(argc, argv)) {
            /* A library DB without a catalogue import (I184): OPEN resolves the best source through it. */
            lib = sel_library_open(host, sel_machine_probe(host), 0, argv[i + 1]);
            if (lib == NULL) {
                fclose(f);
                return 1;
            }
            cfg.library = lib;
        } else if (strcmp(argv[i], "--catalogue") == 0) {
            const char* db = NULL;
            int j;
            for (j = 0; j + 1 < argc; j++) {
                if (strcmp(argv[j], "--db") == 0) db = argv[j + 1];
            }
            lib = db != NULL ? sel_cli_open_catalogue_at(argv[i + 1], db) : sel_cli_open_catalogue(argv[i + 1]);
            if (lib == NULL) {
                fclose(f);
                return 1;
            }
            cfg.library = lib;
        }
    }
    sel_registry_init(sel_host_default());
    cfg.registry_count = sel_registry_count;
    cfg.registry_get = sel_registry_get;
    cfg.machine = sel_machine_probe(sel_host_default());
    e = sel_engine_create(host, &cfg);
    if (e == NULL) {
        fclose(f);
        sel_library_close(lib);
        fprintf(stderr, "selecta-cli: engine allocation failed\n");
        return 1;
    }
    for (i = 0; i + 1 < argc; i++) {
        if (strcmp(argv[i], "--integration") == 0) {
            int_id = argv[i + 1];
        }
    }
    if (int_id != NULL) {
        g_int = sel_int_session_open(e, sel_host_default(), int_id, print_event, NULL);
        if (g_int == NULL) {
            fprintf(stderr, "selecta-cli: integration '%s' is not registered\n", int_id);
            fclose(f);
            sel_engine_destroy(e);
            sel_library_close(lib);
            return 1;
        }
    }
    while (fgets(line, sizeof line, f)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] == '\0' || line[0] == '#') {
            continue;
        }
        if (strcmp(line, "@ratings") == 0 || strcmp(line, "@ratings_wait") == 0) {
            sel_library_rating_wait(lib); /* test hook: the DEViLBOX push thread (I173c) is idle after this */
            if (line[8] != '\0') {
                continue;
            }
        }
        if (strcmp(line, "@votes") == 0) { /* test hook: wait for the vote request (I174), print its outcome */
            sel_engine_stats es;
            sel_vote_result vr;

            sel_library_rating_wait(lib);
            sel_engine_stats_get(e, &es);
            sel_library_vote_result(lib, &vr);
            if (vr.done && strcmp(sel_vote_status_name(vr.status), "missing") == 0) printf("devilbox_missing=hip_vote\n");
            printf("VOTES vote_posts=%u status=%s count=%u yours=%u m=%s\n", (unsigned)es.vote_posts,
                   vr.done ? sel_vote_status_name(vr.status) : "none", vr.count, vr.yours, vr.mkey);
            continue;
        }
        if (strcmp(line, "@lyrics") == 0) { /* test hook: wait for the lyrics request (I193), print the lyrics state */
            static const char* const names[] = {"none", "lrc", "message", "devilbox"};
            sel_engine_stats es;
            sel_lyrics_result lr;
            const sel_state* st;

            sel_library_rating_wait(lib);
            sel_engine_pump(e, NULL, NULL); /* takes a finished answer into the snapshot */
            sel_engine_stats_get(e, &es);
            sel_library_lyrics_result(lib, &lr);
            st = sel_engine_state(e);
            if (lr.seq > 0 && lr.status < 0) printf("devilbox_missing=lyrics\n");
            printf("LYRICS lyric_source=%s lyric_line=%d lyric_lines=%u lyric_fetches=%u text=",
                   names[st->lyric_source < 4u ? st->lyric_source : 0u], (int)st->lyric_line, (unsigned)st->lyric_lines,
                   (unsigned)es.lyric_fetches);
            if (st->lyric_lines == 0u && st->lyric_text != NULL) { /* plain lyrics: the whole text, line breaks as '|' */
                const char* t = st->lyric_text;
                for (; *t != '\0'; t++) putchar(*t == '\r' || *t == '\n' ? '|' : *t);
                putchar('\n');
            } else {
                printf("%s\n", st->lyric_current != NULL ? st->lyric_current : "");
            }
            continue;
        }
        if (strcmp(line, "@prefetch_wait") == 0) {
            sel_library_prefetch_wait(lib); /* test hook: the prefetch thread is idle after this */
            continue;
        }
#ifndef __amigaos__
        if (strncmp(line, "@sleep ", 7) == 0) { /* test hook: let a background transfer get going */
            usleep((useconds_t)strtoul(line + 7, NULL, 10) * 1000u);
            continue;
        }
        if (strncmp(line, "@stop_after ", 12) == 0) { /* test hook: STOP sent from a second thread after N ms (I195a) */
            static stop_job job; /* the thread ends after its one send; the next @stop_after joins it */
            if (job.started) pthread_join(job.th, NULL);
            job.e = e;
            job.ms = (unsigned)strtoul(line + 12, NULL, 10);
            job.started = pthread_create(&job.th, NULL, stop_after_main, &job) == 0;
            continue;
        }
        if (strncmp(line, "@timed ", 7) == 0) { /* test hook: one protocol line, wall time printed */
            struct timespec t0, t1;
            clock_gettime(CLOCK_MONOTONIC, &t0);
            sel_engine_send_line(e, line + 7, pool, sizeof pool);
            sel_engine_pump(e, print_event, NULL);
            clock_gettime(CLOCK_MONOTONIC, &t1);
            printf("timed_ms=%ld\n", (long)((t1.tv_sec - t0.tv_sec) * 1000 + (t1.tv_nsec - t0.tv_nsec) / 1000000));
            continue;
        }
#endif
        if (strncmp(line, "@ratings_read ", 14) == 0) {
            sel_cli_ratings_read(lib, line + 14);
            continue;
        }
        if (strncmp(line, "@mark ", 6) == 0 && lib != NULL) {
            char table[8]; /* kept or liked */
            const char* colon = strchr(line + 6, ':');
            if (colon != NULL && (size_t)(colon - line - 6) < sizeof table) {
                memcpy(table, line + 6, (size_t)(colon - line - 6));
                table[colon - line - 6] = '\0';
                sel_library_mark(lib, table, colon + 1);
            }
            continue;
        }
        if (strcmp(line, "@folder_stats") == 0 && lib != NULL) {
            sel_engine_stats es;
            uint32_t fo = 0, inf = 0, pls = 0, ent = 0;
            sel_engine_stats_get(e, &es);
            sel_library_folder_counts(lib, &fo, &inf, &pls, &ent);
            printf("FOLDERS folders=%u playlists_in_folders=%u playlists=%u entries=%u folder_ops=%u\n", (unsigned)fo,
                   (unsigned)inf, (unsigned)pls, (unsigned)ent, (unsigned)es.folder_ops);
            continue;
        }
        if (strcmp(line, "@folder_tree") == 0 && lib != NULL) {
            static char tree[8192];
            uint32_t n = 0;
            if (sel_library_folder_tree(lib, tree, sizeof tree, &n) == SEL_FOLDER_OK) fputs(tree, stdout);
            printf("TREE lines=%u\n", (unsigned)n);
            continue;
        }
        if (strncmp(line, "@playlist_add ", 14) == 0 && lib != NULL) {
            char keys[2048];
            char name[128];
            char* tok;
            char* rest;
            uint32_t folder, id = 0;
            int rc;
            folder = (uint32_t)strtoul(line + 14, &rest, 10);
            keys[0] = '\0';
            name[0] = '\0';
            tok = strtok(rest, " ");
            if (tok != NULL) snprintf(name, sizeof name, "%s", tok);
            while ((tok = strtok(NULL, " ")) != NULL) {
                strncat(keys, tok, sizeof keys - strlen(keys) - 2u);
                strncat(keys, "\n", sizeof keys - strlen(keys) - 1u);
            }
            rc = sel_library_playlist_add(lib, name, folder, keys, &id);
            if (rc == SEL_FOLDER_OK) {
                printf("PLAYLIST id=%u name=%s\n", (unsigned)id, name);
            } else {
                printf("ERROR playlist_add code=%d\n", rc);
            }
            continue;
        }
        if (strncmp(line, "@budget ", 8) == 0 && lib != NULL) {
            sel_library_set_budget(lib, (uint32_t)strtoul(line + 8, NULL, 10));
            continue;
        }
        if (line[0] == '@') {
            script_directive(e, line);
            continue;
        }
        sel_engine_send_line(e, line, pool, sizeof pool);
        sel_engine_pump(e, print_event, NULL);
    }
    fclose(f);
    if (lib != NULL) {
        sel_library_prefetch_wait(lib); /* the job thread is idle, so the counters below are final (I170a) */
    }
    sel_library_cache_report(lib);
    sel_int_session_close(g_int);
    g_int = NULL;
    sel_engine_destroy(e);
    sel_library_close(lib);
    return 0;
}

static int cmd_script(const char* path, int argc, char** argv) {
    FILE* f = fopen(path, "r");
    if (f == NULL) {
        fprintf(stderr, "selecta-cli: cannot open %s\n", path);
        return 1;
    }
    return cmd_script_file(f, argc, argv);
}

/* T84 (I180): share links. `share make` and `share parse` drive the codec alone; `share song|position|playlist`
 * and `open` run the engine verbs SHARE and OPEN through the script path, so the events and counters are the
 * engine's own. */
static const char* const k_share_kind[] = {"none", "song", "playlist", "view"};

static void print_link(const sel_share_link* l) {
    char text[SEL_SHARE_QUERY_MAX + SEL_SHARE_PATH_MAX], dev[SEL_SHARE_QUERY_MAX + SEL_SHARE_PATH_MAX];
    sel_share_link back;
    int ok = sel_share_format(l, text, sizeof text) && sel_share_format_devilbox(l, dev, sizeof dev);
    if (!ok) {
        puts("ERROR link_too_long");
        return;
    }
    printf("LINK %s\nLINK_DEVILBOX %s\n", text, dev);
    printf("ROUNDTRIP selecta=%s devilbox=%s\n",
           sel_share_parse(text, &back) && sel_share_equal(l, &back) ? "equal" : "differs",
           sel_share_parse(dev, &back) && sel_share_equal(l, &back) ? "equal" : "differs");
}

static int share_make(int argc, char** argv) {
    sel_share_link l;
    int i, pos = 0;
    memset(&l, 0, sizeof l);
    if (argc < 2) return usage(stderr);
    l.kind = strcmp(argv[0], "song") == 0       ? SEL_SHARE_SONG
             : strcmp(argv[0], "playlist") == 0 ? SEL_SHARE_PLAYLIST
             : strcmp(argv[0], "view") == 0     ? SEL_SHARE_VIEW
                                                : SEL_SHARE_NONE;
    if (l.kind == SEL_SHARE_NONE) return usage(stderr);
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--sub") == 0 && i + 1 < argc) {
            l.has_sub = 1;
            l.sub = (uint32_t)strtoul(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "--t") == 0 && i + 1 < argc) {
            l.has_t = 1;
            l.t_ms = (uint32_t)strtoul(argv[++i], NULL, 10);
        } else if (l.kind == SEL_SHARE_VIEW && pos == 0) {
            snprintf(l.path, sizeof l.path, "%s", argv[i]);
            pos++;
        } else if (pos == 0) {
            snprintf(l.source, sizeof l.source, "%s", argv[i]);
            pos++;
        } else if (pos == 1) {
            snprintf(l.path, sizeof l.path, "%s", argv[i]);
            pos++;
        }
    }
    print_link(&l);
    return 0;
}

static int share_parse(const char* text) {
    sel_share_link l;
    char key[SEL_SHARE_PATH_MAX + SEL_SHARE_SOURCE_MAX];
    if (!sel_share_parse(text, &l)) {
        puts("ERROR bad_link");
        return 2;
    }
    printf("PARSED kind=%s source=%s path=%s sub=%d t=%d", k_share_kind[l.kind], l.source, l.path,
           l.has_sub ? (int)l.sub : -1, l.has_t ? (int)l.t_ms : -1);
    if (l.kind == SEL_SHARE_SONG && sel_share_key(l.source, l.path, key, sizeof key)) printf(" key=%s", key);
    putchar('\n');
    return 0;
}

/* Builds the script in a temp file and hands it to the script runner (which closes it). */
static int run_generated(const char* const* lines, int n, int argc, char** argv) {
    FILE* f = tmpfile();
    int i;
    if (f == NULL) {
        fprintf(stderr, "selecta-cli: cannot create a script\n");
        return 1;
    }
    for (i = 0; i < n; i++) fprintf(f, "%s\n", lines[i]);
    rewind(f);
    return cmd_script_file(f, argc, argv);
}

static int cmd_share(int argc, char** argv) {
    char open_line[1100], seek_line[48], share_line[1100];
    const char* lines[6];
    int n = 0, i;
    const char* at = NULL;
    if (argc < 2) return usage(stderr);
    if (strcmp(argv[0], "make") == 0) return share_make(argc - 1, argv + 1);
    if (strcmp(argv[0], "parse") == 0) return share_parse(argv[1]);
    if (strcmp(argv[0], "playlist") == 0) {
        snprintf(share_line, sizeof share_line, "SHARE playlist %s", argv[1]);
        lines[n++] = share_line;
    } else if (strcmp(argv[0], "song") == 0 || strcmp(argv[0], "position") == 0) {
        for (i = 2; i + 1 < argc; i++) {
            if (strcmp(argv[i], "--at") == 0) at = argv[i + 1];
        }
        snprintf(open_line, sizeof open_line, "OPEN %s", argv[1]);
        lines[n++] = open_line;
        lines[n++] = "@render 441";
        if (at != NULL) {
            snprintf(seek_line, sizeof seek_line, "SEEK %s", at);
            lines[n++] = seek_line;
        }
        snprintf(share_line, sizeof share_line, "SHARE %s", argv[0]);
        lines[n++] = share_line;
    } else {
        return usage(stderr);
    }
    lines[n++] = "@share_stats";
    return run_generated(lines, n, argc - 2, argv + 2);
}

static int cmd_open(int argc, char** argv) {
    char open_line[1100];
    const char* lines[3];
    if (argc < 1) return usage(stderr);
    snprintf(open_line, sizeof open_line, "OPEN %s", argv[0]);
    lines[0] = open_line;
    lines[1] = "@render 441";
    lines[2] = "@share_stats";
    return run_generated(lines, 3, argc - 1, argv + 1);
}

/* T35: `send <verb> [args]` writes one protocol line to the running shell's
 * control socket and prints the reply. Exit 0 on OK, 1 on ERROR, 2 when no
 * shell is running. */
static int cmd_send(int argc, char** argv) {
#ifdef SEL_NO_CONTROL_SOCKET
    (void)argc;
    (void)argv;
    fputs("selecta-cli: send needs the control socket; the Amiga uses the ARexx port\n", stderr);
    return 2;
#else
    char path[256], line[1024], reply[1024];
    int i;
    size_t used = 0;
    if (argc < 1) {
        return usage(stderr);
    }
    line[0] = '\0';
    for (i = 0; i < argc; i++) {
        int n = snprintf(line + used, sizeof line - used, i ? " %s" : "%s", argv[i]);
        if (n < 0 || (size_t)n >= sizeof line - used) {
            fputs("selecta-cli: command line too long\n", stderr);
            return 2;
        }
        used += (size_t)n;
    }
    if (!sel_ctl_path(path, sizeof path)) {
        fputs("selecta-cli: socket path too long\n", stderr);
        return 2;
    }
    if (sel_ctl_send(path, line, reply, sizeof reply) != 0) {
        fprintf(stderr, "no selecta running at %s\n", path);
        return 2;
    }
    printf("%s\n", reply);
    return strncmp(reply, "OK", 2) == 0 ? 0 : 1;
#endif
}

#ifdef __amigaos__
static int app_main(int argc, char** argv);

int main(int argc, char** argv) {
    return sel_amiga_run_with_stack(app_main, argc, argv, SEL_AMIGA_STACK_BYTES);
}

static int app_main(int argc, char** argv)
#else
int main(int argc, char** argv)
#endif
{
    const char* cmd;
    if (argc < 2) {
        return usage(stderr);
    }
    cmd = argv[1];
    if (strcmp(cmd, "--dry-run") == 0) {
        return sel_cli_dry_run(argc - 2, argv + 2);
    }
    if (strcmp(cmd, "version") == 0) {
        return cmd_version();
    }
    if (strcmp(cmd, "help") == 0 || strcmp(cmd, "--help") == 0) {
        return usage(stdout);
    }
    if (strcmp(cmd, "commands") == 0) {
        if (argc > 2 && strcmp(argv[2], "--unimplemented") == 0) {
            return cmd_commands(1);
        }
        return cmd_commands(0);
    }
    if (strcmp(cmd, "loudness") == 0) {
        return sel_cli_loudness(argc - 2, argv + 2);
    }
    if (strcmp(cmd, "analyse") == 0) {
        return sel_cli_analyse(argc - 2, argv + 2);
    }
    if (strcmp(cmd, "script") == 0) {
        if (argc < 3) {
            return usage(stderr);
        }
        return cmd_script(argv[2], argc - 3, argv + 3);
    }
    if (strcmp(cmd, "send") == 0) {
        return cmd_send(argc - 2, argv + 2);
    }
    if (strcmp(cmd, "events") == 0) {
        return cmd_events();
    }
    if (strcmp(cmd, "tagtypes") == 0) {
        return cmd_tagtypes();
    }
    if (strcmp(cmd, "config-dir") == 0) {
        return cmd_config_dir();
    }
    if (strcmp(cmd, "roundtrip") == 0) {
        return cmd_roundtrip();
    }
    if (strcmp(cmd, "abi") == 0) {
        return sel_cli_abi(argc > 2 ? argv[2] : NULL);
    }
    if (strcmp(cmd, "plugins") == 0) {
        return sel_cli_plugins(argc - 2, argv + 2);
    }
    if (strcmp(cmd, "machine") == 0) {
        return sel_cli_machine(argc - 2, argv + 2);
    }
    if (strcmp(cmd, "play-null") == 0) {
        return sel_cli_play_null(argc - 2, argv + 2);
    }
    if (strcmp(cmd, "play") == 0) {
        return sel_cli_play(argc - 2, argv + 2);
    }
    if (strcmp(cmd, "spsc-selftest") == 0) {
        return sel_cli_spsc_selftest();
    }
    if (strcmp(cmd, "library") == 0) {
        return sel_cli_library(argc - 2, argv + 2);
    }
    if (strcmp(cmd, "wrong") == 0) {
        return sel_cli_wrong(argc - 2, argv + 2);
    }
    if (strcmp(cmd, "userdata") == 0) {
        return sel_cli_userdata(argc - 2, argv + 2);
    }
    if (strcmp(cmd, "share") == 0) {
        return cmd_share(argc - 2, argv + 2);
    }
    if (strcmp(cmd, "open") == 0) {
        return cmd_open(argc - 2, argv + 2);
    }
    if (strcmp(cmd, "devilbox") == 0) {
        return sel_cli_devilbox(argc - 2, argv + 2);
    }
    if (strcmp(cmd, "fetch") == 0) {
        return sel_cli_fetch(argc - 2, argv + 2);
    }
    if (strcmp(cmd, "record") == 0) {
        return sel_cli_record(argc - 2, argv + 2);
    }
    if (strcmp(cmd, "render") == 0) {
        return sel_cli_render(argc - 2, argv + 2);
    }
    fprintf(stderr, "selecta-cli: unknown command %s\n", cmd);
    return usage(stderr);
}
