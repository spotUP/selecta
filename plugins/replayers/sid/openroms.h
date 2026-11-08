/* OpenROMs replacement C64 ROM images (MEGA65/open-roms, LGPL-3.0-or-later; third_party/openroms),
 * embedded at configure time by CMakeLists.txt. D10: the default ROMs of the SID replayer; a file
 * in the config directory wins. */
#ifndef SELECTA_SID_OPENROMS_H
#define SELECTA_SID_OPENROMS_H
#include <stdint.h>

extern const uint8_t sid_openrom_kernal[8192];
extern const uint8_t sid_openrom_basic[8192];
extern const uint8_t sid_openrom_chargen[4096];
/* Polly Tracker player, RSID header + 962 bytes (I710ar, third_party/polly_player). */
#define SID_POLLY_PLAYER_SIZE 1086u
extern const uint8_t sid_polly_player[SID_POLLY_PLAYER_SIZE];
#endif
