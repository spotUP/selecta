/* Arkos Tracker 3 .aks importer (row I433a). See at3_song.h. Field names follow the AT3 serializers
(source/business/serialization/song/ and source/export/at3/SongNodes.h of the upstream tree). */
#include "at3_song.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "miniz.h"

/* ---- minimal XML tree (elements, text, attributes skipped; no namespaces, no DTD) ---- */

typedef struct {
  const char *name;
  size_t nlen;
  const char *text;
  size_t tlen;
  long first, last, next;
} xnode;

typedef struct {
  xnode *n;
  size_t count, cap;
} xdoc;

#define XDEPTH 64

static int xnew(xdoc *d, const char *name, size_t nlen) {
  xnode *p;
  if (d->count == d->cap) {
    size_t cap = d->cap ? d->cap * 2 : 1024;
    p = (xnode *)realloc(d->n, cap * sizeof *p);
    if (!p) return -1;
    d->n = p;
    d->cap = cap;
  }
  p = &d->n[d->count];
  p->name = name;
  p->nlen = nlen;
  p->text = NULL;
  p->tlen = 0;
  p->first = p->last = p->next = -1;
  return (int)d->count++;
}

static int is_name_char(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-' || c == ':' || c == '.';
}

/* Returns 0 and the root index in *root, or -1 with a reason. buf is not modified and must outlive the doc. */
static int xparse(xdoc *d, const char *buf, size_t size, long *root, const char **why) {
  long stack[XDEPTH];
  size_t sp = 0, p = 0;
  *root = -1;
  while (p < size) {
    size_t lt = p;
    while (lt < size && buf[lt] != '<') lt++;
    if (lt >= size) break;
    p = lt + 1;
    if (p >= size) { *why = "truncated tag"; return -1; }
    if (buf[p] == '?' || buf[p] == '!') {
      if (buf[p] == '!' && p + 2 < size && buf[p + 1] == '-' && buf[p + 2] == '-') {
        while (p + 2 < size && !(buf[p] == '-' && buf[p + 1] == '-' && buf[p + 2] == '>')) p++;
        p += 3;
      } else {
        while (p < size && buf[p] != '>') p++;
        p++;
      }
      continue;
    }
    if (buf[p] == '/') {
      size_t s = ++p, e;
      while (p < size && is_name_char(buf[p])) p++;
      e = p;
      while (p < size && buf[p] != '>') p++;
      if (p >= size || sp == 0) { *why = "unbalanced closing tag"; return -1; }
      {
        xnode *nd = &d->n[stack[sp - 1]];
        if (nd->nlen != e - s || memcmp(nd->name, buf + s, e - s) != 0) { *why = "mismatched closing tag"; return -1; }
        if (nd->first < 0 && nd->text) nd->tlen = lt - (size_t)(nd->text - buf);
        else nd->text = NULL;
      }
      sp--;
      p++;
      continue;
    }
    {
      size_t s = p;
      int self = 0;
      long id;
      while (p < size && is_name_char(buf[p])) p++;
      if (p == s) { *why = "empty tag name"; return -1; }
      id = xnew(d, buf + s, p - s);
      if (id < 0) { *why = "out of memory"; return -1; }
      /* attributes: skip, honouring quotes */
      while (p < size && buf[p] != '>') {
        if (buf[p] == '"' || buf[p] == '\'') {
          char q = buf[p++];
          while (p < size && buf[p] != q) p++;
        } else if (buf[p] == '/' && p + 1 < size && buf[p + 1] == '>') {
          self = 1;
        }
        p++;
      }
      if (p >= size) { *why = "truncated tag"; return -1; }
      p++;
      if (sp > 0) {
        xnode *par = &d->n[stack[sp - 1]];
        if (par->last >= 0) d->n[par->last].next = id;
        else par->first = id;
        par->last = id;
      } else if (*root < 0) {
        *root = id;
      } else {
        *why = "more than one root element";
        return -1;
      }
      if (!self) {
        if (sp == XDEPTH) { *why = "nesting too deep"; return -1; }
        d->n[id].text = buf + p;
        stack[sp++] = id;
      }
    }
  }
  if (sp != 0) { *why = "unclosed element"; return -1; }
  if (*root < 0) { *why = "no root element"; return -1; }
  return 0;
}

