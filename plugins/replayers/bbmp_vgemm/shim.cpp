// bbmp_vgemm shim (row I710ah): C entry points over bbmp's unchanged csrc/retro/vgemm_engine.cpp (GPL-3.0-or-later). The engine registers
// itself with REGISTER_ENGINE; the helpers of namespace retro it uses (register_engine, field) are bbmp-retro.cpp's, restated
// here because that file is the helper's main program.
#include "engine.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>

namespace retro {

EngineMaker g_maker = nullptr;
void register_engine(const char*, EngineMaker maker) { g_maker = maker; }

std::string from_latin1(const std::string& s) {
  std::string out;
  for (unsigned char c : s) {
    if (c < 0x80) {
      out += static_cast<char>(c);
    } else {
      out += static_cast<char>(0xc0 | c >> 6);
      out += static_cast<char>(0x80 | (c & 0x3f));
    }
  }
  return out;
}

// bbmp-retro's to_utf8 keeps valid UTF-8 and otherwise reads Shift-JIS, then Latin-1; here the fallback is Latin-1 only (song texts of
// VGM Music Maker are Windows ANSI), which only differs for Shift-JIS text.
static bool valid_utf8(const std::string& s) {
  size_t i = 0;
  while (i < s.size()) {
    const unsigned char c = static_cast<unsigned char>(s[i]);
    const size_t n = c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : (c >> 3) == 30 ? 4 : 0;
    if (n == 0 || i + n > s.size()) return false;
    for (size_t k = 1; k < n; k++)
      if ((static_cast<unsigned char>(s[i + k]) & 0xc0) != 0x80) return false;
    i += n;
  }
  return true;
}

std::string to_utf8(const std::string& s) { return valid_utf8(s) ? s : from_latin1(s); }

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

struct vgemm {
  std::vector<uint8_t> file;
  std::unique_ptr<retro::Engine> eng;
  retro::SongInfo info;
  uint32_t rate = 0;
};

// Opens the song at `rate`; NULL when the engine refuses the file (magic, version, sizes, tables, libopenmpt).
struct vgemm* vgemm_open(const uint8_t* data, uint32_t size, uint32_t rate) {
  if (retro::g_maker == nullptr) return nullptr;
  std::unique_ptr<vgemm> o(new vgemm());
  o->file.assign(data, data + size);
  o->eng.reset(retro::g_maker());
  std::string err;
  if (!o->eng->open(o->file, "", 0, static_cast<int>(rate), o->info, err)) {
    if (retro::debug_on()) std::fprintf(stderr, "vgemm: %s\n", err.c_str());
    return nullptr;
  }
  o->rate = rate;
  return o.release();
}

uint32_t vgemm_duration_ms(const struct vgemm* o) { return static_cast<uint32_t>(o->info.duration_ms); }
const char* vgemm_title(const struct vgemm* o) { return o->info.title.c_str(); }
const char* vgemm_type(const struct vgemm* o) { return o->info.type.c_str(); }

// Renders the whole song; *pcm is malloc'ed stereo int16, *frames its length. Returns 0 on failure.
int vgemm_render(struct vgemm* o, int16_t** pcm, uint64_t* frames) {
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

void vgemm_close(struct vgemm* o) { delete o; }
}
