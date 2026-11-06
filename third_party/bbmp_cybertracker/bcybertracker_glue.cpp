// SPDX-License-Identifier: GPL-3.0-or-later
// Selecta glue for bbmp's CyberTracker engine (row I710g): provides what bbmp-retro's main program gives an engine (the registry,
// debug_on, read_file, dir_of), finds the user's CyberTracker disk image, and renders a track through the engine's own
// open()/render(), the loop bbmp-retro's main runs.
#include "cybertracker_engine.cpp"

#include <fstream>

namespace retro {
static EngineMaker g_maker;  // constant-initialised, so the engine's static registrar may run before this file's own dynamic init
void register_engine(const char*, EngineMaker maker) { g_maker = maker; }
bool debug_on() { return false; }

std::vector<uint8_t> read_file(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) return {};
  return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

std::string dir_of(const std::string& path) {
  const size_t slash = path.rfind('/');
  return slash == std::string::npos ? std::string() : path.substr(0, slash + 1);
}
}  // namespace retro

namespace {
// The engine reads BBMP_CYBERTRACKER_DISK. Selecta's order: SELECTA_CYBERTRACKER_DISK, BBMP_CYBERTRACKER_DISK, then the config directory.
void pick_disk() {
  const char* own = std::getenv("SELECTA_CYBERTRACKER_DISK");
  if (own && *own) {
    setenv("BBMP_CYBERTRACKER_DISK", own, 1);
    return;
  }
  const char* bbmp = std::getenv("BBMP_CYBERTRACKER_DISK");
  if (bbmp && *bbmp) return;
  const char* xdg = std::getenv("XDG_CONFIG_HOME");
  const char* home = std::getenv("HOME");
  std::string dir;
  if (xdg && *xdg) dir = std::string(xdg) + "/selecta";
  else if (home && *home) dir = std::string(home) + "/.config/selecta";
  else return;
  setenv("BBMP_CYBERTRACKER_DISK", (dir + "/cybertracker101.d64").c_str(), 1);
}
}  // namespace

extern "C" {
// Renders track `track` at `rate`. Returns 1 and a malloc'd s16 stereo buffer, or 0 with `err` set. check_only: open() only.
// *tracks is the number of songs in the file (also set when the track asked for does not exist).
int bbcybertracker_render(const uint8_t* data, size_t size, uint32_t rate, int track, int check_only, int16_t** pcm, uint64_t* frames,
                          long* duration_ms, int* tracks, char* err, size_t errn) {
  if (retro::g_maker == nullptr) {
    std::snprintf(err, errn, "engine not registered");
    return 0;
  }
  pick_disk();
  std::unique_ptr<retro::Engine> engine(retro::g_maker());
  std::vector<uint8_t> file(data, data + size);
  retro::SongInfo info;
  std::string why;
  const bool ok = engine->open(file, "", track, (int)rate, info, why);
  if (tracks != nullptr) *tracks = info.tracks;
  if (!ok) {
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
