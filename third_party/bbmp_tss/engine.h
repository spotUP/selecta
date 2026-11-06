// SPDX-License-Identifier: BSD-3-Clause
// Selecta's stand-in for the engine interface of bbmp's csrc/retro/engine.h (which is GPL-3.0-or-later and so not vendored): the few names
// tss_engine.cpp uses, with the same meaning. Written for Selecta, not copied.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace retro {

struct SongInfo {
  int tracks = 1;
  std::string title, author, type, comment;
  long long duration_ms = 0;
  std::vector<std::string> names;
};

class Engine {
 public:
  virtual ~Engine() {}
  virtual bool open(const std::vector<uint8_t>& file, const std::string& path, int track, int rate, SongInfo& info,
                    std::string& error) = 0;
  // Interleaved stereo s16; fewer frames than asked for means the song has ended.
  virtual size_t render(int16_t* out, size_t frames) = 0;
};

typedef Engine* (*EngineMaker)();
void register_engine(const char* name, EngineMaker maker);
struct Registrar {
  Registrar(const char* name, EngineMaker maker) { register_engine(name, maker); }
};
#define REGISTER_ENGINE(name, Class) \
  static retro::Engine* make_##Class() { return new Class; } \
  static retro::Registrar registrar_##Class(name, make_##Class);

// Kept when it is valid UTF-8 already, otherwise read as Latin-1 (Selecta has no Shift-JIS table; the engine only compares the result with
// its input to learn whether the text is not UTF-8).
std::string to_utf8(const std::string& s);
bool debug_on();

}  // namespace retro
