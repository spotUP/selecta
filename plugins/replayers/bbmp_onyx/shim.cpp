// bbmp_onyx shim (row I710r): C entry points over bbmp's unchanged csrc/retro/jangle_engine.cpp (GPL-3.0-or-later). The engine registers
// itself with REGISTER_ENGINE; the helpers of namespace retro it uses (register_engine, from_cp437, field) are bbmp-retro.cpp's, restated
// here because that file is the helper's main program.
#include "engine.h"

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

// IBM code page 437 (bbmp-retro.cpp converts with iconv, which is not in every libc; the table is the same mapping).
std::string from_cp437(const std::string& s) {
  static const unsigned short high[128] = {0x00C7, 0x00FC, 0x00E9, 0x00E2, 0x00E4, 0x00E0, 0x00E5, 0x00E7, 0x00EA, 0x00EB, 0x00E8, 0x00EF, 0x00EE, 0x00EC, 0x00C4, 0x00C5, 0x00C9, 0x00E6, 0x00C6, 0x00F4, 0x00F6, 0x00F2, 0x00FB, 0x00F9, 0x00FF, 0x00D6, 0x00DC, 0x00A2, 0x00A3, 0x00A5, 0x20A7, 0x0192, 0x00E1, 0x00ED, 0x00F3, 0x00FA, 0x00F1, 0x00D1, 0x00AA, 0x00BA, 0x00BF, 0x2310, 0x00AC, 0x00BD, 0x00BC, 0x00A1, 0x00AB, 0x00BB, 0x2591, 0x2592, 0x2593, 0x2502, 0x2524, 0x2561, 0x2562, 0x2556, 0x2555, 0x2563, 0x2551, 0x2557, 0x255D, 0x255C, 0x255B, 0x2510, 0x2514, 0x2534, 0x252C, 0x251C, 0x2500, 0x253C, 0x255E, 0x255F, 0x255A, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256C, 0x2567, 0x2568, 0x2564, 0x2565, 0x2559, 0x2558, 0x2552, 0x2553, 0x256B, 0x256A, 0x2518, 0x250C, 0x2588, 0x2584, 0x258C, 0x2590, 0x2580, 0x03B1, 0x00DF, 0x0393, 0x03C0, 0x03A3, 0x03C3, 0x00B5, 0x03C4, 0x03A6, 0x0398, 0x03A9, 0x03B4, 0x221E, 0x03C6, 0x03B5, 0x2229, 0x2261, 0x00B1, 0x2265, 0x2264, 0x2320, 0x2321, 0x00F7, 0x2248, 0x00B0, 0x2219, 0x00B7, 0x221A, 0x207F, 0x00B2, 0x25A0, 0x00A0};
  std::string out;
  for (unsigned char c : s) {
    unsigned cp = c < 0x80 ? c : high[c - 0x80];
    if (cp < 0x80) {
      out += static_cast<char>(cp);
    } else if (cp < 0x800) {
      out += static_cast<char>(0xc0 | cp >> 6);
      out += static_cast<char>(0x80 | (cp & 0x3f));
    } else {
      out += static_cast<char>(0xe0 | cp >> 12);
      out += static_cast<char>(0x80 | ((cp >> 6) & 0x3f));
      out += static_cast<char>(0x80 | (cp & 0x3f));
    }
  }
  return out;
}

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

struct onyx {
  std::vector<uint8_t> file;
  std::unique_ptr<retro::Engine> eng;
  retro::SongInfo info;
  uint32_t rate = 0;
};

// Opens the song at `rate`; NULL when the engine refuses the file (magic, version, sizes, tables, libopenmpt).
struct onyx* onyx_open(const uint8_t* data, uint32_t size, uint32_t rate) {
  if (retro::g_maker == nullptr) return nullptr;
  std::unique_ptr<onyx> o(new onyx());
  o->file.assign(data, data + size);
  o->eng.reset(retro::g_maker());
  std::string err;
  if (!o->eng->open(o->file, "", 0, static_cast<int>(rate), o->info, err)) return nullptr;
  o->rate = rate;
  return o.release();
}

uint32_t onyx_duration_ms(const struct onyx* o) { return static_cast<uint32_t>(o->info.duration_ms); }
const char* onyx_title(const struct onyx* o) { return o->info.title.c_str(); }
const char* onyx_type(const struct onyx* o) { return o->info.type.c_str(); }

// Renders the whole song; *pcm is malloc'ed stereo int16, *frames its length. Returns 0 on failure.
int onyx_render(struct onyx* o, int16_t** pcm, uint64_t* frames) {
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

void onyx_close(struct onyx* o) { delete o; }
}
