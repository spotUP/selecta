// bbmp_fac shim (row I710ak): C entry points over the unchanged bbmp csrc/fac/fac_cr.cpp (GPL-3.0-or-later). The file's
// stdio front end is left out (FAC_NO_MAIN); this renders the whole song into memory through its own Render::run.
#define FAC_NO_MAIN
#include "fac_cr.cpp"

#include <stdio.h>
#include <stdlib.h>

extern "C" {

struct facr {
  Song song;
  Kit kit;
  Length len;
  bool fifty = false;
  std::string title, author, type, comment;
};

/* 1 when the bytes load as a FAC SoundTracker song (the loader's own checks, no length measuring). */
int facr_probe(const uint8_t* data, uint32_t size, const char* path) {
  if (path != nullptr && (endsWithNoCase(path, ".sm1") || endsWithNoCase(path, ".sm2"))) return 0;
  std::vector<uint8_t> d(data, data + size);
  Song s;
  return load(d, s, 0).empty();
}

struct facr* facr_open(const uint8_t* data, uint32_t size, const char* path) {
  std::vector<uint8_t> d(data, data + size);
  if (path != nullptr) {
    const std::string p = path;
    if (endsWithNoCase(p, ".sm1") || endsWithNoCase(p, ".sm2")) return nullptr; // a drum kit, not a song
  }
  facr* f = new facr();
  if (!load(d, f->song, 0).empty()) {
    delete f;
    return nullptr;
  }
  f->fifty = fiftyFromEnv();
  if (!f->song.music && f->song.samples) f->kit = findKit(path != nullptr ? path : "", f->song.kit);
  f->len = measure(f->song, f->fifty, f->kit.halves);
  f->title = f->song.title;
  f->author = f->song.author;
  f->type = std::string("FAC SoundTracker ") + (f->song.version == VPro ? "Pro" : f->song.version == V20 ? "2.0" : "1.0") +
            (f->song.music ? ", MSX-MUSIC" : ", MSX-AUDIO");
  return f;
}

uint32_t facr_duration_ms(const struct facr* f) { return (uint32_t)((f->len.songSeconds + f->len.tail) * 1000.0); }
const char* facr_title(const struct facr* f) { return f->title.c_str(); }
const char* facr_author(const struct facr* f) { return f->author.c_str(); }
const char* facr_type(const struct facr* f) { return f->type.c_str(); }

/* Renders the whole song at `rate`; *pcm is malloc'ed stereo int16, *frames its length. Returns 0 on failure. */
int facr_render(struct facr* f, uint32_t rate, int16_t** pcm, uint64_t* frames) {
  char* buf = nullptr;
  size_t n = 0;
  FILE* mem = open_memstream(&buf, &n);
  if (mem == nullptr) return 0;
  Render r{f->song, f->kit, f->fifty, (int)rate, f->len, nullptr};
  r.run(0, mem);
  fclose(mem);
  *pcm = (int16_t*)buf;
  *frames = n / 4u;
  return buf != nullptr;
}

void facr_close(struct facr* f) { delete f; }
}
