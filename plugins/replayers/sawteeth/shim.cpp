// sawteeth shim (row I1242e): C entry points over Jonas Norberg's unchanged Sawteeth player library (third_party/sawteeth, MIT).
// The library renders 44100 Hz stereo float in 882-sample ticks (50 Hz PAL) and reports when the song looped; the whole first
// cycle is rendered here and converted to 16-bit.
#include "stSong.h"
#include "txt.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {
const uint32_t kRate = 44100;
const uint64_t kMostFrames = static_cast<uint64_t>(kRate) * 1800;  // endless songs stop after 30 minutes
}  // namespace

extern "C" {

struct swtsong {
  stSong* song;
  std::vector<char> file;
};

// NULL when the library refuses the file (magic, version, truncation, instrument numbers out of range).
struct swtsong* swtsong_open(const uint8_t* data, uint32_t size) {
  if (size < 6) return nullptr;
  swtsong* o = new swtsong();
  o->file.assign(reinterpret_cast<const char*>(data), reinterpret_cast<const char*>(data) + size);
  txt t(o->file.data(), static_cast<int>(size));
  o->song = new stSong(t);
  if (o->song->InitCheck() != JNG_OK || o->song->Sps() < 1) {
    delete o->song;
    delete o;
    return nullptr;
  }
  return o;
}

const char* swtsong_title(struct swtsong* o) { return o->song->Name() != nullptr ? o->song->Name() : ""; }
const char* swtsong_author(struct swtsong* o) { return o->song->Author() != nullptr ? o->song->Author() : ""; }

// Renders the first cycle (until the song loops, at most 30 minutes) at 44100 Hz. *pcm is malloc'ed stereo int16.
int swtsong_render(struct swtsong* o, int16_t** pcm, uint64_t* frames) {
  const uint32_t chunk = 882;
  std::vector<float> buf(chunk * 2);
  std::vector<int16_t> out;
  o->song->SetPos(0);
  for (;;) {
    bool looped = o->song->Play(buf.data(), chunk);
    for (uint32_t i = 0; i < chunk * 2; i++) {
      float v = buf[i] * 32767.0f;
      if (!(v == v)) v = 0.0f;
      if (v > 32767.0f) v = 32767.0f;
      if (v < -32768.0f) v = -32768.0f;
      out.push_back(static_cast<int16_t>(v));
    }
    if (looped || out.size() / 2 >= kMostFrames) break;
  }
  *frames = out.size() / 2;
  *pcm = static_cast<int16_t*>(std::malloc(out.size() * sizeof(int16_t) + 4));
  if (*pcm == nullptr) return 0;
  std::memcpy(*pcm, out.data(), out.size() * sizeof(int16_t));
  return 1;
}

void swtsong_close(struct swtsong* o) {
  if (o != nullptr) {
    delete o->song;
    delete o;
  }
}
}
