// I1223c: Beepola's integer sound path (BBAY_INT_SOUND, the 68k build) renders a whole song on the host in open()
// (BBAY_NO_THREADS) with the float path's frame count and real output, and a truncated song is refused.
// usage: beepola_int_sound <song.bbsong> <expected frames at 22050 Hz>   (BBMP_BEEPOLA_DATA names the data folder)
#include "engine.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>

static int open_frames(const std::vector<uint8_t>& d, long* frames, long long* energy) {
  auto e = ay::make_beepola();
  ay::Info info;
  try {
    e->open("", d, 0, 22050, info);
  } catch (const ay::Refused&) {
    return 0;
  }
  int16_t buf[2048];
  int k;
  *frames = 0;
  *energy = 0;
  while ((k = e->render(buf, 1024)) > 0) {
    for (int i = 0; i < k; ++i) *energy += (long long)buf[2 * i] * buf[2 * i];
    *frames += k;
  }
  return 1;
}

int main(int argc, char** argv) {
  if (argc < 3) return 2;
  std::ifstream f(argv[1], std::ios::binary);
  const std::vector<uint8_t> d((std::istreambuf_iterator<char>(f)), {});
  const long want = std::atol(argv[2]);
  long frames = 0;
  long long energy = 0;
  if (!open_frames(d, &frames, &energy)) { std::printf("FAIL: the song was refused\n"); return 1; }
  if (frames < want - 2 || frames > want + 2) { std::printf("FAIL: frames=%ld, want %ld\n", frames, want); return 1; }
  if (energy / frames < 1000000) { std::printf("FAIL: silent, mean square %lld\n", energy / frames); return 1; }
  const std::vector<uint8_t> cut(d.begin(), d.begin() + 100);
  if (open_frames(cut, &frames, &energy)) { std::printf("FAIL: a 100-byte truncated song was accepted\n"); return 1; }
  std::printf("beepola_int_sound_ok frames=%ld\n", want);
  return 0;
}
