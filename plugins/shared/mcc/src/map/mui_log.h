/* UP- MUI class package: the one diagnostic log function (design 2026-11-06-mui-mcc-package.md section 4 decoupling table, D5).
 * Quiet by default; prints to stderr when the debug variable is set (Amiga: ENV:UP-MCC/Debug, host: UP_MCC_DEBUG). */
#ifndef MCC_MUI_LOG_H
#define MCC_MUI_LOG_H

#ifdef __cplusplus
extern "C" {
#endif

/* printf-style line to stderr, only when the debug variable is set. The caller writes the "up-mcc: " prefix itself in the format. */
void aa_log(const char* fmt, ...)
#if defined(__GNUC__)
    __attribute__((format(printf, 1, 2)))
#endif
    ;

/* I625a6: the host's trace sink (a "t_ms phase" line in its --log). Null by default, so the package traces nothing on its own. */
void aa_trace_set(void (*fn)(const char* phase));
void aa_trace(const char* phase);

#ifdef __cplusplus
}
#endif

#endif