static long xchild(const xdoc *d, long parent, const char *name) {
  size_t nl = strlen(name);
  long c;
  if (parent < 0) return -1;
  for (c = d->n[parent].first; c >= 0; c = d->n[c].next)
    if (d->n[c].nlen == nl && memcmp(d->n[c].name, name, nl) == 0) return c;
  return -1;
}

static long xnext(const xdoc *d, long node, const char *name) {
  size_t nl = strlen(name);
  long c;
  for (c = d->n[node].next; c >= 0; c = d->n[c].next)
    if (d->n[c].nlen == nl && memcmp(d->n[c].name, name, nl) == 0) return c;
  return -1;
}

static size_t xcount(const xdoc *d, long parent, const char *name) {
  size_t n = 0;
  long c = xchild(d, parent, name);
  for (; c >= 0; c = xnext(d, c, name)) n++;
  return n;
}

/* ---- values ---- */

static int xint(const xdoc *d, long parent, const char *name, int def) {
  long c = xchild(d, parent, name);
  const char *s, *e;
  long v = 0;
  int neg = 0;
  if (c < 0 || !d->n[c].text) return def;
  s = d->n[c].text;
  e = s + d->n[c].tlen;
  while (s < e && (*s == ' ' || *s == '\n' || *s == '\t' || *s == '\r')) s++;
  if (s < e && (*s == '-' || *s == '+')) neg = (*s++ == '-');
  if (s >= e || *s < '0' || *s > '9') return def;
  while (s < e && *s >= '0' && *s <= '9') {
    v = v * 10 + (*s++ - '0');
    if (v > 2147483647L) v = 2147483647L; /* the colour fields reach 4294967295: clamp, they are not used */
  }
  return neg ? (int)-v : (int)v;
}

/* Decimal with up to 3 fraction digits as thousandths ("12.5" -> 12500). */
static int xmilli(const xdoc *d, long parent, const char *name, int def) {
  long c = xchild(d, parent, name);
  const char *s, *e;
  long whole = 0, frac = 0;
  int digits = 0;
  if (c < 0 || !d->n[c].text) return def;
  s = d->n[c].text;
  e = s + d->n[c].tlen;
  while (s < e && *s == ' ') s++;
  if (s >= e || *s < '0' || *s > '9') return def;
  while (s < e && *s >= '0' && *s <= '9') {
    whole = whole * 10 + (*s++ - '0');
    if (whole > 1000000) whole = 1000000;
  }
  if (s < e && *s == '.') {
    s++;
    while (s < e && *s >= '0' && *s <= '9') {
      if (digits < 3) { frac = frac * 10 + (*s - '0'); digits++; }
      s++;
    }
  }
  while (digits++ < 3) frac *= 10;
  return (int)(whole * 1000 + frac);
}

static int xis(const xdoc *d, long c, const char *s) {
  size_t n = strlen(s);
  return c >= 0 && d->n[c].text && d->n[c].tlen == n && memcmp(d->n[c].text, s, n) == 0;
}

/* Copies the text of a child, decoding the five predefined entities, truncating to cap-1 bytes. */
static void xstr(const xdoc *d, long parent, const char *name, char *out, size_t cap) {
  long c = xchild(d, parent, name);
  size_t o = 0, i, n;
  const char *t;
  out[0] = 0;
  if (c < 0 || !d->n[c].text) return;
  t = d->n[c].text;
  n = d->n[c].tlen;
  for (i = 0; i < n && o + 1 < cap; i++) {
    if (t[i] == '&') {
      static const struct { const char *e; char c; } ent[] = {{"&amp;", '&'}, {"&lt;", '<'}, {"&gt;", '>'}, {"&quot;", '"'}, {"&apos;", '\''}};
      size_t k;
      for (k = 0; k < 5; k++) {
        size_t l = strlen(ent[k].e);
        if (i + l <= n && memcmp(t + i, ent[k].e, l) == 0) { out[o++] = ent[k].c; i += l - 1; break; }
      }
      if (k < 5) continue;
    }
    out[o++] = t[i];
  }
  out[o] = 0;
}

static char *xdup(const xdoc *d, long parent, const char *name) {
  long c = xchild(d, parent, name);
  size_t cap = (c >= 0 && d->n[c].text ? d->n[c].tlen : 0) + 1;
  char *s = (char *)malloc(cap);
  if (s) xstr(d, parent, name, s, cap);
  return s;
}

/* ---- model building ---- */

