/* One protocol line in, one reply out, shared by the control socket, the ARexx port and selecta-cli (plan
 * 2026-10-25-m4-arexx section 3). */
#ifndef SELECTA_CTLEXEC_H
#define SELECTA_CTLEXEC_H

#include <selecta/engine.h>
#include <stddef.h>

/* Reply buffer size for the socket and the ARexx port: COMMANDS is about 7 KB, so the cap is more than twice that. */
#define SEL_CTL_REPLY_MAX 16384u

/* Sends `line`, pumps the engine and builds the reply from the events of that line:
 * - an ERROR event: rc = 10, reply `<code> <detail>`, result lines dropped;
 * - otherwise rc = 0 and the RESULT lines joined with '\n' (empty when the verb has none);
 * - a reply that does not fit `cap`: rc = 10 and reply `result_truncated`.
 * Every event is also handed to `fwd` (may be NULL), so the shell's UIs still see it. */
int sel_ctl_exec_fwd(sel_engine* e, const char* line, char* reply, size_t cap, int* rc, sel_event_sink fwd,
                     void* fwd_ctx);

/* The same without a forwarding sink. Returns 1 when rc == 0. */
int sel_ctl_exec(sel_engine* e, const char* line, char* reply, size_t cap, int* rc);

#endif /* SELECTA_CTLEXEC_H */
