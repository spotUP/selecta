// bbmp_spu shim (row I710y): C entry points over the unchanged bbmp csrc/console SPU engine (spu_engine.cpp, GPL-2.0-or-later)
// and the P.E.Op.S. SPU of the Audio Overload SDK (third_party/bbmp_spu/peops, compiled as C by the CMakeLists). The engine's
// own open() validates the whole log, so a file that is not an SPU log fails there. P.E.Op.S. keeps its state in globals, so
// the engine used for the song's info is destroyed before the one that plays is opened.
#include "spu_engine.cpp"

#include <cstdlib>

extern "C" {

struct spu {
  std::vector<uint8_t> file;
  std::string title, author, comment, type;
  uint32_t duration_ms = 0;
};

// 1 when the head of the file looks like an SPU log (the engine's open() checks the rest).
int spu_probe(const uint8_t* d, uint32_t n) {
  const size_t log = 0x80000 + 0x200;
  if (n < log + 8 + 12) return 0;
  const uint32_t a = le32(d + log), b = le32(d + log + 4);
  if (a == 44100) return b > 0 && log + 8 + static_cast<uint64_t>(b) * 12 <= n;
  return b <= a && a <= 4u * 3600 * 44100;
}

struct spu* spu_open(const uint8_t* data, uint32_t size) {
  if (!spu_probe(data, size)) return nullptr;
  std::unique_ptr<spu> f(new spu());
  f->file.assign(data, data + size);
  std::unique_ptr<Engine> e(make_spu_engine());
  SongInfo info;
  std::string why;
  if (!e->open(f->file, 0, 44100, info, why)) return nullptr;
  f->title = info.title;
  f->author = info.author;
  f->comment = info.comment;
  f->type = info.type;
  f->duration_ms = static_cast<uint32_t>(info.duration_ms);
  return f.release();
}

uint32_t spu_duration_ms(const struct spu* f) { return f->duration_ms; }
const char* spu_title(const struct spu* f) { return f->title.c_str(); }
const char* spu_author(const struct spu* f) { return f->author.c_str(); }
const char* spu_comment(const struct spu* f) { return f->comment.c_str(); }
const char* spu_type(const struct spu* f) { return f->type.c_str(); }

// Renders the whole song at `rate`; *pcm is malloc'ed stereo int16, *frames its length. Returns 0 on failure.
int spu_render(struct spu* f, uint32_t rate, int16_t** pcm, uint64_t* frames) {
  std::unique_ptr<Engine> e(make_spu_engine());
  SongInfo info;
  std::string why;
  if (!e->open(f->file, 0, static_cast<int>(rate), info, why)) return 0;
  std::vector<int16_t> out;
  std::vector<int16_t> chunk(4096 * 2);
  for (;;) {
    const size_t n = e->render(chunk.data(), 4096);
    out.insert(out.end(), chunk.begin(), chunk.begin() + n * 2);
    if (n < 4096) break;
  }
  *frames = out.size() / 2;
  *pcm = static_cast<int16_t*>(std::malloc(out.size() * sizeof(int16_t) + 4));
  if (*pcm == nullptr) return 0;
  std::memcpy(*pcm, out.data(), out.size() * sizeof(int16_t));
  return 1;
}

void spu_close(struct spu* f) { delete f; }
}
