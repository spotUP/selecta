// bbmp_sclub shim (row I710ac): C entry points over the unchanged bbmp csrc/misc/sclub_cr.cpp (GPL-3.0-or-later). The engine's
// stdio front end (playSoundClub) goes through four helpers of bbmp-misc.cpp; they are stubbed here (never called by the shim),
// and the song is loaded and rendered into memory through the engine's own load / Player.
#include "sclub_cr.cpp"

namespace misc {
void header(const Info&) {}
int fail(const std::string&) { return 1; }
bool emit(const int16_t*, size_t) { return false; }
std::vector<uint8_t> readFile(const char*) { return {}; }
}  // namespace misc

#include <stdlib.h>

extern "C" {

struct scr {
  misc::Song song;
  long long ms = 0;
  std::string title, type, comment;
};

/* 1 when the bytes carry the Sound Club 1 ("SNGs") or Sound Club 2 ("SN2") magic. */
int scr_probe(const uint8_t* data, uint32_t size) {
  return (size >= 4 && memcmp(data, "SNGs", 4) == 0) || (size >= 31 && memcmp(data, "SN2", 3) == 0);
}

/* Loads the song (instruments decoded, FM rendered); NULL when the engine refuses it (bank files missing, damaged). */
struct scr* scr_open(const uint8_t* data, uint32_t size) {
  scr* s = new scr();
  try {
    misc::DataFiles files;
    misc::load(std::vector<uint8_t>(data, data + size), s->song, files);
    s->ms = misc::duration_ms(s->song);
  } catch (const misc::Refusal&) {
    delete s;
    return nullptr;
  } catch (...) {
    delete s;
    return nullptr;
  }
  s->title = s->song.title;
  s->comment = s->song.comment;
  s->type = s->song.version == 1 ? "Sound Club" : "Sound Club 2";
  return s;
}

uint32_t scr_duration_ms(const struct scr* s) { return (uint32_t)s->ms; }
const char* scr_title(const struct scr* s) { return s->title.c_str(); }
const char* scr_type(const struct scr* s) { return s->type.c_str(); }

/* Renders the whole song at `rate`; *pcm is malloc'ed stereo int16, *frames its length. Returns 0 on failure. */
int scr_render(struct scr* s, uint32_t rate, int16_t** pcm, uint64_t* frames) {
  std::unique_ptr<misc::Player> p(new misc::Player(s->song, (int)rate));
  if (p->ended()) return 0;
  std::vector<int16_t> all;
  std::vector<int16_t> buf(2 * 4096);
  for (;;) {
    const int got = p->render(buf.data(), 4096);
    if (got <= 0) break;
    all.insert(all.end(), buf.begin(), buf.begin() + (size_t)got * 2);
    if (all.size() > (size_t)rate * 2 * 3600) break;
  }
  int16_t* out = (int16_t*)malloc(all.size() * sizeof(int16_t) + 4);
  if (out == nullptr) return 0;
  memcpy(out, all.data(), all.size() * sizeof(int16_t));
  *pcm = out;
  *frames = all.size() / 2;
  return 1;
}

void scr_close(struct scr* s) { delete s; }
}
