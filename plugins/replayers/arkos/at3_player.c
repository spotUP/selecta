/* Arkos Tracker 3 song player (I433b), ported from AT3 SongPlayer.cpp, ChannelPlayer.cpp and PsgPeriod.cpp (MIT,
 * Julien Nevo; commit in VERSION.txt). The AY register glue around Ayumi is written here, not taken from bbmp. */
#include "at3_player.h"

#include <math.h>
#include <string.h>

#define FP(i) ((int32_t)(i) * 256)
#define FP_INT(f) ((int16_t)((f) >> 8)) /* the FpFloat integer part, read signed */

static const at3_track *find_track(const at3_subsong *s, int index) {
  size_t i;
  for (i = 0; i < s->n_tracks; i++)
    if (s->tracks[i].index == index) return &s->tracks[i];
  return NULL;
}

static const at3_cell *find_cell(const at3_track *t, int row) {
  size_t i;
  if (t)
    for (i = 0; i < t->n_cells; i++)
      if (t->cells[i].index == row) return &t->cells[i];
  return NULL;
}

static int speed_cell(const at3_subsong *s, int index, int row) {
  size_t i, k;
  for (i = 0; i < s->n_speed_tracks; i++)
    if (s->speed_tracks[i].index == index)
      for (k = 0; k < s->speed_tracks[i].n_cells; k++)
        if (s->speed_tracks[i].cells[k].index == row) return s->speed_tracks[i].cells[k].value;
  return 0;
}

