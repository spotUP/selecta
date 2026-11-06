// Selecta shim over bbmp's piece::Song (third_party/bbmp_piece, GPL-3.0-or-later): a C API for adapter.c. Written for Selecta.
// The song is rendered whole at start: 16 kHz mono from the engine, to the host rate by straight lines between samples (as bbmp's
// bbmp-piece.cpp does and as the P/ECE doubles its 16 kHz), a loop-forever song gets the same 8 s fade bbmp gives it.
#include "piece.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

struct pcsong {
  std::vector<uint8_t> data;
  piece::Song* song = nullptr;
  piece::Length len;
  std::string title;
};

extern "C" {

struct pcsong* pc_open(const uint8_t* data, uint32_t size) {
  std::vector<uint8_t> v(data, data + size);
  std::string why;
  piece::Song* s = piece::load(v, why);
  if (s == nullptr) return nullptr;
  pcsong* p = new pcsong;
  p->song = s;
  p->len = piece::measure(v);
  p->data = std::move(v);
  const std::string t = piece::title(s);
  bool ascii = true;
  for (unsigned char c : t) if (c < 0x20 || c > 0x7e) ascii = false;
  if (ascii) p->title = t;
  return p;
}

void pc_close(struct pcsong* p) {
  if (p == nullptr) return;
  piece::destroy(p->song);
  delete p;
}

const char* pc_title(const struct pcsong* p) { return p->title.c_str(); }

uint32_t pc_duration_ms(const struct pcsong* p) {
  return static_cast<uint32_t>((p->len.seconds + (p->len.loops ? 8.0 : 0.0)) * 1000.0);
}

// Renders the whole song as stereo 16-bit at `rate`; *pcm is malloc'd. Returns 1 on success.
int pc_render(struct pcsong* p, uint32_t rate, int16_t** pcm, uint64_t* frames) {
  piece::destroy(p->song);
  std::string why;
  p->song = piece::load(p->data, why);
  if (p->song == nullptr) return 0;
  const double total = p->len.seconds + (p->len.loops ? 8.0 : 0.0);
  const long long end = static_cast<long long>(total * rate);
  const long long fadeFrom = p->len.loops ? static_cast<long long>(p->len.seconds * rate) : end;
  const double stepIn = 16000.0 / rate;
  int16_t* out = static_cast<int16_t*>(malloc((static_cast<size_t>(end) + 1) * 4));
  if (out == nullptr) return 0;
  std::vector<float> in(4096);
  size_t have = 0, at = 0;
  bool ended = false;
  double pos = 0;
  float prev = 0;
  long long k = 0;
  for (; k < end; k++) {
    while (pos >= 1.0 || at >= have) {
      if (at < have && pos >= 1.0) { prev = in[at]; at++; pos -= 1.0; continue; }
      if (ended) break;
      const int got = piece::render16k(p->song, in.data(), static_cast<int>(in.size()));
      have = static_cast<size_t>(got);
      at = 0;
      if (got < static_cast<int>(in.size())) ended = true;
      if (got == 0) break;
    }
    if (at >= have) break;
    double v = prev + (in[at] - prev) * pos;
    pos += stepIn;
    if (k > fadeFrom) v *= 1.0 - double(k - fadeFrom) / (end - fadeFrom);
    const long s = std::lround(std::fmax(-1.0, std::fmin(1.0, v)) * 32767.0);
    out[2 * k] = out[2 * k + 1] = static_cast<int16_t>(s);
  }
  *pcm = out;
  *frames = static_cast<uint64_t>(k);
  return 1;
}

}
