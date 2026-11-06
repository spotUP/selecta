// bbmp_ntgs shim (row I710p): C entry points over the unchanged bbmp csrc/iigs (GPL-3.0-or-later). bbmp-iigs.cpp is
// included with its main() renamed: its Resampler and bent() turn the chip's 27118 Hz into the asked rate exactly as the
// helper does. The render loop below is the helper's, into memory instead of stdout. SoundSmith (ss_cr.cpp) is not part of
// this module (bbmp_iigs): its loader is stubbed out.
#define main bbmp_ntgs_helper_main
#include "bbmp-iigs.cpp"
#undef main

Song* load_soundsmith(const std::string&, const Bytes&, std::string& why) {
  why = "SoundSmith is not part of this module";
  return nullptr;
}

extern "C" {

struct ntgs {
  std::string path;
  Bytes file;
  std::unique_ptr<Song> song;
  std::string title, author, type;
  uint32_t duration_ms = 0;
  int sides = 0;
};

// 1 when the file is a Noise Tracker GS module: the loader's own checks (magic "FTA MODULEFILE" at byte 2, header).
int ntgs_probe(const uint8_t* d, uint32_t n) {
  if (n < 606u || std::memcmp(d + 2, "FTA MODULEFILE", 14) != 0) return 0;
  Bytes f(d, d + n);
  std::string why;
  std::unique_ptr<Song> s(load_noisetracker("", f, why));
  return s != nullptr;
}

struct ntgs* ntgs_open(const uint8_t* data, uint32_t size, const char* path) {
  if (path == nullptr || !ntgs_probe(data, size)) return nullptr;
  std::unique_ptr<ntgs> f(new ntgs());
  f->path = path;
  f->file.assign(data, data + size);
  std::string why;
  f->song.reset(load_noisetracker(f->path, f->file, why));
  if (!f->song) return nullptr;
  f->title = f->song->title;
  f->author = f->song->author;
  f->type = f->song->type;
  // The song's length: once through with only the chip's clock running.
  std::unique_ptr<Song> dry(load_noisetracker(f->path, f->file, why));
  if (!dry) return nullptr;
  Doc5503 doc;
  dry->start(doc);
  const long long most = static_cast<long long>(kLongest * doc.rate());
  long long n = 0;
  while (!dry->ended() && n < most) { doc.step_clock(); dry->idle(); ++n; }
  f->duration_ms = n >= most ? 0 : static_cast<uint32_t>(n / doc.rate() * 1000 + 0.5);
  f->sides = doc.sides();
  return f.release();
}

uint32_t ntgs_duration_ms(const struct ntgs* f) { return f->duration_ms; }
const char* ntgs_title(const struct ntgs* f) { return f->title.c_str(); }
const char* ntgs_author(const struct ntgs* f) { return f->author.c_str(); }
const char* ntgs_type(const struct ntgs* f) { return f->type.c_str(); }

// Renders the whole song at `rate`; *pcm is malloc'ed stereo int16, *frames its length. Returns 0 on failure.
int ntgs_render(struct ntgs* f, uint32_t rate, int16_t** pcm, uint64_t* frames) {
  std::string why;
  std::unique_ptr<Song> song(load_noisetracker(f->path, f->file, why));
  if (!song) return 0;
  std::vector<int16_t> out;
  Doc5503 doc;
  song->start(doc);
  Resampler rs(doc.rate(), rate);
  const float gain = 1.0f / 8;
  const float near = f->sides == 3 ? 0.8f : 0.5f, far = 1 - near;
  const long long most = static_cast<long long>(kLongest * doc.rate());
  int tail = 64;
  for (long long n = 0; n < most && tail > 0; ++n) {
    int32_t l = 0, r = 0;
    if (song->ended()) --tail;
    else {
      doc.step(l, r);
      song->idle();
    }
    rs.push((l * near + r * far) * gain, (r * near + l * far) * gain);
    float a, b;
    while (rs.pull(a, b)) {
      out.push_back(bent(a));
      out.push_back(bent(b));
    }
  }
  *frames = out.size() / 2;
  *pcm = static_cast<int16_t*>(std::malloc(out.size() * sizeof(int16_t) + 4));
  if (*pcm == nullptr) return 0;
  std::memcpy(*pcm, out.data(), out.size() * sizeof(int16_t));
  return 1;
}

void ntgs_close(struct ntgs* f) { delete f; }
}
