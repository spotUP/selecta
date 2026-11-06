// SPDX-License-Identifier: GPL-3.0-or-later
// Selecta glue for bbmp's AXS replayer (row I710a): includes the unchanged helper source with its main() compiled out and exposes the
// whole-song render as one C function. The song plays once as AXS's Winamp plug-in plays it (default, not --clean), at 44100 Hz or
// resampled to `rate` with the helper's own Resampler, exactly the path the helper's main() takes.
#define AXS_NO_MAIN 1
#include "bbmp-axs.cpp"

extern "C" {
// 1 when the engine accepts the file (its load() parses the whole song), else 0.
int bbaxs_check(const uint8_t* data, size_t size) {
  std::vector<uint8_t> file(data, data + size);
  Song song;
  return load(file, song).empty() ? 1 : 0;
}

// Renders the whole song. Returns 1 and a malloc'd s16 stereo buffer (`*frames` frames at `rate`), or 0 with `err` set.
// `*duration_ms` is the song length at the engine's rate. rate 0 means 44100.
int bbaxs_render(const uint8_t* data, size_t size, uint32_t rate, int16_t** pcm, uint64_t* frames, long* duration_ms,
                 char* err, size_t errn) {
  std::vector<uint8_t> file(data, data + size);
  Song song;
  const std::string why = load(file, song);
  if (!why.empty()) {
    std::snprintf(err, errn, "%s", why.c_str());
    return 0;
  }
  if (rate < 8000u || rate > 192000u) rate = RATE;
  bool ends = true;
  const long long total = measure(song, ends);
  if (duration_ms != nullptr) *duration_ms = (long)(total * 1000 / RATE);
  const long long out = rate == (uint32_t)RATE ? total : (long long)((unsigned long long)total * rate / RATE);
  int16_t* buf = (int16_t*)std::malloc((size_t)out * 4u + 4u);
  if (buf == nullptr) {
    std::snprintf(err, errn, "out of memory");
    return 0;
  }
  Engine engine(song, false, false);
  if (rate == (uint32_t)RATE) {
    for (long long i = 0; i < out; ++i) engine.frame(&buf[2 * i]);
  } else {
    Resampler rs(RATE, (int)rate, source, &engine);
    for (long long i = 0; i < out; ++i) rs.frame(&buf[2 * i]);
  }
  *pcm = buf;
  *frames = (uint64_t)out;
  return 1;
}
}
