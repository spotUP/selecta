// bbmp_synder shim (row I710an): C entry points over bbmp's unchanged csrc/synder/bbmp-synder.cpp (GPL-3.0-or-later). That file is a helper program with its own
// main(); it is included here with main renamed, and the render loop of its main() is restated below on its Song, Synth and Resampler.
#define main bbmp_synder_helper_main
#include "bbmp-synder.cpp"
#undef main

extern "C" {

struct synsong {
  Song song;
  std::string title, author, type;
  long long duration_ms = 0;
  uint32_t rate = 0;
};

// Opens the song at `rate`; NULL when the file is refused (magic, type, sizes, damaged packed stream).
struct synsong* synsong_open(const uint8_t* data, uint32_t size, uint32_t rate) {
  if (size > MAX_FILE) return nullptr;
  std::unique_ptr<synsong> o(new synsong());
  std::vector<uint8_t> file(data, data + size);
  if (!load(file, o->song).empty()) return nullptr;
  makeTables();
  const std::string text = latin1(o->song.text);
  size_t by = text.compare(0, 3, "by ") == 0 ? 0 : text.find(" - by ");
  if (by != std::string::npos) {
    if (by > 0) o->title = trim(text.substr(0, by));
    const size_t from = text.find("by ", by) + 3;
    const size_t colon = text.find(" :", from);
    o->author = trim(text.substr(from, colon == std::string::npos ? std::string::npos : colon - from));
  }
  o->type = o->song.voices >= 4 ? "Synder SNG Stereo" : "Synder SNG";
  o->duration_ms = static_cast<long long>(o->song.frames * size_t(SYNTH_RATE / o->song.rate) * 1000 / SYNTH_RATE);
  o->rate = rate;
  return o.release();
}

uint32_t synsong_duration_ms(const struct synsong* o) { return static_cast<uint32_t>(o->duration_ms); }
const char* synsong_title(const struct synsong* o) { return o->title.c_str(); }
const char* synsong_author(const struct synsong* o) { return o->author.c_str(); }
const char* synsong_type(const struct synsong* o) { return o->type.c_str(); }

// Renders the whole song; *pcm is malloc'ed stereo int16, *frames its length. Returns 0 on failure.
int synsong_render(struct synsong* o, int16_t** pcm, uint64_t* frames) {
  Synth synth(o->song, false);
  const size_t total = synth.total();
  const size_t n = o->rate == SYNTH_RATE ? total : static_cast<size_t>(total * static_cast<unsigned long long>(o->rate) / SYNTH_RATE);
  *frames = n;
  *pcm = static_cast<int16_t*>(std::malloc(n * 2 * sizeof(int16_t) + 4));
  if (*pcm == nullptr) return 0;
  if (o->rate == SYNTH_RATE) {
    for (size_t i = 0; i < n; ++i) synth.sample(*pcm + 2 * i);
  } else {
    Resampler rs(SYNTH_RATE, static_cast<int>(o->rate), source, &synth);
    for (size_t i = 0; i < n; ++i) rs.frame(*pcm + 2 * i);
  }
  return 1;
}

void synsong_close(struct synsong* o) { delete o; }
}
