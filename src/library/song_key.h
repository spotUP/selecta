/* The canonical song key (I211). Split from source.h so code that also includes playlist/playlist.h
 * (which defines its own sel_playlist) can use it. */
#ifndef SELECTA_LIBRARY_SONG_KEY_H
#define SELECTA_LIBRARY_SONG_KEY_H

#include <stddef.h>

/* THE canonical song key of anything a caller holds (D-M3 key rule, I211): a queue path or a key. A string that
 * starts with a registered source id and ':' is a key and stays; a URL ("://") stays; anything else is a local path and gets "local:".
 * Idempotent. Every writer and reader of history, loudness, stickers, votes, ratings, liked, kept, cues, notes and
 * flags goes through this. 0 when it does not fit. */
int sel_song_key_of(const char* ref, char* out, size_t n);

#endif
