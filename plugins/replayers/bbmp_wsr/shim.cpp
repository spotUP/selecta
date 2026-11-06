// bbmp_wsr shim (row I710z): C entry points over bbmp's csrc/console/wsr_engine (unchanged). The engine finds the song numbers
// (forked scan workers), the loop or end of each, and plays it once with a fade; this renders one track into memory.
// engine.h also wants to_utf8 and debug_on, which live in bbmp-console.cpp (the stdio main); they are provided here.
#include "engine.h"

#include <cstdlib>
#include <cstring>
#include <memory>

bool debug_on() {
  static const bool on = std::getenv("BBMP_CONSOLE_DEBUG") != nullptr;
  return on;
}

// Titles are not used (a .wsr has none); the engine only fills the comment.
std::string to_utf8(const std::string& text) { return text; }

Engine* make_spu_engine() { return nullptr; }

extern "C" {

struct wsrr {
  std::unique_ptr<Engine> engine;  // the engine opened for track 0 at kScanRate, kept for the first start
  int tracks = 0;
  uint32_t rate = 0;
  std::string comment;
};

static const uint32_t kScanRate = 44100;

int wsrr_probe(const uint8_t* data, uint32_t size) {
  return size >= 0x10000u && (size & 0xFFFFu) == 0 && size <= (64u << 20) && memcmp(data + size - 0x20, "WSRF", 4) == 0;
}

static std::vector<uint8_t> g_file;

struct wsrr* wsrr_open(const uint8_t* data, uint32_t size) {
  if (!wsrr_probe(data, size)) return nullptr;
  g_file.assign(data, data + size);
  wsrr* r = new wsrr();
  SongInfo info;
  std::string err;
  r->engine.reset(make_wsr_engine());
  if (!r->engine->open(g_file, 0, (int)kScanRate, info, err)) {
    delete r;
    return nullptr;
  }
  r->tracks = info.tracks;
  r->rate = kScanRate;
  r->comment = info.comment;
  return r;
}

int wsrr_tracks(const struct wsrr* r) { return r->tracks; }
const char* wsrr_comment(const struct wsrr* r) { return r->comment.c_str(); }

/* Renders track `track` at `rate`: *pcm is malloc'ed stereo int16, *frames its length. 0 on failure. The emulator state is
 * global, so only one engine is alive at a time. */
int wsrr_render(struct wsrr* r, int track, uint32_t rate, int16_t** pcm, uint64_t* frames, uint32_t* ms) {
  if (track < 0 || track >= r->tracks) return 0;
  if (!(r->engine && track == 0 && rate == r->rate)) {
    SongInfo info;
    std::string err;
    r->engine.reset();
    r->engine.reset(make_wsr_engine());
    if (!r->engine->open(g_file, track, (int)rate, info, err)) return 0;
    r->comment = info.comment;
  }
  std::vector<int16_t> all;
  std::vector<int16_t> buf(4096 * 2);
  for (;;) {
    size_t n = r->engine->render(buf.data(), 4096);
    all.insert(all.end(), buf.begin(), buf.begin() + n * 2);
    if (n < 4096) break;
  }
  r->engine.reset();  // consumed; the next start opens again
  r->rate = 0;
  size_t n = all.size() / 2;
  int16_t* out = (int16_t*)malloc((n ? n : 1) * 4u);
  if (out == nullptr) return 0;
  memcpy(out, all.data(), n * 4u);
  *pcm = out;
  *frames = n;
  *ms = (uint32_t)(n * 1000u / rate);
  return n > 0;
}

void wsrr_close(struct wsrr* r) { delete r; }
}