#define NEED(p) do { if (!(p)) return -1; } while (0)

static at3_link link_of(const xdoc *d, long c) {
  static const char *const names[] = {"noSoftwareNoHardware", "softwareOnly", "hardwareOnly", "softwareToHardware", "hardwareToSoftware", "softwareAndHardware"};
  int i;
  for (i = 0; i < 6; i++) if (xis(d, c, names[i])) return (at3_link)i;
  return AT3_LINK_NONE;
}

static int read_instrument(const xdoc *d, long n, at3_instrument *in) {
  long cells, c;
  size_t k = 0;
  xstr(d, n, "name", in->name, sizeof in->name);
  in->type = xis(d, xchild(d, n, "type"), "psg") ? AT3_INSTRUMENT_PSG : AT3_INSTRUMENT_SAMPLE;
  in->speed = xint(d, n, "speed", 0);
  in->retrig = xint(d, n, "isRetrig", 0) || xis(d, xchild(d, n, "isRetrig"), "true");
  in->loop_start = xint(d, n, "loopStartIndex", 0);
  in->end = xint(d, n, "endIndex", 0);
  in->looping = xis(d, xchild(d, n, "isLooping"), "true");
  if (in->type != AT3_INSTRUMENT_PSG) return 0;
  cells = xchild(d, n, "cells");
  in->n_cells = xcount(d, cells, "cell");
  if (in->n_cells == 0) return 0;
  in->cells = (at3_psg_cell *)calloc(in->n_cells, sizeof *in->cells);
  NEED(in->cells);
  for (c = xchild(d, cells, "cell"); c >= 0; c = xnext(d, c, "cell"), k++) {
    at3_psg_cell *pc = &in->cells[k];
    pc->volume = xint(d, c, "volume", 0);
    pc->noise = xint(d, c, "noise", 0);
    pc->primary_period = xint(d, c, "primaryPeriod", 0);
    pc->primary_arp_note = xint(d, c, "primaryArpeggioNoteInOctave", 0);
    pc->primary_arp_octave = xint(d, c, "primaryArpeggioOctave", 0);
    pc->primary_pitch = xint(d, c, "primaryPitch", 0);
    pc->link = link_of(d, xchild(d, c, "link"));
    pc->ratio = xint(d, c, "ratio", 4);
    pc->hardware_envelope = xint(d, c, "hardwareEnvelope", 8);
    pc->secondary_period = xint(d, c, "secondaryPeriod", 0);
    pc->secondary_arp_note = xint(d, c, "secondaryArpeggioNoteInOctave", 0);
    pc->secondary_arp_octave = xint(d, c, "secondaryArpeggioOctave", 0);
    pc->secondary_pitch = xint(d, c, "secondaryPitch", 0);
    pc->retrig = xis(d, xchild(d, c, "isRetrig"), "true");
  }
  return 0;
}

static int read_expressions(const xdoc *d, long root, at3_expression **out, size_t *count) {
  size_t i = 0;
  long e;
  *count = xcount(d, root, "expression");
  if (*count == 0) return 0;
  *out = (at3_expression *)calloc(*count, sizeof **out);
  NEED(*out);
  for (e = xchild(d, root, "expression"); e >= 0; e = xnext(d, e, "expression"), i++) {
    at3_expression *x = &(*out)[i];
    long vals = xchild(d, e, "values"), v;
    size_t k = 0;
    xstr(d, e, "name", x->name, sizeof x->name);
    x->is_arpeggio = xis(d, xchild(d, e, "isArpeggio"), "true");
    x->speed = xint(d, e, "speed", 0);
    x->shift = xint(d, e, "shift", 0);
    x->loop_start = xint(d, e, "loopStartIndex", 0);
    x->end = xint(d, e, "endIndex", 0);
    x->n_values = xcount(d, vals, "value");
    if (x->n_values == 0) continue;
    x->values = (int *)calloc(x->n_values, sizeof(int));
    NEED(x->values);
    for (v = xchild(d, vals, "value"); v >= 0; v = xnext(d, v, "value"), k++) {
      const char *s = d->n[v].text ? d->n[v].text : "";
      int neg = (d->n[v].tlen && s[0] == '-');
      size_t j = neg ? 1 : 0;
      int val = 0;
      for (; j < d->n[v].tlen && s[j] >= '0' && s[j] <= '9'; j++) val = val * 10 + (s[j] - '0');
      x->values[k] = neg ? -val : val;
    }
  }
  return 0;
}

