// SPDX-License-Identifier: GPL-3.0-or-later
// In-process entry for bbmp's BeRoTracker replayer (row I710b). bbmp-berotracker.cpp is a command-line helper; it is included
// unchanged with its main() renamed, and this file adds a small C API over its Song / Engine / Player / Resampler (the same
// calls its run() makes), so the plugin needs no child process. State is global: one song at a time.
#define main bbmp_berotracker_cli_main
#include "bbmp-berotracker.cpp"
#undef main

#include "brt_engine.h"

namespace {
std::unique_ptr<Song> g_song;
std::unique_ptr<Engine> g_engine;
std::unique_ptr<Player> g_player;
std::unique_ptr<Resampler> g_rs;
long long g_left = 0, g_total = 0;

void copy(char* dst, size_t n, const std::string& s) {
  std::snprintf(dst, n, "%s", s.c_str());
}
}  // namespace

extern "C" void brt_close(void) {
  g_rs.reset();
  g_player.reset();
  g_engine.reset();
  g_song.reset();
  g_left = g_total = 0;
}

extern "C" int brt_open(const uint8_t* data, size_t size, brt_info* info) {
  brt_close();
  info->error[0] = 0;
  info->title[0] = 0;
  info->duration_ms = 0;
  try {
    constants();
    if (size == 0 || size > MAX_FILE) {
      copy(info->error, sizeof info->error, "not a BeRoTracker song");
      return 0;
    }
    std::vector<uint8_t> file(data, data + size);
    g_song.reset(new Song());
    std::string why = load(file, *g_song);
    if (why.empty()) why = prepare(*g_song);
    if (!why.empty()) {
      copy(info->error, sizeof info->error, why);
      brt_close();
      return 0;
    }
    const long long total = measure(*g_song);
    if (total < 0) {
      copy(info->error, sizeof info->error, "the song does not end within 30 minutes");
      brt_close();
      return 0;
    }
    g_total = total;
    makeFir();
    copy(info->title, sizeof info->title, trim(latin1(g_song->title)));
    info->duration_ms = total * 1000 / RATE;
    info->frames_44100 = total;
    return 1;
  } catch (const std::bad_alloc&) {
    copy(info->error, sizeof info->error, "the song needs more memory than there is");
    brt_close();
    return 0;
  }
}

extern "C" int brt_start(uint32_t rate) {
  if (!g_song) return 0;
  try {
    g_rs.reset();
    g_player.reset();
    g_engine.reset(new Engine(*g_song, false));
    g_player.reset(new Player{g_engine.get()});
    const long long total = brt_total_frames();
    if (rate == (uint32_t)RATE) {
      g_left = total;
    } else {
      g_rs.reset(new Resampler(RATE, (int)rate, source, g_player.get()));
      g_left = (long long)((unsigned long long)total * rate / RATE);
    }
    return 1;
  } catch (const std::bad_alloc&) {
    return 0;
  }
}

extern "C" long long brt_total_frames(void) { return g_song ? g_total : 0; }

extern "C" uint32_t brt_render(int16_t* out, uint32_t frames) {
  if (!g_player) return 0;
  const uint32_t n = (long long)frames > g_left ? (uint32_t)g_left : frames;
  for (uint32_t i = 0; i < n; ++i) {
    if (g_rs) g_rs->frame(out + 2u * i);
    else g_player->frame(out + 2u * i);
  }
  g_left -= n;
  return n;
}
