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

#endif
