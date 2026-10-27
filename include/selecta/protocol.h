/* Commands, events and the text codec of the remote protocol (design doc
 * sections 3 and 8). Everything here is generated from commands.def and
 * events.def; nothing is retyped. */
#ifndef SELECTA_PROTOCOL_H
#define SELECTA_PROTOCOL_H

#include <selecta/tags.h>

SEL_EXTERN_C_BEGIN

typedef enum sel_cmd_id {
#define SEL_CMD(id, args, area, argnames, label) SEL_CMD_##id,
#include <selecta/commands.def>
#undef SEL_CMD
    SEL_CMD_COUNT
} sel_cmd_id;

typedef enum sel_evt_id {
#define SEL_EVT(id, args) SEL_EVT_##id,
#include <selecta/events.def>
#undef SEL_EVT
    SEL_EVT_COUNT
} sel_evt_id;

enum { SEL_MSG_CMD = 0, SEL_MSG_EVT = 1 };

/* Argument types are the signature letters themselves. */
enum { SEL_ARG_STR = 's', SEL_ARG_INT = 'i', SEL_ARG_UINT = 'u', SEL_ARG_TAGS = 't' };
enum { SEL_ARG_RELATIVE = 1u }; /* an `i` argument written with a leading + or - */

typedef struct sel_arg {
    uint8_t type;  /* SEL_ARG_* */
    uint8_t flags; /* SEL_ARG_RELATIVE */
    union {
        int32_t i;
        uint32_t u;
        const char* s;
        const sel_tag* t;
    } v;
} sel_arg;

/* Largest argument count any signature allows (repeats included). */
#define SEL_MAX_ARGS 8

/* Compile-time check: no signature in the tables is longer than SEL_MAX_ARGS. */
#define SEL_CMD(id, args, area, argnames, label) typedef char sel_sig_fits_cmd_##id[(sizeof(args) - 1 <= SEL_MAX_ARGS) ? 1 : -1];
#include <selecta/commands.def>
#undef SEL_CMD
#define SEL_EVT(id, args) typedef char sel_sig_fits_evt_##id[(sizeof(args) - 1 <= SEL_MAX_ARGS) ? 1 : -1];
#include <selecta/events.def>
#undef SEL_EVT

typedef struct sel_message {
    uint32_t kind; /* SEL_MSG_CMD or SEL_MSG_EVT */
    uint32_t id;   /* sel_cmd_id or sel_evt_id */
    uint32_t argc;
    sel_arg args[SEL_MAX_ARGS];
} sel_message;

/* Reflection. Unknown id: NULL / 0. */
const char* sel_cmd_name(uint32_t id);
const char* sel_cmd_args(uint32_t id);
const char* sel_cmd_area(uint32_t id); /* transport, queue, playlists, view or protocol */
const char* sel_cmd_argnames(uint32_t id); /* space separated names, [optional] and repeating..., see commands.def */
const char* sel_cmd_label(uint32_t id);    /* the verb's name for people, full English words */
int sel_cmd_by_name(const char* name, uint32_t* out);
const char* sel_evt_name(uint32_t id);
const char* sel_evt_args(uint32_t id);
int sel_evt_by_name(const char* name, uint32_t* out);

/* Codec. Encode returns bytes written (no trailing newline, always
 * NUL-terminated) or -1 when `cap` is too small. Decode fills `out` from
 * one line; strings and tag lists are copied into `pool`, so the message
 * stays valid as long as the pool does. No allocation on either path. */
enum {
    SEL_CODEC_OK = 0,
    SEL_CODEC_E_EMPTY,
    SEL_CODEC_E_UNKNOWN_VERB,
    SEL_CODEC_E_ARG_COUNT,
    SEL_CODEC_E_ARG_TYPE,
    SEL_CODEC_E_UNTERMINATED,
    SEL_CODEC_E_BAD_TAG,
    SEL_CODEC_E_OVERFLOW
};
int sel_codec_encode(const sel_message* msg, char* out, size_t cap);
int sel_codec_decode(const char* line, uint32_t kind, sel_message* out, char* pool, size_t poolcap);
const char* sel_codec_strerror(int err);

/* First token of a line (the verb), copied into `out`; used to name an
 * unknown verb in an ERROR event. Returns its length, 0 for an empty line. */
size_t sel_codec_verb(const char* line, char* out, size_t cap);

SEL_EXTERN_C_END

#endif /* SELECTA_PROTOCOL_H */