static int clamp(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

/* FpFloat::correctFrom0To */
static int32_t fp_clamp(int32_t f, int max) { return f < 0 ? 0 : FP_INT(f) > max ? FP(max) : f; }

static const at3_expression *expr(const at3_expression *list, size_t n, int id) {
  return id >= 0 && (size_t)id < n ? &list[id] : NULL;
}

/* Reads the expression value at *step, then advances step/tick the AT3 way (step past end goes to loop start). */
static int expr_step(const int *values, int n, int end, int loop, int speed, int *step, int *tick) {
  int v;
  if (n <= 0) return 0;
  if (*step >= n) *step = 0;
  v = values[*step];
  if (++*tick > speed) {
    *tick = 0;
    if (++*step > end || *step >= n) *step = loop < n ? loop : 0;
  }
  return v;
}

static void reset_effects(at3_channel *c, int volume) {
  c->volume = FP(volume);
  c->volume_slide = 0;
  c->pitch_slide = 0;
  c->inline_arp = 0;
  c->arp_id = 0;
  c->arp_step = c->arp_tick = 0;
  c->forced_arp_speed = -1;
  c->pitch_id = 0;
  c->pitch_step = c->pitch_tick = 0;
  c->forced_pitch_speed = -1;
}

static void inline_arp(at3_channel *c, const int *vals, int n) {
  c->inline_vals[0] = 0; /* the inline arpeggio starts on the base note */
  memcpy(c->inline_vals + 1, vals, (size_t)n * sizeof *vals);
  c->inline_n = n + 1;
  c->inline_arp = 1;
  c->arp_step = c->arp_tick = 0;
  c->forced_arp_speed = -1;
}

static void effect(at3_channel *c, const at3_effect *e) {
  const char *n = e->name;
  int v = e->value;
  if (!strcmp(n, "volume")) {
    c->volume = FP(v);
    c->volume_slide = 0;
  } else if (!strcmp(n, "volumeIn")) {
    c->volume_slide = v & 0xffff;
  } else if (!strcmp(n, "volumeOut")) {
    c->volume_slide = -(v & 0xffff);
  } else if (!strcmp(n, "pitchUp") || !strcmp(n, "fastPitchUp")) {
    c->pitch_slide = -((n[0] == 'f' ? v << 4 : v) & 0xffff);
  } else if (!strcmp(n, "pitchDown") || !strcmp(n, "fastPitchDown")) {
    c->pitch_slide = (n[0] == 'f' ? v << 4 : v) & 0xffff;
  } else if (!strcmp(n, "pitchTable")) {
    c->pitch_id = v;
    c->pitch_step = c->pitch_tick = 0;
  } else if (!strcmp(n, "arpeggioTable")) {
    c->inline_arp = 0;
    c->arp_id = v;
    c->arp_step = c->arp_tick = 0;
  } else if (!strcmp(n, "arpeggio3Notes")) {
    int a[2];
    a[0] = (v >> 4) & 0xf;
    a[1] = v & 0xf;
    inline_arp(c, a, 2);
  } else if (!strcmp(n, "arpeggio4Notes")) {
    int a[3];
    a[0] = (v >> 8) & 0xf;
    a[1] = (v >> 4) & 0xf;
    a[2] = v & 0xf;
    inline_arp(c, a, 3);
  } else if (!strcmp(n, "reset")) {
    reset_effects(c, 15 - (v & 0xf));
  } else if (!strcmp(n, "forceInstrumentSpeed")) {
    c->forced_instr_speed = v;
  } else if (!strcmp(n, "forceArpeggioSpeed")) {
    c->forced_arp_speed = v;
  } else if (!strcmp(n, "forcePitchTableSpeed")) {
    c->forced_pitch_speed = v;
  }
  /* shortcut: pitchGlide plays as a plain note change and legato is ignored; port the glide when a song needs it. */
}

static void read_cell(at3_channel *c, const at3_cell *cell, int transposition) {
  size_t i;
  if (cell->note >= 0) {
    c->base_note = cell->note + transposition;
    c->pitch = c->pitch_slide = 0;
    c->new_note = 1;
    if (cell->instrument >= 0) {
      c->volume_slide = 0;
      c->arp_step = c->arp_tick = 0;
      c->pitch_step = c->pitch_tick = 0;
      c->instr = cell->instrument;
      c->instr_line = c->instr_tick = 0;
      c->forced_instr_speed = -1;
    }
  }
  for (i = 0; i < cell->n_effects; i++) effect(c, &cell->effects[i]);
}

static void trailing_effects(const at3_song *song, at3_channel *c, int within_line) {
  const at3_expression *x;
  int arp = 0;
  if (c->inline_arp) {
    arp = expr_step(c->inline_vals, c->inline_n, c->inline_n - 1, 0,
                    c->forced_arp_speed >= 0 ? c->forced_arp_speed : 0, &c->arp_step, &c->arp_tick);
  } else if ((x = expr(song->arpeggios, song->n_arpeggios, c->arp_id)) != NULL) {
    arp = expr_step(x->values, (int)x->n_values, x->end, x->loop_start,
                    c->forced_arp_speed >= 0 ? c->forced_arp_speed : x->speed, &c->arp_step, &c->arp_tick);
  }
  c->track_note = clamp(c->base_note + arp, 0, 127);
  if (within_line) {
    c->pitch += c->pitch_slide;
    c->volume = fp_clamp(c->volume + c->volume_slide, 15);
  }
  c->pitch_table_value = 0;
  if ((x = expr(song->pitches, song->n_pitches, c->pitch_id)) != NULL)
    c->pitch_table_value = expr_step(x->values, (int)x->n_values, x->end, x->loop_start,
                                     c->forced_pitch_speed >= 0 ? c->forced_pitch_speed : x->speed, &c->pitch_step,
                                     &c->pitch_tick);
}

/* ChannelPlayer::playPsgInstrumentFrame */
static void instrument_frame(const at3_song *song, at3_channel *c, const uint16_t *periods) {
  const at3_instrument *in =
      c->instr >= 0 && (size_t)c->instr < song->n_instruments ? &song->instruments[c->instr] : NULL;
  const at3_psg_cell *cell;
  int link, sw_arp, sw_pitch, sw_forced, hw_arp, hw_pitch, hw_forced, vol = 0, sw = 0, hw = 0, speed;
  c->noise = 0;
  c->sound_open = 0;
  c->hw_retrig = 0;
  c->vol = 0;
  if (in == NULL || in->type != AT3_INSTRUMENT_PSG || in->n_cells == 0) return;
  if ((size_t)c->instr_line >= in->n_cells) c->instr_line = 0;
  cell = &in->cells[c->instr_line];
  link = cell->link;
  c->hw_retrig = c->new_note && c->instr_tick == 0 && in->retrig;
  c->noise = cell->noise;
  c->sound_open = link != AT3_LINK_HARDWARE_ONLY && link != AT3_LINK_NONE;
  /* SpreadPsgInstrumentCell::buildLowLevelCell: hardware-first links keep the hardware side in "primary" */
  if (link == AT3_LINK_HARDWARE_ONLY || link == AT3_LINK_HARDWARE_TO_SOFTWARE) {
    hw_forced = cell->primary_period, hw_arp = cell->primary_arp_note + cell->primary_arp_octave * 12;
    hw_pitch = cell->primary_pitch;
    sw_forced = cell->secondary_period, sw_arp = cell->secondary_arp_note + cell->secondary_arp_octave * 12;
    sw_pitch = cell->secondary_pitch;
  } else {
    sw_forced = cell->primary_period, sw_arp = cell->primary_arp_note + cell->primary_arp_octave * 12;
    sw_pitch = cell->primary_pitch;
    hw_forced = cell->secondary_period, hw_arp = cell->secondary_arp_note + cell->secondary_arp_octave * 12;
    hw_pitch = cell->secondary_pitch;
  }
  if (link == AT3_LINK_NONE || link == AT3_LINK_SOFTWARE_ONLY) {
    vol = cell->volume;
  } else {
    vol = 16;
    c->hw_env = cell->hardware_envelope;
    if (!c->hw_retrig) c->hw_retrig = cell->retrig && c->instr_tick == 0;
  }
  if (link == AT3_LINK_SOFTWARE_ONLY || link == AT3_LINK_SOFTWARE_AND_HARDWARE || link == AT3_LINK_SOFTWARE_TO_HARDWARE) {
    if (sw_forced != 0) {
      sw = sw_forced;
    } else {
      sw = periods[clamp(c->track_note + sw_arp, 0, 127)];
      sw += FP_INT(c->pitch) - c->pitch_table_value - sw_pitch;
    }
  }
  if (link == AT3_LINK_SOFTWARE_TO_HARDWARE) {
    hw = sw >> cell->ratio;
    if (cell->ratio > 0 && ((sw >> (cell->ratio - 1)) & 1)) ++hw;
    hw -= hw_pitch;
  } else if (link == AT3_LINK_HARDWARE_ONLY || link == AT3_LINK_HARDWARE_TO_SOFTWARE ||
             link == AT3_LINK_SOFTWARE_AND_HARDWARE) {
    if (hw_forced != 0) {
      hw = hw_forced;
    } else {
      hw = periods[clamp(c->track_note + hw_arp, 0, 127)];
      hw += FP_INT(c->pitch) - c->pitch_table_value - hw_pitch;
    }
  }
  if (link == AT3_LINK_HARDWARE_TO_SOFTWARE) sw = (hw << cell->ratio) - sw_pitch;
  c->sw_period = sw & 0xfff;
  c->hw_period = hw & 0xffff;
  if (vol <= 15) {
    vol -= 15 - FP_INT(c->volume);
    if (vol < 0) vol = 0;
  }
  c->vol = vol;
  speed = c->forced_instr_speed >= 0 ? c->forced_instr_speed : in->speed;
  if (++c->instr_tick > speed) {
    c->instr_tick = 0;
    if (++c->instr_line > in->end) {
      if (in->looping) {
        c->instr_line = in->loop_start;
      } else {
        c->instr_line = 0;
        c->instr = -1;
      }
    }
  }
}

/* SongPlayer::getNextRegisters + fillWithChannelResult + advanceInSong; `sound` 0 runs the sequencer only. */
static void tick(at3_player *p, int sound) {
  const at3_subsong *s = p->sub;
  const at3_position *pos = &s->positions[p->position];
  const at3_pattern *pat = (size_t)pos->pattern_index < s->n_patterns ? &s->patterns[pos->pattern_index] : NULL;
  int i, k;
  if (p->tick == 0 && pat != NULL) {
    int sp = speed_cell(s, pat->speed_track_index, p->line);
    if (sp > 0) p->speed = sp;
    for (i = 0; i < p->n_channels && (size_t)i < pat->n_channels; i++) {
      const at3_cell *cell = find_cell(find_track(s, pat->track_index[i]), p->line);
      int tr = 0;
      size_t t;
      for (t = 0; t < pos->n_transpositions; t++)
        if (pos->transpositions[t * 2] == i) tr = pos->transpositions[t * 2 + 1];
      if (cell) read_cell(&p->ch[i], cell, tr);
    }
  }
  for (i = 0; i < p->n_channels; i++) {
    at3_channel *c = &p->ch[i];
    trailing_effects(p->song, c, p->tick < p->speed);
    instrument_frame(p->song, c, p->periods[i / 3]);
    c->new_note = 0;
  }
  if (sound) {
    for (k = 0; k < p->n_psgs; k++) {
      struct ayumi *a = &p->ay[k];
      int shape = -1, retrig = 0;
      for (i = 0; i < 3; i++) {
        const at3_channel *c = &p->ch[k * 3 + i];
        ayumi_set_tone(a, i, c->sw_period);
        if (c->noise > 0) ayumi_set_noise(a, c->noise & 0x1f);
        ayumi_set_mixer(a, i, !c->sound_open, c->noise <= 0, c->vol > 15);
        ayumi_set_volume(a, i, c->vol > 15 ? 0 : c->vol);
        if (c->vol > 15) { /* last hardware channel of the PSG wins, as on the chip */
          ayumi_set_envelope(a, c->hw_period);
          shape = c->hw_env & 0xf;
          retrig |= c->hw_retrig;
        }
      }
      if (shape >= 0 && (retrig || shape != p->last_shape[k])) {
        ayumi_set_envelope_shape(a, shape); /* restarts the envelope: only on retrig or a new shape */
        p->last_shape[k] = shape;
      }
    }
  }
  if (++p->tick >= p->speed) {
    p->tick = 0;
    if (++p->line >= pos->height) {
      p->line = 0;
      if (++p->position > s->end_position || (size_t)p->position >= s->n_positions) {
        p->position = s->loop_start_position >= 0 && (size_t)s->loop_start_position < s->n_positions
                          ? s->loop_start_position
                          : 0;
        p->ended = 1;
      }
    }
  }
}

int at3_player_start(at3_player *p, const at3_song *song, int subsong, int rate) {
  const at3_subsong *s;
  int i, n, k;
  double frame_hz;
  if (song == NULL || subsong < 0 || (size_t)subsong >= song->n_subsongs || rate <= 0) return -1;
  s = &song->subsongs[subsong];
  if (s->n_positions == 0 || s->n_psgs == 0 || s->n_patterns == 0) return -1;
  memset(p, 0, sizeof *p);
  p->song = song;
  p->sub = s;
  p->n_psgs = (int)(s->n_psgs < AT3_MAX_PSGS ? s->n_psgs : AT3_MAX_PSGS);
  p->n_channels = p->n_psgs * 3;
  p->speed = s->initial_speed > 0 ? s->initial_speed : 6;
  frame_hz = s->replay_milli_hz > 0 ? s->replay_milli_hz / 1000.0 : 50.0;
  p->per_tick = rate / frame_hz;
  p->gain = 32767.0 / 1.5 / p->n_psgs; /* three channels at full volume give 1.5 on a side */
  for (k = 0; k < p->n_psgs; k++) {
    const at3_psg *g = &s->psgs[k];
    double clock = g->frequency_hz > 0 ? g->frequency_hz : 1000000.0;
    double ref = g->reference_milli_hz > 0 ? g->reference_milli_hz / 1000.0 : 440.0;
    double pan[3] = {0.5, 0.5, 0.5};
    if (!strcmp(g->mixing, "ABC")) pan[0] = 0.2, pan[2] = 0.8;
    else if (!strcmp(g->mixing, "ACB")) pan[0] = 0.2, pan[1] = 0.8;
    else if (!strcmp(g->mixing, "BAC")) pan[1] = 0.2, pan[2] = 0.8;
    else if (!strcmp(g->mixing, "BCA")) pan[1] = 0.2, pan[0] = 0.8;
    else if (!strcmp(g->mixing, "CAB")) pan[2] = 0.2, pan[1] = 0.8;
    else if (!strcmp(g->mixing, "CBA")) pan[2] = 0.2, pan[0] = 0.8;
    /* PsgPeriod::getPeriod over the 128 notes */
    for (n = 0; n < 128; n++) {
      double f = ref * pow(2.0, (n / 12 - 3) + ((n % 12) + 1 - 10) / 12.0);
      long per = lround(clock / 8.0 / f);
      p->periods[k][n] = (uint16_t)(per > 0xffff ? 0xffff : per);
    }
    if (!ayumi_configure(&p->ay[k], g->type_ym, clock, rate)) return -1;
    for (i = 0; i < 3; i++) ayumi_set_pan(&p->ay[k], i, pan[i], 0);
    p->last_shape[k] = -1;
  }
  for (i = 0; i < p->n_channels; i++) {
    at3_channel *c = &p->ch[i];
    c->instr = -1;
    c->volume = FP(15);
    c->forced_instr_speed = c->forced_arp_speed = c->forced_pitch_speed = -1;
  }
  return 0;
}

uint32_t at3_player_count_ticks(at3_player *p) {
  uint32_t n = 0;
  while (!p->ended && n < 50u * 60u * 60u) { /* one hour of 50 Hz ticks stops a broken song */
    tick(p, 0);
    n++;
  }
  return n;
}

void at3_player_skip(at3_player *p, uint32_t ticks) {
  while (ticks-- && !p->ended) tick(p, 0);
}

uint32_t at3_player_render(at3_player *p, int16_t *out, uint32_t frames) {
  uint32_t done = 0;
  while (done < frames) {
    uint32_t n, i;
    int k;
    if (p->owed < 1.0) {
      if (p->ended) break;
      tick(p, 1);
      p->owed += p->per_tick;
    }
    n = (uint32_t)p->owed;
    if (n > frames - done) n = frames - done;
    for (i = 0; i < n; i++) {
      double l = 0, r = 0;
      long sl, sr;
      for (k = 0; k < p->n_psgs; k++) {
        ayumi_process(&p->ay[k]);
        ayumi_remove_dc(&p->ay[k]);
        l += p->ay[k].left;
        r += p->ay[k].right;
      }
      sl = lround(l * p->gain);
      sr = lround(r * p->gain);
      out[(done + i) * 2] = (int16_t)(sl < -32768 ? -32768 : sl > 32767 ? 32767 : sl);
      out[(done + i) * 2 + 1] = (int16_t)(sr < -32768 ? -32768 : sr > 32767 ? 32767 : sr);
    }
    done += n;
    p->owed -= n;
  }
  return done;
}
