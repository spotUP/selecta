/* Subcommands of selecta-cli that live in their own files. */
#ifndef SELECTA_CLI_H
#define SELECTA_CLI_H

/* T6: prints `sel_host size=<n>` then `<field> offset=<n>` per field. */
int sel_cli_abi(void);

/* T4: `play-null --rate <hz> --frames <n>`; prints
 * `pulled=<n> render_calls=<k> rate=<r>`. Writes SEL_ALLOC_FREEZE to
 * stderr once setup is complete (no_alloc.sh counts allocations after it). */
int sel_cli_play_null(int argc, char** argv);

/* T5: `plugins [--lint]`; prints `plugins=<n> unknown_tags=<u> duplicate_tags=<d>`. */
int sel_cli_plugins(int argc, char** argv);

/* T10/T11: `machine`; prints `probe_calls=<n>`, then `NAME=value` per SEL_MACHINE_* tag. */
int sel_cli_machine(void);

#endif