static int read_tracks(const xdoc *d, long sub, at3_subsong *ss) {
  long tracks = xchild(d, sub, "tracks"), t;
  size_t i = 0;
  ss->n_tracks = xcount(d, tracks, "track");
  if (ss->n_tracks) {
    ss->tracks = (at3_track *)calloc(ss->n_tracks, sizeof *ss->tracks);
    NEED(ss->tracks);
  }
  for (t = xchild(d, tracks, "track"); t >= 0; t = xnext(d, t, "track"), i++) {
    at3_track *tr = &ss->tracks[i];
    long c;
    size_t k = 0;
    tr->index = xint(d, t, "index", -1);
    tr->n_cells = xcount(d, t, "cell");
    if (!tr->n_cells) continue;
    tr->cells = (at3_cell *)calloc(tr->n_cells, sizeof *tr->cells);
    NEED(tr->cells);
    for (c = xchild(d, t, "cell"); c >= 0; c = xnext(d, c, "cell"), k++) {
      at3_cell *ce = &tr->cells[k];
      long e;
      size_t m = 0;
      ce->index = xint(d, c, "index", -1);
      ce->note = xint(d, c, "note", -1);
      ce->instrument = xint(d, c, "instrument", -1);
      ce->n_effects = xcount(d, c, "effect");
      if (!ce->n_effects) continue;
      ce->effects = (at3_effect *)calloc(ce->n_effects, sizeof *ce->effects);
      NEED(ce->effects);
      for (e = xchild(d, c, "effect"); e >= 0; e = xnext(d, e, "effect"), m++) {
        xstr(d, e, "name", ce->effects[m].name, sizeof ce->effects[m].name);
        ce->effects[m].index = xint(d, e, "index", 0);
        ce->effects[m].value = xint(d, e, "logicalValue", 0);
      }
    }
  }
  return 0;
}

static int read_special(const xdoc *d, long sub, const char *group, const char *item, at3_special_track **out, size_t *count) {
  long grp = xchild(d, sub, group), t;
  size_t i = 0;
  *count = xcount(d, grp, item);
  if (!*count) return 0;
  *out = (at3_special_track *)calloc(*count, sizeof **out);
  NEED(*out);
  for (t = xchild(d, grp, item); t >= 0; t = xnext(d, t, item), i++) {
    at3_special_track *st = &(*out)[i];
    long c;
    size_t k = 0;
    st->index = xint(d, t, "index", -1);
    st->n_cells = xcount(d, t, "cell");
    if (!st->n_cells) continue;
    st->cells = (at3_special_cell *)calloc(st->n_cells, sizeof *st->cells);
    NEED(st->cells);
    for (c = xchild(d, t, "cell"); c >= 0; c = xnext(d, c, "cell"), k++) {
      st->cells[k].index = xint(d, c, "index", -1);
      st->cells[k].value = xint(d, c, "value", 0);
    }
  }
  return 0;
}

