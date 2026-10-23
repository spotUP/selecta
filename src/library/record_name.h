/* File naming and tagging for recordings (F36 stream recording, I196c; F6 will reuse it for the analysis/record
 * render, the design doc says both share "file naming, tag writing"). There was no F6 record path in the tree when
 * this was written, so this is the one shared helper. */
#ifndef SELECTA_LIBRARY_RECORD_NAME_H
#define SELECTA_LIBRARY_RECORD_NAME_H

#include <stddef.h>
#include <stdint.h>

/* Builds "<dir>/<name>.<ext>" into out. `name` is the song title (sanitised: path separators, reserved and control
 * characters become '_', surrounding blanks are dropped, at most 120 bytes); NULL or empty falls back to `fallback`
 * (then "stream"). When that file already exists, " (2)", " (3)" ... is appended so a recording never overwrites an
 * earlier one. `dir` is created (one level) when missing. Returns 1 on success, 0 when out is too small. */
int sel_record_name(char* out, size_t cap, const char* dir, const char* name, const char* fallback, const char* ext);

/* Extension for a response media type ("audio/mpeg" -> "mp3"), else the extension of the URL path (copied into
 * buf), else "mp3". */
const char* sel_record_ext(const char* content_type, const char* url, char* buf, size_t cap);

/* Tag block reserved at the start of an MP3 recording: ID3v2.3 with one TIT2 frame, padded to a fixed size so the
 * title can be written after the first bytes were already recorded. */
#define SEL_RECORD_ID3_SIZE 512u
/* Fills dst[SEL_RECORD_ID3_SIZE] with the tag block for `title` (NULL or empty: an empty tag). */
void sel_record_id3(uint8_t* dst, const char* title);

/* I196d: a recording that splits per song (ICY title change) cuts between two MP3 frames or two Ogg pages, never
 * inside one. The walker follows the frame (page) lengths of the bytes it is fed and holds back the header bytes of
 * the next frame until they are known to start one. Other formats, and a stream that stays out of sync for
 * SEL_RECORD_CUT_GIVE_UP bytes, split at the next byte. */
#define SEL_RECORD_CUT_HOLD 282u      /* the largest Ogg page header: 27 bytes and 255 lacing values */
#define SEL_RECORD_CUT_GIVE_UP 65536u
typedef struct sel_record_cut {
    int kind;        /* 0: cut anywhere, 1: MPEG audio frames, 2: Ogg pages */
    int split;       /* 0: none asked, 1: at the next frame start, 2: at the frame start after the held one */
    uint32_t left;   /* bytes of the current frame still to pass */
    uint32_t held;   /* header bytes held back */
    uint32_t lost;   /* bytes passed out of sync since the last frame */
    uint8_t hold[SEL_RECORD_CUT_HOLD];
} sel_record_cut;

/* Receives the bytes in stream order. `split` = 1: the new recording starts with these bytes. */
typedef void (*sel_record_cut_out)(void* ctx, const uint8_t* p, size_t n, int split);

/* `ext` is the recording's extension ("mp3", "ogg", ...). */
void sel_record_cut_init(sel_record_cut* c, const char* ext);
/* Asks for a split at the first frame that starts at or after the bytes fed so far. */
void sel_record_cut_request(sel_record_cut* c);
void sel_record_cut_feed(sel_record_cut* c, const uint8_t* p, size_t len, sel_record_cut_out out, void* ctx);
/* Passes on the held bytes (end of stream). */
void sel_record_cut_flush(sel_record_cut* c, sel_record_cut_out out, void* ctx);

#endif
