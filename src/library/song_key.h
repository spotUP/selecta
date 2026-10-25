/* The canonical song key (I211). Split from source.h so code that also includes playlist/playlist.h
 * (which defines its own sel_playlist) can use it. */
#ifndef SELECTA_LIBRARY_SONG_KEY_H
#define SELECTA_LIBRARY_SONG_KEY_H

#include <stddef.h>
#include <stdint.h>

/* Longest song key incl. NUL: id + ':' + path. Every buffer that carries a song key (library, home view rows) is sized
 * from this one number (I263). */
#define SEL_SONG_KEY_MAX 1200u

/* THE canonical song key of anything a caller holds (D-M3 key rule, I211): a queue path or a key. A string that
 * starts with a registered source id and ':' is a key and stays; a URL ("://") stays; anything else is a local path and gets "local:".
 * Idempotent. Every writer and reader of history, loudness, stickers, votes, ratings, liked, kept, cues, notes and
 * flags goes through this. 0 when it does not fit. */
int sel_song_key_of(const char* ref, char* out, size_t n);

/* Calls `fn` once per registered source in registration order: id, display name, home URL (may be NULL) and 1 when its
 * songs live off the machine (the SOURCES command, DUI-P4-3). Returns the rows. Split from source.h for the same reason. */
typedef void (*sel_source_each_fn)(void* ctx, const char* id, const char* name, const char* home, int remote);
uint32_t sel_source_each(sel_source_each_fn fn, void* ctx);

#endif
