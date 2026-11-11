/* Magic byte table for nuade content detection (I1305i2).
 *
 * On the Amiga the nuade players live in an on-demand LoadSeg module (Players/sel_nuade, 1.6 MB). The registry
 * links only the manifest, so the manifest cannot ask the players' probes without loading that module. Johan's
 * decision 2026-11-11 (ledger I1305i): each player with a fixed header declares its magic bytes here (offset +
 * bytes), the manifest probe matches them with zero player code and no allocation, and players without a fixed
 * header fall back to the file extension. The table mirrors the header gates of the players' own probes in
 * third_party/nuade/players/<id>/; test nuade-signature-probe-match checks that it agrees with the real probes.
 *
 * Plain C, no nuade or selecta headers: the host test links it next to the real players. */
#ifndef SEL_NUADE_SIGNATURES_H
#define SEL_NUADE_SIGNATURES_H

#include <stddef.h>
#include <stdint.h>

/* Number of table entries (one per player with a fixed header). */
size_t sel_nuade_sig_count(void);
/* Player id of entry i ("protracker4"), NULL past the end. */
const char* sel_nuade_sig_player(size_t i);
/* 1 when the header in data[0..size) matches one of entry i's signatures, else 0. */
int sel_nuade_sig_match_entry(size_t i, const uint8_t* data, size_t size);
/* Player id whose signature matches data, NULL when none does. */
const char* sel_nuade_sig_match(const uint8_t* data, size_t size);
/* 1 when `name` (a file name, no directory) has a prefix ("mod.x") or suffix ("x.mod") in the comma list `exts`,
 * case-insensitive. Same rule as the engine's EXTENSIONS claim (src/engine/claims.c), which plugins cannot link. */
int sel_nuade_sig_name_claimed(const char* exts, const char* name);

#endif
