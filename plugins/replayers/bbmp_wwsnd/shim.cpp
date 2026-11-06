// bbmp_wwsnd shim (row I710aj): C entry points over bbmp's unchanged csrc/retro/wwsnd_engine.cpp (GPL-3.0-or-later). The engine registers
// itself with REGISTER_ENGINE; the helpers of namespace retro it uses (register_engine, field, debug_on) are bbmp-retro.cpp's, restated
// here because that file is the helper's main program.
#include "engine.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>

namespace retro {

EngineMaker g_maker = nullptr;
void register_engine(const char*, EngineMaker maker) { g_maker = maker; }

std::string field(const void* p, size_t size) {
  const char* c = static_cast<const char*>(p);
  size_t n = 0;
  while (n < size && c[n]) n++;
  while (n > 0 && (c[n - 1] == ' ' || c[n - 1] == '\t' || c[n - 1] == '\r' || c[n - 1] == '\n')) n--;
  return std::string(c, n);
}

bool debug_on() { return std::getenv("BBMP_RETRO_DEBUG") != nullptr; }

}  // namespace retro

extern "C" {

struct wwsong {
  std::vector<uint8_t> file;
  std::unique_ptr<retro::Engine> eng;
  retro::SongInfo info;
  uint32_t rate = 0;
};

// Opens track `track` of the song at `rate`; NULL when the engine refuses the file (magic, version, size, track).
struct wwsong* wwsong_open(const uint8_t* data, uint32_t size, uint32_t track, uint32_t rate) {
  if (retro::g_maker == nullptr) return nullptr;
  std::unique_ptr<wwsong> o(new wwsong());
  o->file.assign(data, data + size);
  o->eng.reset(retro::g_maker());
  std::string err;
  if (!o->eng->open(o->file, "", static_cast<int>(track), static_cast<int>(rate), o->info, err)) {
    if (retro::debug_on()) std::fprintf(stderr, "wwsnd: %s\n", err.c_str());
    return nullptr;
  }
  o->rate = rate;
  return o.release();
}

uint32_t wwsong_tracks(const struct wwsong* o) { return static_cast<uint32_t>(o->info.tracks); }
uint32_t wwsong_duration_ms(const struct wwsong* o) { return static_cast<uint32_t>(o->info.duration_ms); }
const char* wwsong_title(const struct wwsong* o) { return o->info.title.c_str(); }
const char* wwsong_author(const struct wwsong* o) { return o->info.author.c_str(); }
const char* wwsong_type(const struct wwsong* o) { return o->info.type.c_str(); }

// Renders the whole track; *pcm is malloc'ed stereo int16, *frames its length. Returns 0 on failure.
int wwsong_render(struct wwsong* o, int16_t** pcm, uint64_t* frames) {
  std::vector<int16_t> out;
  std::vector<int16_t> buf(4096 * 2);
  const uint64_t most = static_cast<uint64_t>(o->rate) * 3600;
  for (;;) {
    const size_t got = o->eng->render(buf.data(), 4096);
    out.insert(out.end(), buf.begin(), buf.begin() + got * 2);
    if (got < 4096 || out.size() / 2 > most) break;
  }
  *frames = out.size() / 2;
  *pcm = static_cast<int16_t*>(std::malloc(out.size() * sizeof(int16_t) + 4));
  if (*pcm == nullptr) return 0;
  std::memcpy(*pcm, out.data(), out.size() * sizeof(int16_t));
  return 1;
}

void wwsong_close(struct wwsong* o) { delete o; }
}