static int read_subsong(const xdoc *d, long sub, at3_subsong *ss) {
  long psgs = xchild(d, sub, "psgs"), p, pos, pats;
  size_t i = 0;
  xstr(d, sub, "title", ss->title, sizeof ss->title);
  ss->initial_speed = xint(d, sub, "initialSpeed", 6);
  ss->digi_channel = xint(d, sub, "digiChannel", 0);
  ss->loop_start_position = xint(d, sub, "loopStartPosition", 0);
  ss->end_position = xint(d, sub, "endPosition", 0);
  ss->replay_milli_hz = xmilli(d, sub, "replayFrequencyHz", 50000);

  ss->n_psgs = xcount(d, psgs, "psg");
  if (ss->n_psgs) {
    ss->psgs = (at3_psg *)calloc(ss->n_psgs, sizeof *ss->psgs);
    NEED(ss->psgs);
  }
  for (p = xchild(d, psgs, "psg"); p >= 0; p = xnext(d, p, "psg"), i++) {
    at3_psg *g = &ss->psgs[i];
    g->type_ym = xis(d, xchild(d, p, "type"), "ym");
    g->frequency_hz = xint(d, p, "frequencyHz", 1000000);
    g->reference_milli_hz = xmilli(d, p, "referenceFrequencyHz", 440000);
    g->sample_player_hz = xint(d, p, "samplePlayerFrequencyHz", 11025);
    xstr(d, p, "mixingOutput", g->mixing, sizeof g->mixing);
  }
  if (read_tracks(d, sub, ss)) return -1;
  if (read_special(d, sub, "speedTracks", "speedTrack", &ss->speed_tracks, &ss->n_speed_tracks)) return -1;
  if (read_special(d, sub, "eventTracks", "eventTrack", &ss->event_tracks, &ss->n_event_tracks)) return -1;

  pos = xchild(d, sub, "positions");
  ss->n_positions = xcount(d, pos, "position");
  if (ss->n_positions) {
    ss->positions = (at3_position *)calloc(ss->n_positions, sizeof *ss->positions);
    NEED(ss->positions);
  }
  i = 0;
  for (p = xchild(d, pos, "position"); p >= 0; p = xnext(d, p, "position"), i++) {
    at3_position *ps = &ss->positions[i];
    long trs = xchild(d, p, "transpositions"), t;
    size_t k = 0;
    ps->pattern_index = xint(d, p, "patternIndex", 0);
    ps->height = xint(d, p, "height", 64);
    ps->n_transpositions = xcount(d, trs, "transposition");
    if (!ps->n_transpositions) continue;
    ps->transpositions = (int *)calloc(ps->n_transpositions * 2, sizeof(int));
    NEED(ps->transpositions);
    for (t = xchild(d, trs, "transposition"); t >= 0; t = xnext(d, t, "transposition"), k++) {
      ps->transpositions[k * 2] = xint(d, t, "channel", 0);
      ps->transpositions[k * 2 + 1] = xint(d, t, "value", 0);
    }
  }

  pats = xchild(d, sub, "patterns");
  ss->n_patterns = xcount(d, pats, "pattern");
  if (ss->n_patterns) {
    ss->patterns = (at3_pattern *)calloc(ss->n_patterns, sizeof *ss->patterns);
    NEED(ss->patterns);
  }
  i = 0;
  for (p = xchild(d, pats, "pattern"); p >= 0; p = xnext(d, p, "pattern"), i++) {
    at3_pattern *pt = &ss->patterns[i];
    long t;
    size_t k = 0;
    pt->n_channels = xcount(d, p, "trackIndexes");
    pt->speed_track_index = xint(d, xchild(d, p, "speedTrackIndex"), "trackIndex", 0);
    pt->event_track_index = xint(d, xchild(d, p, "eventTrackIndex"), "trackIndex", 0);
    if (!pt->n_channels) continue;
    pt->track_index = (int *)calloc(pt->n_channels, sizeof(int));
    NEED(pt->track_index);
    for (t = xchild(d, p, "trackIndexes"); t >= 0; t = xnext(d, t, "trackIndexes"), k++)
      pt->track_index[k] = xint(d, t, "trackIndex", 0);
  }
  return 0;
}

void at3_song_free(at3_song *s) {
  size_t i, j, k;
  if (!s) return;
  for (i = 0; i < s->n_instruments; i++) free(s->instruments[i].cells);
  free(s->instruments);
  for (i = 0; i < s->n_arpeggios; i++) free(s->arpeggios[i].values);
  free(s->arpeggios);
  for (i = 0; i < s->n_pitches; i++) free(s->pitches[i].values);
  free(s->pitches);
  for (i = 0; i < s->n_subsongs; i++) {
    at3_subsong *ss = &s->subsongs[i];
    for (j = 0; j < ss->n_tracks; j++) {
      for (k = 0; k < ss->tracks[j].n_cells; k++) free(ss->tracks[j].cells[k].effects);
      free(ss->tracks[j].cells);
    }
    free(ss->tracks);
    for (j = 0; j < ss->n_speed_tracks; j++) free(ss->speed_tracks[j].cells);
    free(ss->speed_tracks);
    for (j = 0; j < ss->n_event_tracks; j++) free(ss->event_tracks[j].cells);
    free(ss->event_tracks);
    for (j = 0; j < ss->n_positions; j++) free(ss->positions[j].transpositions);
    free(ss->positions);
    for (j = 0; j < ss->n_patterns; j++) free(ss->patterns[j].track_index);
    free(ss->patterns);
    free(ss->psgs);
  }
  free(s->subsongs);
  free(s->title);
  free(s->author);
  free(s->composer);
  free(s->comment);
  free(s);
}

