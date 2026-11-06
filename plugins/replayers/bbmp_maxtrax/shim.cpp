// bbmp_maxtrax shim (row I710k): C entry points over bbmp's csrc/amiga/maxtrax_engine (unchanged; ScummVM's MaxTrax player
// and Paula, GPL 3 or later, in third_party/bbmp_maxtrax). bbmp-amiga.cpp (the stdio main) is not taken; the engine registry
// and the two helper functions engine.h declares are provided here. The engine measures the song's length itself (a dry run at a
// low rate), then this renders one score into memory.
#include "engine.h"

#include <cstdlib>
#include <cstring>
#include <memory>

static EngineEntry* g_engines = nullptr;

EngineEntry::EngineEntry(const char* n, Engine* (*m)()) : name(n), make(m), next(g_engines) { g_engines = this; }

Engine* make_engine(const std::string& name) {
  for (EngineEntry* e = g_engines; e != nullptr; e = e->next)
    if (name == e->name) return e->make();
  return nullptr;
}

std::string to_utf8(const std::string& text) { return text; }

bool debug_on() {
  static const bool on = std::getenv("BBMP_AMIGA_DEBUG") != nullptr;
  return on;
}

extern "C" {

struct mxtxr {
  std::vector<uint8_t> file;
  std::string path, comment;
  int tracks = 0;
  uint32_t ms = 0;
};

int mxtxr_probe(const uint8_t* d, uint32_t n) { return n >= 10 && std::memcmp(d, "MXTX", 4) == 0; }

struct mxtxr* mxtxr_open(const uint8_t* data, uint32_t size, const char* path) {
  if (!mxtxr_probe(data, size)) return nullptr;
  std::unique_ptr<mxtxr> r(new mxtxr());
  r->file.assign(data, data + size);
  r->path = path != nullptr ? path : "";
  std::unique_ptr<Engine> e(make_engine("maxtrax"));
  SongInfo info;
  std::string err;
  if (!e || !e->open(r->path, r->file, 0, 44100, info, err)) return nullptr;
  r->tracks = info.tracks;
  r->comment = info.comment;
  return r.release();
}

int mxtxr_tracks(const struct mxtxr* r) { return r->tracks; }
const char* mxtxr_comment(const struct mxtxr* r) { return r->comment.c_str(); }
uint32_t mxtxr_duration_ms(const struct mxtxr* r) { return r->ms; }

/* Renders score `track` at `rate`: *pcm is malloc'ed stereo int16, *frames its length (at most 15 minutes). 0 on failure. */
int mxtxr_render(struct mxtxr* r, int track, uint32_t rate, int16_t** pcm, uint64_t* frames) {
  if (track < 0 || track >= r->tracks) return 0;
  std::unique_ptr<Engine> e(make_engine("maxtrax"));
  SongInfo info;
  std::string err;
  if (!e || !e->open(r->path, r->file, track, (int)rate, info, err)) return 0;
  r->ms = (uint32_t)info.duration_ms;
  const uint64_t cap = (uint64_t)rate * 900u;
  std::vector<int16_t> all, buf(4096 * 2);
  while (all.size() / 2 < cap) {
    size_t n = e->render(buf.data(), 4096);
    all.insert(all.end(), buf.begin(), buf.begin() + n * 2);
    if (n < 4096) break;
  }
  size_t n = all.size() / 2;
  int16_t* out = (int16_t*)malloc((n ? n : 1) * 4u);
  if (out == nullptr) return 0;
  memcpy(out, all.data(), n * 4u);
  *pcm = out;
  *frames = n;
  return n > 0;
}

void mxtxr_close(struct mxtxr* r) { delete r; }
}
