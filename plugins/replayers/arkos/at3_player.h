/* Arkos Tracker 3 song player (I433b): a plain C port of the AT3 SongPlayer and ChannelPlayer
 * (source/player/SongPlayer.cpp, source/player/channel/ChannelPlayer.cpp, MIT, Julien Nevo; see VERSION.txt) that
 * drives one Ayumi AY/YM emulator per PSG. PSG instruments only: sample and digidrum instruments, SID effects,
 * auto-spread instrument cells, the glide effect and event tracks are not played yet. */
#ifndef SELECTA_ARKOS_AT3_PLAYER_H
#define SELECTA_ARKOS_AT3_PLAYER_H

#include <stdint.h>

#include "at3_song.h"
#include "ayumi.h"

/* shortcut: four PSGs (12 channels) at most; AT3 allows more, raise it when a song needs it. */
#define AT3_MAX_PSGS 4
#define AT3_MAX_CHANNELS (AT3_MAX_PSGS * 3)

typedef struct {
  int base_note, track_note, new_note;
  int32_t pitch, pitch_slide, volume, volume_slide; /* 8.8 fixed point, the AT3 FpFloat */
  int instr, instr_line, instr_tick, forced_instr_speed;
  int arp_id, arp_step, arp_tick, forced_arp_speed, inline_arp, inline_n, inline_vals[4];
  int pitch_id, pitch_step, pitch_tick, forced_pitch_speed, pitch_table_value;
  /* output of the last frame */
  int vol, noise, sound_open, sw_period, hw_period, hw_env, hw_retrig;
} at3_channel;

typedef struct {
  const at3_song *song;
  const at3_subsong *sub;
  int n_channels, n_psgs;
  int position, line, tick, speed, ended;
  at3_channel ch[AT3_MAX_CHANNELS];
  uint16_t periods[AT3_MAX_PSGS][128];
  int last_shape[AT3_MAX_PSGS];
  struct ayumi ay[AT3_MAX_PSGS];
  double gain, per_tick, owed;
} at3_player;

/* Sets up subsong `subsong` at `rate` Hz output. Returns 0, or -1 when the subsong can't be played. */
int at3_player_start(at3_player *p, const at3_song *song, int subsong, int rate);
/* Ticks played before the song first loops, for the duration (runs the sequencer without sound). */
uint32_t at3_player_count_ticks(at3_player *p);
/* Runs `ticks` sequencer ticks without sound (seek). */
void at3_player_skip(at3_player *p, uint32_t ticks);
/* Interleaved stereo int16. Returns frames written; fewer than asked once the song has looped. */
uint32_t at3_player_render(at3_player *p, int16_t *out, uint32_t frames);

#endif
