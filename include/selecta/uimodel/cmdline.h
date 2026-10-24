/* Command line of the text UI (plan M1.2 phase 5, row I77; design doc section
 * 7). `:` opens it. The line is an engine command (`QUEUE_SHUFFLE`, `SEEK
 * +5000`) or `!program args`, which runs a program with the selected paths
 * appended. Tab completes the verb from the COMMANDS table, Up and Down walk
 * the history. Pure C99, no allocation. */
#ifndef SELECTA_TEXT_CMDLINE_H
#define SELECTA_TEXT_CMDLINE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TXT_CMD_LINE 192u
#define TXT_CMD_HISTORY 100u
#define TXT_CMD_STATUS 192u

typedef enum txt_cmd_result {
    TXT_CMD_EDITING = 0, /* the line stays open */
    TXT_CMD_SUBMIT,      /* Enter: txt_cmdline_text holds the line, already pushed to the history */
    TXT_CMD_CANCEL       /* Escape, or Backspace on an empty line */
} txt_cmd_result;

typedef struct txt_cmdline {
    char text[TXT_CMD_LINE];
    uint32_t len;
    char history[TXT_CMD_HISTORY][TXT_CMD_LINE];
    uint32_t history_count; /* oldest first, at most TXT_CMD_HISTORY */
    uint32_t history_at;    /* history_count when not browsing */
    char draft[TXT_CMD_LINE]; /* the line typed before Up was pressed */
    char status[TXT_CMD_STATUS]; /* completion list or an error, shown by the UI */
    char prefix[TXT_CMD_LINE];   /* the text the current Tab series started from */
    uint32_t tab_at;             /* matches already cycled through; 0 = no series */
} txt_cmdline;

/* Empties the line and the status; keeps the history. */
void txt_cmdline_open(txt_cmdline* c);

/* Feeds one key (keys.h codes). */
txt_cmd_result txt_cmdline_key(txt_cmdline* c, uint32_t code, uint32_t mods);

/* The line as typed (the part after `:`). */
const char* txt_cmdline_text(const txt_cmdline* c);

/* History as text, one line each, oldest first (persistence is the caller's). */
uint32_t txt_cmdline_history_load(txt_cmdline* c, const char* text);
size_t txt_cmdline_history_dump(const txt_cmdline* c, char* out, size_t cap);

#ifdef __cplusplus
}
#endif
#endif
