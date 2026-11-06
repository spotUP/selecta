// SPDX-License-Identifier: BSD-3-Clause
// Selecta glue for bbmp's TSS engine (row I710af): the registry hook, to_utf8 and debug_on the engine takes from bbmp-retro's main program, and
// a C entry point that renders the song through the engine's own open()/render().
#include "tss_engine.cpp"

#include <memory>

namespace retro {
static EngineMaker g_maker;  // constant-initialised, so the engine's static registrar may run before this file's own dynamic init
void register_engine(const char*, EngineMaker maker) { g_maker = maker; }
bool debug_on() { return false; }

static bool valid_utf8(const std::string& s) {
  size_t i = 0;
  while (i < s.size()) {
    const unsigned char c = static_cast<unsigned char>(s[i]);
    size_t n = c < 0x80 ? 0 : (c >> 5) == 6 ? 1 : (c >> 4) == 14 ? 2 : (c >> 3) == 30 ? 3 : 99;
    if (n == 99 || i + n >= s.size()) return false;
    for (size_t k = 1; k <= n; k++)
      if ((static_cast<unsigned char>(s[i + k]) & 0xc0) != 0x80) return false;
    i += n + 1;
  }
  return true;
}

std::string to_utf8(const std::string& s) {
  if (valid_utf8(s)) return s;
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
}  // namespace retro

extern "C" {
// Renders the song at `rate`. check_only: open() only (the song compiles and has notes). Title (UTF-8) is copied to `title` when given.
int bbtss_render(const uint8_t* data, size_t size, uint32_t rate, int check_only, int16_t** pcm, uint64_t* frames, long* duration_ms,
                 char* err, size_t errn, char* title, size_t titlen) {
  if (retro::g_maker == nullptr) {
    std::snprintf(err, errn, "engine not registered");
    return 0;
  }
  std::unique_ptr<retro::Engine> engine(retro::g_maker());
  std::vector<uint8_t> file(data, data + size);
  retro::SongInfo info;
  std::string why;
  try {
    if (!engine->open(file, "", 0, (int)rate, info, why)) {
      std::snprintf(err, errn, "%s", why.c_str());
      return 0;
    }
  } catch (const std::exception& e) {
    std::snprintf(err, errn, "%s", e.what());
    return 0;
  }
  if (duration_ms != nullptr) *duration_ms = (long)info.duration_ms;
  if (title != nullptr && titlen > 0) std::snprintf(title, titlen, "%s", info.title.c_str());
  if (check_only) return 1;
  std::vector<int16_t> all;
  std::vector<int16_t> buf(2 * 4096);
  const size_t limit = (size_t)3600 * rate;
  for (;;) {
    const size_t got = engine->render(buf.data(), 4096);
    all.insert(all.end(), buf.begin(), buf.begin() + 2 * got);
    if (got < 4096 || all.size() / 2 >= limit) break;
  }
  int16_t* out = (int16_t*)std::malloc(all.size() * sizeof(int16_t) + 4u);
  if (out == nullptr) {
    std::snprintf(err, errn, "out of memory");
    return 0;
  }
  std::memcpy(out, all.data(), all.size() * sizeof(int16_t));
  *pcm = out;
  *frames = all.size() / 2;
  return 1;
}
}
