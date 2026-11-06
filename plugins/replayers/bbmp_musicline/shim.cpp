// bbmp_musicline shim (row I710n): C entry points over the unchanged bbmp csrc/musicline (replay routine LGPL-2.1-or-later,
// Paula mixer GPL-3.0-or-later). The stdio main() of bbmp-musicline.cpp is renamed away; this renders one tune into memory
// with the same Machine, using the same end-of-song logic as that main().
#define main bbmp_musicline_main
#include "bbmp-musicline.cpp"
#undef main

extern "C" {

struct mlr {
  Module mod;
  std::string title, author, type;
  int tracks = 0;
};

int mlr_probe(const uint8_t* data, uint32_t size) {
  if (size < 8u || memcmp(data, "MLEDMODL", 8) != 0) return 0;
  Module m;
  std::string err;
  return m.load(data, size, err);
}

struct mlr* mlr_open(const uint8_t* data, uint32_t size) {
  mlr* r = new mlr();
  std::string err;
  if (!r->mod.load(data, size, err) || r->mod.tunes.empty()) {
    delete r;
    return nullptr;
  }
  r->tracks = (int)r->mod.tunes.size();
  r->author = r->mod.author;
  r->type = "Musicline Editor";
  if (!r->mod.version.empty()) r->type += " " + r->mod.version;
  return r;
}

int mlr_tracks(const struct mlr* r) { return r->tracks; }
const char* mlr_author(const struct mlr* r) { return r->author.c_str(); }
const char* mlr_type(const struct mlr* r) { return r->type.c_str(); }
const char* mlr_title(struct mlr* r, int track) {
  const Tune& tune = r->mod.tunes[track];
  r->title = r->mod.title.empty() ? tune.title : r->mod.title;
  if (r->tracks > 1 && !tune.title.empty() && tune.title != r->title) r->title += " - " + tune.title;
  return r->title.c_str();
}

/* Renders tune `track` at `rate`: *pcm is malloc'ed stereo int16, *frames its length, *ms the song length. 0 on failure. */
int mlr_render(struct mlr* r, int track, uint32_t rate, int16_t** pcm, uint64_t* frames, uint32_t* ms) {
  if (track < 0 || track >= r->tracks) return 0;
  const double pan = 0.4, maxSeconds = 900;
  const uint64_t maxFrames = (uint64_t)(maxSeconds * rate);
  const uint64_t tail = (uint64_t)rate * 6, fade = (uint64_t)rate * 3;
  uint64_t total = 0, fadeFrom = UINT64_MAX;
  {
    Machine m(r->mod, track, (int)rate, pan, false);
    total = m.run(nullptr, maxFrames, true);
    if (!m.endSeen) {
      fadeFrom = total > fade ? total - fade : 0;
    } else if (m.stoppedByItself()) {
      uint64_t end = total;
      while (m.audible() && total - end < tail) total += m.run(nullptr, rate / 100);
      if (total - end >= tail) fadeFrom = total - fade;
    } else {
      uint64_t ms20 = (uint64_t)rate / 50;
      fadeFrom = total > ms20 ? total - ms20 : 0;
    }
  }
  int16_t* buf = (int16_t*)malloc((size_t)(total ? total : 1) * 4u);
  if (buf == nullptr) return 0;
  Machine m(r->mod, track, (int)rate, pan, false);
  uint64_t at = 0;
  while (at < total) {
    size_t n = 4096;
    if (total - at < n) n = (size_t)(total - at);
    int16_t* o = buf + at * 2u;
    m.run(o, n);
    if (fadeFrom != UINT64_MAX && at + n > fadeFrom) {
      for (size_t i = 0; i < n; i++) {
        uint64_t p = at + i;
        if (p < fadeFrom) continue;
        double g = (double)(total - p) / (double)(total - fadeFrom);
        o[2 * i] = (int16_t)lrint(o[2 * i] * g);
        o[2 * i + 1] = (int16_t)lrint(o[2 * i + 1] * g);
      }
    }
    at += n;
  }
  *pcm = buf;
  *frames = total;
  *ms = (uint32_t)(total * 1000u / rate);
  return 1;
}

void mlr_close(struct mlr* r) { delete r; }
}
