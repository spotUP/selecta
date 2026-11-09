// bbmp_monotone shim (row I710ao): C entry points over bbmp's unchanged csrc/pcmusic/monotone.cpp (GPL-3.0-or-later). That file is the engine half of the
// bbmp-pcmusic helper and calls the helper's I/O functions (common.h); they are restated here over memory: readFile returns the song in memory, emit collects
// the PCM, header keeps the song's facts. playMonotone() is the helper's own render loop.
#include <algorithm>
#include <cstdlib>
#include "monotone.cpp"

namespace pcmusic {

namespace {
struct Io {
  const uint8_t* data = nullptr;
  size_t size = 0;
  std::vector<int16_t> pcm;   // stereo
  Info info;
  bool failed = false;
};
thread_local Io* g_io = nullptr;
}  // namespace

std::string fromCp437(const std::string& s) {
  static const char32_t hi[128] = {
      U'Ç', U'ü', U'é', U'â', U'ä', U'à', U'å', U'ç', U'ê', U'ë', U'è', U'ï', U'î', U'ì', U'Ä', U'Å',
      U'É', U'æ', U'Æ', U'ô', U'ö', U'ò', U'û', U'ù', U'ÿ', U'Ö', U'Ü', U'¢', U'£', U'¥', U'₧', U'ƒ',
      U'á', U'í', U'ó', U'ú', U'ñ', U'Ñ', U'ª', U'º', U'¿', U'⌐', U'¬', U'½', U'¼', U'¡', U'«', U'»',
      U'░', U'▒', U'▓', U'│', U'┤', U'╡', U'╢', U'╖', U'╕', U'╣', U'║', U'╗', U'╝', U'╜', U'╛', U'┐',
      U'└', U'┴', U'┬', U'├', U'─', U'┼', U'╞', U'╟', U'╚', U'╔', U'╩', U'╦', U'╠', U'═', U'╬', U'╧',
      U'╨', U'╤', U'╥', U'╙', U'╘', U'╒', U'╓', U'╫', U'╪', U'┘', U'┌', U'█', U'▄', U'▌', U'▐', U'▀',
      U'α', U'ß', U'Γ', U'π', U'Σ', U'σ', U'µ', U'τ', U'Φ', U'Θ', U'Ω', U'δ', U'∞', U'φ', U'ε', U'∩',
      U'≡', U'±', U'≥', U'≤', U'⌠', U'⌡', U'÷', U'≈', U'°', U'∙', U'·', U'√', U'ⁿ', U'²', U'■', U' '};
  std::string out;
  for (unsigned char c : s) {
    char32_t cp = c < 128 ? c : hi[c - 128];
    if (cp < 0x80) out += static_cast<char>(cp);
    else if (cp < 0x800) { out += static_cast<char>(0xc0 | cp >> 6); out += static_cast<char>(0x80 | (cp & 0x3f)); }
    else { out += static_cast<char>(0xe0 | cp >> 12); out += static_cast<char>(0x80 | ((cp >> 6) & 0x3f)); out += static_cast<char>(0x80 | (cp & 0x3f)); }
  }
  return out;
}

std::string field(const void* p, size_t size) {
  const char* c = static_cast<const char*>(p);
  size_t n = 0;
  while (n < size && c[n] != 0) n++;
  while (n > 0 && c[n - 1] == ' ') n--;
  return std::string(c, n);
}

void header(const Info& info) { g_io->info = info; }
int fail(const std::string&) { g_io->failed = true; return 1; }
bool emit(const int16_t* stereo, size_t frames) {
  g_io->pcm.insert(g_io->pcm.end(), stereo, stereo + 2 * frames);
  return true;
}
std::vector<uint8_t> readFile(const char*) { return std::vector<uint8_t>(g_io->data, g_io->data + g_io->size); }

}  // namespace pcmusic

extern "C" {

struct monsong {
  std::vector<uint8_t> file;
  std::string title, comment, type;
  long long duration_ms = 0;
};

static int monsong_run(pcmusic::Io& io, uint32_t rate) {
  pcmusic::g_io = &io;
  const int rc = pcmusic::playMonotone("", 0, static_cast<int>(rate), 0);
  pcmusic::g_io = nullptr;
  return rc == 0 && !io.failed;
}

// Opens the song; NULL when the file is refused (magic, version, track count, empty, no notes).
struct monsong* monsong_open(const uint8_t* data, uint32_t size) {
  std::unique_ptr<monsong> o(new monsong());
  o->file.assign(data, data + size);
  pcmusic::Io io;
  io.data = o->file.data();
  io.size = o->file.size();
  if (!monsong_run(io, 8000) || io.info.durationMs == 0) return nullptr;
  o->title = io.info.title;
  o->comment = io.info.comment;
  o->type = io.info.type;
  o->duration_ms = io.info.durationMs;
  return o.release();
}

uint32_t monsong_duration_ms(const struct monsong* o) { return static_cast<uint32_t>(o->duration_ms); }
const char* monsong_title(const struct monsong* o) { return o->title.c_str(); }
const char* monsong_comment(const struct monsong* o) { return o->comment.c_str(); }
const char* monsong_type(const struct monsong* o) { return o->type.c_str(); }

// Renders the whole song at `rate`; *pcm is malloc'ed stereo int16, *frames its length. Returns 0 on failure.
int monsong_render(struct monsong* o, uint32_t rate, int16_t** pcm, uint64_t* frames) {
  pcmusic::Io io;
  io.data = o->file.data();
  io.size = o->file.size();
  if (!monsong_run(io, rate)) return 0;
  const size_t n = io.pcm.size() / 2;
  *frames = n;
  *pcm = static_cast<int16_t*>(std::malloc(n * 2 * sizeof(int16_t) + 4));
  if (*pcm == nullptr) return 0;
  std::copy(io.pcm.begin(), io.pcm.end(), *pcm);
  return 1;
}

void monsong_close(struct monsong* o) { delete o; }
}
