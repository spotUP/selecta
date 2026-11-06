// SPDX-License-Identifier: GPL-3.0-or-later
// Selecta glue for bbmp's csrc/t0ast engine (I710ag): includes the unchanged engine file (its main() renamed away) and renders a whole song.
#define main bbmp_t0ast_unused_main
#include "bbmp-t0ast.cpp"
#undef main

#include <cstdio>
#include <cstdlib>

extern "C" {
// check_only: load and measure only. Otherwise renders the whole song at `rate` into a malloc'ed buffer (stereo s16). duration_ms may be NULL.
int bbt0ast_render(const uint8_t* data, size_t size, uint32_t rate, int check_only, int16_t** pcm, uint64_t* frames, long* duration_ms, char* err,
                   size_t errn, char* title, size_t titlen) {
  if (size == 0 || size > MAX_FILE) {
    std::snprintf(err, errn, "the file is too big for a T0AST song");
    return 0;
  }
  constants();
  std::vector<uint8_t> file(data, data + size);
  Song song;
  const std::string why = load(file, song);
  if (!why.empty()) {
    std::snprintf(err, errn, "%s", why.c_str());
    return 0;
  }
  const long long total = measure(song);
  if (total < 0) {
    std::snprintf(err, errn, "the song does not end within 30 minutes");
    return 0;
  }
  if (duration_ms != nullptr) *duration_ms = (long)(total * 1000 / RATE);
  if (title != nullptr && titlen > 0) std::snprintf(title, titlen, "%s", trim(latin1(song.title)).c_str());
  if (check_only) return 1;
  if (rate < 8000 || rate > 192000) rate = RATE;
  Player player{makeEngine(song, true), total};
  const unsigned long long n_total = rate == (uint32_t)RATE ? (unsigned long long)total : (unsigned long long)total * rate / RATE;
  int16_t* out = (int16_t*)std::malloc((size_t)n_total * 4u + 4u);
  if (out == nullptr) {
    delete player.engine;
    std::snprintf(err, errn, "out of memory");
    return 0;
  }
  if (rate == (uint32_t)RATE) {
    for (unsigned long long i = 0; i < n_total; ++i) player.frame(&out[2 * i]);
  } else {
    Resampler rs(RATE, (int)rate, source, &player);
    for (unsigned long long i = 0; i < n_total; ++i) rs.frame(&out[2 * i]);
  }
  delete player.engine;
  *pcm = out;
  *frames = n_total;
  return 1;
}
}
