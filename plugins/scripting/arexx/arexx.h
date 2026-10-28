/* ARexx port (plan 2026-10-25-m4-arexx section 5). arexx_core.c is portable and host-tested: it maps one ARexx
 * message (ARG0, the RXFF_RESULT flag) to (Result1, result string or none) through the shared command executor
 * and counts replies. arexx_amiga.c is the thin exec/rexxsyslib shell around it, built only for __amigaos__. */
#ifndef SELECTA_AREXX_H
#define SELECTA_AREXX_H

#include <stddef.h>

#define AREXX_RC_OK 0
#define AREXX_RC_FAILED 10
#define AREXX_RC_CLOSING 20
#define AREXX_LINE_MAX 1024u /* the socket's request line limit; a longer ARG0 is refused, never cut */
#define AREXX_NAME_MAX 16u
#define AREXX_MSGS_PER_TURN 8u /* open question 7 of the plan sets the final number */

/* One protocol line in, reply out; same shape as sel_ctl_exec_fwd bound to an engine by the caller. */
typedef int (*arexx_exec_fn)(void* ctx, const char* line, char* reply, size_t cap, int* rc);

typedef struct arexx_counters {
    unsigned cmds;   /* replies with rc == 0 */
    unsigned errors; /* every other reply */
} arexx_counters;

typedef struct arexx_result {
    long result1;   /* rm_Result1 */
    int has_string; /* 1: create an argstring of `text` for rm_Result2; 0: rm_Result2 = 0 */
    const char* text; /* the caller's reply buffer on success; the error text on failure (never sent as RESULT) */
} arexx_result;

/* Maps one message. `arg0` may be NULL (a message that is not a command). `reply` is the caller's buffer. */
void arexx_dispatch(const char* arg0, int want_result, arexx_exec_fn exec, void* exec_ctx, char* reply, size_t cap,
                    arexx_result* out, arexx_counters* counters);

/* Close-time batch end (open question 8): when a BEGIN is still open the port sends END through `exec` before the
 * port goes away, so the engine does not hold events forever. Returns 1 when END was sent. */
int arexx_end_batch(int batch_open, arexx_exec_fn exec, void* exec_ctx, char* reply, size_t cap,
                    arexx_counters* counters);

/* The command line of a message: `arg0` when the action code (the top byte of rm_Action, RXCODEMASK) is RXCOMM, else
 * NULL. Whether the sender built the message with CreateRexxMsg is not part of the decision. */
const char* arexx_message_line(unsigned long action, const char* arg0);

/* The closing reply for a message left in the queue at exit: RC 20, `port_closing`. */
void arexx_closing_result(arexx_result* out, arexx_counters* counters);

/* Port name for attempt `n` (0 -> SELECTA, 1 -> SELECTA.2 ... 98 -> SELECTA.99); 0 when n is past the last. */
int arexx_port_name(unsigned n, char* out, size_t cap);

/* The --stats line: `arexx_port=SELECTA arexx_cmds=N arexx_errors=M`, or `arexx=off` when `port` is NULL. */
void arexx_stats_line(char* out, size_t cap, const char* port, const arexx_counters* c);

/* Song-change hook (M4-AX-4-2, Amiga only): OPTION hook_command names an ARexx script; each SONG_OPENED runs
 * `rx <script>` asynchronously through `launch`. A launch that fails (script missing, no RexxMast) gives one log
 * line in `logline` and never stops the shell. */
typedef int (*arexx_launch_fn)(void* ctx, const char* script, const char* cmdline); /* 1: started */
typedef struct arexx_hook_stats {
    unsigned fired;  /* launches that started */
    unsigned failed; /* launches that did not */
} arexx_hook_stats;

/* Builds `rx "<script>"` (quotes in the path are refused: 0). */
int arexx_hook_cmdline(const char* script, char* out, size_t cap);
/* One event: acts only when `song_opened`; a NULL or empty script does nothing. Returns 1 started, 0 failed
 * (logline filled), -1 nothing to do. */
int arexx_hook_event(int song_opened, const char* script, arexx_launch_fn launch, void* launch_ctx,
                     arexx_hook_stats* st, char* logline, size_t cap);

#ifdef __amigaos__
typedef struct arexx_port arexx_port;
/* Opens rexxsyslib.library and adds the first free port name. NULL leaves ARexx off. */
arexx_port* arexx_open(void);
const char* arexx_name(const arexx_port* p);
const arexx_counters* arexx_stats(const arexx_port* p);
/* Takes up to AREXX_MSGS_PER_TURN messages and replies to each; returns how many. */
unsigned arexx_poll(arexx_port* p, arexx_exec_fn exec, void* exec_ctx);
/* Ends an open batch through `exec` (open question 8), then RemPort, replies every queued message with RC 20 and
 * closes the library. Runs on every way out of the shell loop. */
void arexx_close(arexx_port* p, arexx_exec_fn exec, void* exec_ctx, int batch_open);
/* The launcher for arexx_hook_event: checks the script exists and RexxMast is running (port "REXX"), then
 * SystemTags(SYS_Asynch). */
int arexx_hook_launch(void* ctx, const char* script, const char* cmdline);
#endif

#endif /* SELECTA_AREXX_H */
