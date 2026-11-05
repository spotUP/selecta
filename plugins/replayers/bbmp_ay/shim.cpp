// C ABI over the bbmp ay engines (third_party/bbmp_ay): open a song, render stereo s16 frames, close. Engines throw
// ay::Refused for a file that is not theirs; that becomes a NULL return here, nothing crosses into C.
#include "engine.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <memory>
#include <string>
#include <vector>

extern "C" {

struct bbay {
  std::unique_ptr<ay::Engine> engine;
  ay::Info info;
};

// kind: 0 fls, 1 fxm (also .amad), 2 strc. Fills the strings (NUL terminated, truncated). Returns NULL when refused.
struct bbay* bbay_open(int kind, const uint8_t* data, uint32_t size, uint32_t track, uint32_t rate, uint32_t* tracks,
                       uint32_t* duration_ms, char* title, char* author, char* comment, uint32_t text_max) {
  try {
    std::unique_ptr<bbay> s(new bbay);
    s->engine = kind == 0 ? ay::make_fls() : kind == 1 ? ay::make_fxm() : ay::make_strc();
    std::vector<uint8_t> bytes(data, data + size);
    s->engine->open("", bytes, static_cast<int>(track), static_cast<int>(rate), s->info);
    *tracks = s->info.tracks < 1 ? 1u : static_cast<uint32_t>(s->info.tracks);
    *duration_ms = s->info.duration_ms > 0 ? static_cast<uint32_t>(s->info.duration_ms) : 0u;
    std::snprintf(title, text_max, "%s", s->info.title.c_str());
    std::snprintf(author, text_max, "%s", s->info.author.c_str());
    std::snprintf(comment, text_max, "%s", s->info.comment.c_str());
    return s.release();
  } catch (const std::exception&) {
    return nullptr;
  }
}

uint32_t bbay_render(struct bbay* s, int16_t* out, uint32_t frames) {
  try {
    const int n = s->engine->render(out, static_cast<int>(frames));
    return n < 0 ? 0u : static_cast<uint32_t>(n);
  } catch (const std::exception&) {
    return 0;
  }
}

void bbay_close(struct bbay* s) { delete s; }

}  // extern "C"
