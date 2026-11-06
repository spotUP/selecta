// SPDX-License-Identifier: GPL-3.0-or-later
// Selecta glue for bbmp's BoyScout engine (row I710c): provides what bbmp-retro's main program gives an engine (the registry, debug_on)
// and renders the whole song through the engine's own open()/render(), the loop bbmp-retro's main runs.
#include "boyscout_engine.cpp"

namespace retro {
static EngineMaker g_maker;  // constant-initialised, so the engine's static registrar may run before this file's own dynamic init
void register_engine(const char*, EngineMaker maker) { g_maker = maker; }
bool debug_on() { return false; }
}  // namespace retro

extern "C" {
// Renders the whole song at `rate`. Returns 1 and a malloc'd s16 stereo buffer, or 0 with `err` set. check_only: open() only.
int bbboyscout_render(const uint8_t* data, size_t size, uint32_t rate, int check_only, int16_t** pcm, uint64_t* frames, long* duration_ms,
                     char* err, size_t errn) {
  if (retro::g_maker == nullptr) {
    std::snprintf(err, errn, "engine not registered");
    return 0;
  }
  std::unique_ptr<retro::Engine> engine(retro::g_maker());
  std::vector<uint8_t> file(data, data + size);
  retro::SongInfo info;
  std::string why;
  if (!engine->open(file, "", 0, (int)rate, info, why)) {
    std::snprintf(err, errn, "%s", why.c_str());
    return 0;
  }
  if (duration_ms != nullptr) *duration_ms = (long)info.duration_ms;
  if (check_only) return 1;
  std::vector<int16_t> all;
  std::vector<int16_t> buf(2 * 4096);
  const size_t limit = (size_t)3600 * 3 * rate;
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
