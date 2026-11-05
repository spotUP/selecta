/* Arkos Tracker 3 song model and .aks importer (row I433a).
 *
 * The model mirrors the song data of Arkos Tracker 3 (Julien Nevo, MIT, bitbucket.org/JulienNevo/arkostracker3,
 * source/song and source/import/at3). The importer reads the format the AT3 application writes: a zip archive
 * holding one XML document (formatVersion 3.0), or that XML document unpacked. There is no JUCE and no floating
 * point: frequencies are stored as milli-Hz integers so the model also builds for the 68020 target.
 * No player here (I433b). All strings are UTF-8 and owned by the song. */
#ifndef SELECTA_ARKOS_AT3_SONG_H
#define SELECTA_ARKOS_AT3_SONG_H

#include <stddef.h>

#define AT3_NAME_MAX 64

/* Instrument cell link kinds (the "link" element). */
typedef enum {
  AT3_LINK_NONE = 0, /* noSoftwareNoHardware */
  AT3_LINK_SOFTWARE_ONLY,
  AT3_LINK_HARDWARE_ONLY,
  AT3_LINK_SOFTWARE_TO_HARDWARE,
  AT3_LINK_HARDWARE_TO_SOFTWARE,
  AT3_LINK_SOFTWARE_AND_HARDWARE
} at3_link;

typedef enum { AT3_INSTRUMENT_PSG = 0, AT3_INSTRUMENT_SAMPLE } at3_instrument_type;

typedef struct {
  int volume, noise;
  int primary_period, primary_arp_note, primary_arp_octave, primary_pitch;
  at3_link link;
  int ratio, hardware_envelope;
  int secondary_period, secondary_arp_note, secondary_arp_octave, secondary_pitch;
  int retrig;
} at3_psg_cell;

typedef struct {
  char name[AT3_NAME_MAX];
  at3_instrument_type type;
  int speed, retrig, loop_start, end, looping;
  size_t n_cells; /* PSG instruments only; a sample instrument keeps 0 (sample data is I433b) */
  at3_psg_cell *cells;
} at3_instrument;

typedef struct {
  char name[AT3_NAME_MAX];
  int is_arpeggio, speed, shift, loop_start, end;
  size_t n_values;
  int *values;
} at3_expression;

typedef struct {
  char name[AT3_NAME_MAX]; /* "arpeggioTable", "volume", ... */
  int index, value;
} at3_effect;

typedef struct {
  int index; /* row inside the track */
  int note;  /* -1: no note */
  int instrument; /* -1: none */
  size_t n_effects;
  at3_effect *effects;
} at3_cell;

typedef struct {
  int index;
  size_t n_cells;
  at3_cell *cells;
} at3_track;

typedef struct {
  int index, row, value; /* speed track: speed; event track: event */
} at3_special_cell;

typedef struct {
  int index;
  size_t n_cells;
  at3_special_cell *cells;
} at3_special_track;

typedef struct {
  int type_ym; /* 0 ay, 1 ym */
  int frequency_hz, sample_player_hz;
  int reference_milli_hz;
  char mixing[8]; /* ABC, ACB, ... */
} at3_psg;

typedef struct {
  int pattern_index, height;
  size_t n_transpositions;
  int *transpositions; /* pairs: channel, value */
} at3_position;

typedef struct {
  size_t n_channels;
  int *track_index; /* one per channel */
  int speed_track_index, event_track_index;
} at3_pattern;

typedef struct {
  char title[AT3_NAME_MAX];
  int initial_speed, digi_channel, loop_start_position, end_position;
  int replay_milli_hz; /* 50000 for 50 Hz */
  size_t n_psgs, n_tracks, n_speed_tracks, n_event_tracks, n_positions, n_patterns;
  at3_psg *psgs;
  at3_track *tracks;
  at3_special_track *speed_tracks, *event_tracks;
  at3_position *positions;
  at3_pattern *patterns;
} at3_subsong;

typedef struct at3_song {
  char *title, *author, *composer, *comment;
  size_t n_instruments, n_arpeggios, n_pitches, n_subsongs;
  at3_instrument *instruments;
  at3_expression *arpeggios, *pitches;
  at3_subsong *subsongs;
} at3_song;

/* Parses a .aks file image. Returns 0 and a song to release with at3_song_free, or -1 with a message in err
 * (err may be NULL). A damaged or foreign file fails here without reading outside the buffer. */
int at3_song_load(const void *data, size_t size, at3_song **out, char *err, size_t err_size);
void at3_song_free(at3_song *song);

#endif
