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
/* I187: analyse <song> | --sine <hz>:<dbfs>; library analyse --db <p> ['source:local']. Print analyse_jobs= analyse_speed= and the gain. */
int sel_cli_analyse(int argc, char** argv);
int sel_cli_library_analyse(int argc, char** argv);

/* wrong [--db <p>] [--export <file>] [<song> [<note>]] (T78, I171): flags the song wrong with the note in the
 * library DB, optionally writes sounds-wrong.txt; prints `flags=<rows> wrong_exported=<lines>`. */
int sel_cli_wrong(int argc, char** argv);

/* userdata --db <p> [--catalogue <tsv>] [--played <song>]... [--note <song> <text>]... [--tag <song> <field> <value>]... [--wrong <song> <note>]
 * (T78, I172): imports the catalogue into the DB file, applies NOTE, tag edits and wrong flags (the same
 * sel_library_* calls as FLAG and `wrong`), prints
 * `history_rows=<n> flags=<n> notes=<n> pending_push=<n>`. */
int sel_cli_userdata(int argc, char** argv);

/* library fetch <source>:<path> (T69): the Mirrors transport; prints mirror_tries= mirror_retries=. */
int sel_cli_library_fetch(int argc, char** argv);
/* I178: library export-folder --db <path> --dir <dir> [--folder <id>] [--format m3u]. */
int sel_cli_library_export_folder(int argc, char** argv);

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
 * the library DB sticker rp.config of the song (an in-memory library; "plugin.key=value,..."; NULL: none). */
void sel_cli_set_rp_config(const char* global, const char* song);
/* As sel_cli_start_song, with `sink` (may be NULL) as the voice backend of the replayer (plan M2 I118). */
sel_engine* sel_cli_start_song_sink(const char* who, const sel_host* host, const char* song, uint32_t rate,
                                    uint32_t resampler, const sel_voices* sink);
#endif
