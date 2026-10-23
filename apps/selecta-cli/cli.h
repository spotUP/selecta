/* Subcommands of selecta-cli that live in their own files. */
#ifndef SELECTA_CLI_H
#define SELECTA_CLI_H

/* T6: prints `sel_host size=<n>` then `<field> offset=<n>` per field. */
int sel_cli_abi(const char* what);

/* T4: `play-null --rate <hz> --frames <n>`; prints
 * `pulled=<n> render_calls=<k> rate=<r>`. Writes SEL_ALLOC_FREEZE to
 * stderr once setup is complete (no_alloc.sh counts allocations after it). */
int sel_cli_play_null(int argc, char** argv);

/* T13/T17: `render <song> --output <wav> --seconds <n> --rate <hz> --resample <name>`;
 * prints `ticks=<n> voice_ops=<v> mixed_frames=<f>`. Writes SEL_ALLOC_FREEZE and
 * SEL_ALLOC_THAW to stderr around the render loop. */
int sel_cli_render(int argc, char** argv);

/* T5: `plugins [--lint]`; prints `plugins=<n> unknown_tags=<u> duplicate_tags=<d>`. */
int sel_cli_plugins(int argc, char** argv);

/* T10/T11: `machine`; prints `probe_calls=<n>`, then `NAME=value` per SEL_MACHINE_* tag. */
int sel_cli_machine(int argc, char** argv);

/* play <song> --output <id> --seconds <n> [--rate <hz>] [--resample <name>] (T16): plays
 * through a real output; prints `callbacks=<k> frames=<f>` from the output's
 * stats. Exits 77 when the output reports no device. */
int sel_cli_play(int argc, char** argv);

/* library browse <dir> (T57): LIBRARY_BROWSE through the engine; prints `dir_entries=<n>`. */
int sel_cli_library(int argc, char** argv);

/* library fetch <source>:<path> (T69): the Mirrors transport; prints mirror_tries= mirror_retries=. */
int sel_cli_library_fetch(int argc, char** argv);

/* LIBRARY_FETCH hook of `script` (T75, I165, I166): file cache + companions. The counters print at
 * the end of the script when the hook served at least one request. */
#include <selecta/engine.h>
typedef struct sel_cli_cache sel_cli_cache;
sel_cli_cache* sel_cli_cache_open(void);
sel_library_fetch_fn sel_cli_cache_hook(void);
void sel_cli_cache_report(const sel_cli_cache* c);

/* fetch <url> (T67, T68): one HTTP request through the library client; prints counters. */
int sel_cli_fetch(int argc, char** argv);

/* --dry-run [--tooltypes <file>] [KEY=VALUE ...] (T58): parses the start options, prints opts=, prefs_hash=, task_pri=. */
int sel_cli_dry_run(int argc, char** argv);

/* spsc-selftest (I114): 1000 messages through a wrapping queue; prints spsc_pushed=, spsc_popped=, spsc_publishes=. */
int sel_cli_spsc_selftest(void);

/* Shared by render and play (session.c). */
#include <selecta/engine.h>
#include <selecta/output.h>

typedef struct sel_cli_output {
    sel_out_open_fn open;
    sel_out_close_fn close;
    sel_out_set_source_fn set_source;
    sel_out_pump_fn pump;
    sel_out_stats_fn stats; /* may be NULL */
    const sel_tag* hooks;   /* the output plugin's hook list (headphones, voices) */
} sel_cli_output;

uint32_t sel_cli_engine_pull(void* ctx, int16_t* stereo, uint32_t frames);
/* Looks up output plugin `id`; prints a message prefixed with `who` and returns 0 on failure. */
int sel_cli_find_output(const char* who, const char* id, sel_cli_output* o);
/* Creates an engine, sets the mix, opens `song` and starts playing; NULL (message printed) on failure. */
sel_engine* sel_cli_start_song(const char* who, const sel_host* host, const char* song, uint32_t rate,
                               uint32_t resampler);
/* Replayer options for the next sel_cli_start_song*: `global` goes to the pref rp.config, `song` to
 * sel_engine_set_song_config ("plugin.key=value,..."; NULL: none). */
void sel_cli_set_rp_config(const char* global, const char* song);
/* As sel_cli_start_song, with `sink` (may be NULL) as the voice backend of the replayer (plan M2 I118). */
sel_engine* sel_cli_start_song_sink(const char* who, const sel_host* host, const char* song, uint32_t rate,
                                    uint32_t resampler, const sel_voices* sink);
#endif
