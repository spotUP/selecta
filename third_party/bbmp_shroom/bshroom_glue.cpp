// SPDX-License-Identifier: GPL-3.0-or-later
// Selecta glue for bbmp's Shroom engine (row I710aa): provides what bbmp-retro's main program gives an engine (the registry, debug_on,
// read_file, dir_of, find_beside, the text helpers), turns the user's Mariopants notes.wav into the engine's note table when the engine
// reads it, and renders the song through the engine's own open()/render(). The conversion is a port of bbmp csrc/retro/shroom_samples.cpp
// (same thresholds, same 400 samples of fade), so the notes come out as the separate tool would write them.
#include "shroom_engine.cpp"

#include <fstream>

namespace retro {
static EngineMaker g_maker;  // constant-initialised, so the engine's static registrar may run before this file's own dynamic init
void register_engine(const char*, EngineMaker maker) { g_maker = maker; }
bool debug_on() { return false; }

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
std::string dir_of(const std::string& path) {
  const size_t slash = path.rfind('/');
  return slash == std::string::npos ? std::string() : path.substr(0, slash + 1);
}
// Songs arrive from memory with no folder: no sibling lookup. The sample set comes only from the declared system file.
std::string find_beside(const std::string&, const std::string&) { return std::string(); }

namespace {
const int kShInstruments = 15, kShPitches = 13;
const int kQuiet = 26, kGap = 3200, kTail = 400;
bool quiet(int s) { return s < kQuiet && s > -kQuiet; }
void put32(std::vector<uint8_t>& out, uint32_t v) {
  for (int i = 0; i < 4; i++) out.push_back(static_cast<uint8_t>(v >> (8 * i)));
}

// Mariopants' notes.wav (PCM mono 16 bit 32000 Hz, 44 byte header) -> "SHRONOTE" table; empty when it is not that file.
std::vector<uint8_t> notes_from_wav(const std::vector<uint8_t>& wav) {
  auto u16 = [&](size_t at) { return static_cast<unsigned>(wav[at] | wav[at + 1] << 8); };
  auto u32 = [&](size_t at) { return u16(at) | u16(at + 2) << 16; };
  if (wav.size() < 44 || std::memcmp(wav.data(), "RIFF", 4) != 0 || std::memcmp(wav.data() + 8, "WAVEfmt ", 8) != 0 || u32(16) != 16 ||
      u16(20) != 1 || u16(22) != 1 || u32(24) != 32000 || u16(34) != 16 || std::memcmp(wav.data() + 36, "data", 4) != 0)
    return {};
  std::vector<int> s;
  for (size_t at = 44; at + 1 < wav.size(); at += 2) s.push_back(static_cast<int16_t>(u16(at)));
  struct Note { size_t offset, length; };
  std::vector<Note> notes;
  std::vector<int16_t> data;
  size_t start = 0;
  for (;;) {
    while (start < s.size() && quiet(s[start])) start++;
    if (start >= s.size()) break;
    size_t end = start + 1;
    int gap = 0;
    while (end < s.size() && gap < kGap) {
      gap = quiet(s[end]) ? gap + 1 : 0;
      end++;
    }
    end -= gap;
    if (end - start <= 5) {
      start = end;
      continue;
    }
    end += kTail;
    if (end > s.size()) end = s.size();
    const size_t length = end - start;
    notes.push_back({data.size(), length});
    for (size_t i = 0; i < length; i++) {
      int v = s[start + i];
      const size_t fromEnd = length - 1 - i;
      if (fromEnd < static_cast<size_t>(kTail)) v = static_cast<int>(fromEnd) * v / kTail;
      data.push_back(static_cast<int16_t>(v));
    }
    start = end;
  }
  const size_t wanted = kShInstruments * kShPitches;
  if (notes.size() < wanted) return {};
  notes.resize(wanted);
  data.resize(notes.back().offset + notes.back().length);
  std::vector<uint8_t> out(8);
  std::memcpy(out.data(), "SHRONOTE", 8);
  put32(out, 1);
  put32(out, 32000);
  put32(out, static_cast<uint32_t>(wanted));
  for (const Note& n : notes) {
    put32(out, static_cast<uint32_t>(n.offset));
    put32(out, static_cast<uint32_t>(n.length));
  }
  for (int16_t v : data) {
    out.push_back(static_cast<uint8_t>(v & 0xff));
    out.push_back(static_cast<uint8_t>((v >> 8) & 0xff));
  }
  return out;
}
}  // namespace

std::vector<uint8_t> read_file(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) return {};
  std::vector<uint8_t> raw((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  if (raw.size() >= 4 && std::memcmp(raw.data(), "RIFF", 4) == 0) return notes_from_wav(raw);
  return raw;
}
}  // namespace retro

extern "C" {
// Renders the song at `rate`. notes_path: the user's notes.wav (or an already converted shroom-notes.bin) from the host's system file
// lookup; NULL leaves the engine without a sample set (open fails: "instrument sounds are missing"). check_only: open() only.
int bbshroom_render(const uint8_t* data, size_t size, uint32_t rate, int check_only, int16_t** pcm, uint64_t* frames, long* duration_ms,
                    char* err, size_t errn, const char* notes_path) {
  if (retro::g_maker == nullptr) {
    std::snprintf(err, errn, "engine not registered");
    return 0;
  }
  if (notes_path != nullptr && *notes_path != '\0') setenv("BBMP_SHROOM_NOTES", notes_path, 1);
  else unsetenv("BBMP_SHROOM_NOTES");
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