static void fail(char *err, size_t n, const char *msg) {
  if (err && n) {
    strncpy(err, msg, n - 1);
    err[n - 1] = 0;
  }
}

/* Unpacks the first member of a zip image; *owned receives the heap block. */
static int unpack(const void *data, size_t size, char **owned, size_t *osize, char *err, size_t en) {
  mz_zip_archive z;
  mz_uint i, n;
  memset(&z, 0, sizeof z);
  if (!mz_zip_reader_init_mem(&z, data, size, 0)) { fail(err, en, "not a valid zip archive"); return -1; }
  n = mz_zip_reader_get_num_files(&z);
  for (i = 0; i < n; i++) {
    if (mz_zip_reader_is_file_a_directory(&z, i)) continue;
    *owned = (char *)mz_zip_reader_extract_to_heap(&z, i, osize, 0);
    mz_zip_reader_end(&z);
    if (!*owned) { fail(err, en, "cannot extract the song member (corrupt or too large)"); return -1; }
    return 0;
  }
  mz_zip_reader_end(&z);
  fail(err, en, "zip archive holds no file");
  return -1;
}

int at3_song_load(const void *data, size_t size, at3_song **out, char *err, size_t err_size) {
  const char *xml = (const char *)data;
  size_t xsize = size;
  char *owned = NULL;
  xdoc d;
  long root, c, ins, subs;
  const char *why = "";
  at3_song *s = NULL;
  size_t i;

  *out = NULL;
  memset(&d, 0, sizeof d);
  if (!data || size < 8) { fail(err, err_size, "file too short"); return -1; }
  if (xml[0] == 'P' && xml[1] == 'K') {
    if (unpack(data, size, &owned, &xsize, err, err_size)) return -1;
    xml = owned;
  }
  if (xparse(&d, xml, xsize, &root, &why)) {
    char m[96];
    snprintf(m, sizeof m, "not an AT3 song: %s", why);
    fail(err, err_size, m);
    goto bad;
  }
  if (d.n[root].nlen != 4 || memcmp(d.n[root].name, "song", 4) != 0) { fail(err, err_size, "root element is not <song>"); goto bad; }
  c = xchild(&d, root, "formatVersion");
  if (!xis(&d, c, "3.0")) { fail(err, err_size, "unsupported or missing formatVersion (want 3.0)"); goto bad; }
  subs = xchild(&d, root, "subsongs");
  if (subs < 0 || xcount(&d, subs, "subsong") == 0) { fail(err, err_size, "song has no subsongs"); goto bad; }

  s = (at3_song *)calloc(1, sizeof *s);
  if (!s) { fail(err, err_size, "out of memory"); goto bad; }
  s->title = xdup(&d, root, "title");
  s->author = xdup(&d, root, "author");
  s->composer = xdup(&d, root, "composer");
  s->comment = xdup(&d, root, "comment");
  if (!s->title || !s->author || !s->composer || !s->comment) goto oom;

  ins = xchild(&d, root, "instruments");
  s->n_instruments = xcount(&d, ins, "instrument");
  if (s->n_instruments) {
    s->instruments = (at3_instrument *)calloc(s->n_instruments, sizeof *s->instruments);
    if (!s->instruments) goto oom;
  }
  for (c = xchild(&d, ins, "instrument"), i = 0; c >= 0; c = xnext(&d, c, "instrument"), i++)
    if (read_instrument(&d, c, &s->instruments[i])) goto oom;
  if (read_expressions(&d, xchild(&d, root, "arpeggios"), &s->arpeggios, &s->n_arpeggios)) goto oom;
  if (read_expressions(&d, xchild(&d, root, "pitches"), &s->pitches, &s->n_pitches)) goto oom;

  s->n_subsongs = xcount(&d, subs, "subsong");
  s->subsongs = (at3_subsong *)calloc(s->n_subsongs, sizeof *s->subsongs);
  if (!s->subsongs) goto oom;
  for (c = xchild(&d, subs, "subsong"), i = 0; c >= 0; c = xnext(&d, c, "subsong"), i++)
    if (read_subsong(&d, c, &s->subsongs[i])) goto oom;

  free(d.n);
  free(owned);
  *out = s;
  return 0;
oom:
  fail(err, err_size, "out of memory");
bad:
  at3_song_free(s);
  free(d.n);
  free(owned);
  return -1;
}
