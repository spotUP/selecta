// SPDX-License-Identifier: GPL-3.0-or-later
// Selecta glue for bbmp's NerdTracker II engine (row I1223c): provides what bbmp-retro's main program gives an engine (the registry,
// debug_on, read_file, dir_of, the text helpers), points the engine at the directory of the two declared system files, and renders a song
// through the engine's own open()/render().
#include "ned_engine.cpp"

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
std::string from_cp437(const std::string& s) { return from_latin1(s); }
std::string field(const void* p, size_t size) {
  const char* c = static_cast<const char*>(p);
  size_t n = 0;
  while (n < size && c[n]) n++;
  while (n > 0 && (c[n - 1] == ' ' || c[n - 1] == '\t' || c[n - 1] == '\r' || c[n - 1] == '\n')) n--;
  return std::string(c, n);
}
}  // namespace retro

extern "C" {
// Renders the song at `rate`. Returns 1 and a malloc'd s16 stereo buffer, or 0 with `err` set. check_only: open() only.
// bin_path / lbl_path: the host-resolved paths of nt2replay.bin / nt2replay.lbl (both live in one directory); NULL = not given.
int bned_render(const uint8_t* data, size_t size, uint32_t rate, int check_only, int16_t** pcm, uint64_t* frames, long* duration_ms,
                char* title, size_t titlen, char* err, size_t errn, const char* bin_path, const char* lbl_path) {
  (void)lbl_path;
  if (retro::g_maker == nullptr) {
    std::snprintf(err, errn, "engine not registered");
    return 0;
  }
  if (bin_path == nullptr || *bin_path == '\0') {
    std::snprintf(err, errn, "the NerdTracker replay routine is missing");
    return 0;
  }
  g_replay_dir = retro::dir_of(bin_path);
  std::unique_ptr<retro::Engine> engine(retro::g_maker());
  std::vector<uint8_t> file(data, data + size);
  retro::SongInfo info;
  std::string why;
  if (!engine->open(file, "", 0, (int)rate, info, why)) {
    std::snprintf(err, errn, "%s", why.c_str());
    return 0;
  }
  if (duration_ms != nullptr) *duration_ms = (long)info.duration_ms;
  if (title != nullptr && titlen > 0) std::snprintf(title, titlen, "%s", info.title.c_str());
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
