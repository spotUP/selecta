/* Terminal device for the text UI: raw mode, size, timed read, full write.
 * One implementation per OS family under plugins/ui/text/<os>/ (RULES.md rule 3);
 * posix/term.c serves macOS and Linux. Plan M1.1 phase 5, row I59. */
#ifndef SELECTA_TEXT_TERM_H
#define SELECTA_TEXT_TERM_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct txt_term {
    int in_fd, out_fd;
    int active;      /* raw mode and screen modes are on */
    /* opaque saved terminal state, inside the struct so no allocation is needed */
    unsigned long long saved_bytes[32];
} txt_term;

/* Sequences written on enter and leave (exposed for tests): alternate screen,
 * hidden cursor, SGR mouse. */
extern const char txt_term_enter_seq[];
extern const char txt_term_leave_seq[];
/* 1 when this OS family's console speaks the Amiga console dialect (row I218a): bytes are
 * Latin-1 both ways, Backspace sends 0x08 and Del sends 0x7F. 0 means xterm: UTF-8, and
 * Backspace sends 0x7F. Each backend defines it next to txt_term_enter_seq. */
extern const int txt_term_amiga_console;

/* Saves the terminal state, switches to raw mode and writes txt_term_enter_seq.
 * Returns 0, or -1 when in_fd is not a terminal. */
int txt_term_open(txt_term* t, int in_fd, int out_fd);
/* Writes txt_term_leave_seq and restores the saved state. Safe to call twice. */
void txt_term_close(txt_term* t);
/* Window size in cells; falls back to 80x24 when the terminal does not say. */
void txt_term_size(const txt_term* t, uint32_t* cols, uint32_t* rows);
/* Asks the terminal for its size again (row I218c). The answer arrives later in the key stream
 * (TXT_KEY_WINDOW) and goes to txt_term_set_size. A no-op where txt_term_size reads the size itself. */
void txt_term_ask_size(txt_term* t);
/* Records a size learned from the key stream, so the next txt_term_size returns it. A no-op where
 * txt_term_size reads the size itself. */
void txt_term_set_size(txt_term* t, uint32_t cols, uint32_t rows);
/* Waits up to timeout_ms (-1 = forever) for input; returns bytes read, 0 on
 * timeout, -1 on error or end of input. */
long txt_term_read(txt_term* t, uint8_t* buf, size_t cap, int timeout_ms);
/* Writes all n bytes (retrying short writes); returns 0 or -1. */
int txt_term_write(txt_term* t, const char* buf, size_t n);

#ifdef __cplusplus
}
#endif
#endif
