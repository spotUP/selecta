// SPDX-License-Identifier: GPL-3.0-or-later
// In-process entry for bbmp's BeRoTracker replayer (row I710b). bbmp-berotracker.cpp is a command-line helper; it is included
// unchanged with its main() renamed, and this file adds a small C API over its Song / Engine / Player / Resampler (the same
// calls its run() makes), so the plugin needs no child process. State lives in one brt_song per open song.
#define main bbmp_berotracker_cli_main
#include "bbmp-berotracker.cpp"
#undef main

#include <mutex>

#include "brt_engine.h"

struct brt_song {
  std::unique_ptr<Song> song;
  std::unique_ptr<Engine> engine;
  std::unique_ptr<Player> player;
  std::unique_ptr<Resampler> rs;
  long long left = 0, total = 0;
};

namespace {
std::once_flag g_tables;

void copy(char* dst, size_t n, const std::string& s) {
  std::snprintf(dst, n, "%s", s.c_str());
}
}  // namespace

extern "C" void brt_close(brt_song* s) { delete s; }

extern "C" brt_song* brt_open(const uint8_t* data, size_t size, brt_info* info) {
  info->error[0] = 0;
  info->title[0] = 0;
  info->duration_ms = 0;
  info->frames_44100 = 0;
  std::unique_ptr<brt_song> s(new brt_song());
  try {
    // constants() and makeFir() fill process-wide tables with the same values every time; run them once.
    std::call_once(g_tables, [] {
      constants();
      makeFir();
    });
    if (size == 0 || size > MAX_FILE) {
      copy(info->error, sizeof info->error, "not a BeRoTracker song");
      return nullptr;
    }
    std::vector<uint8_t> file(data, data + size);
    s->song.reset(new Song());
    std::string why = load(file, *s->song);
    if (why.empty()) why = prepare(*s->song);
    if (!why.empty()) {
      copy(info->error, sizeof info->error, why);
      return nullptr;
    }
    const long long total = measure(*s->song);
    if (total < 0) {
      copy(info->error, sizeof info->error, "the song does not end within 30 minutes");
      return nullptr;
    }
    s->total = total;
    copy(info->title, sizeof info->title, trim(latin1(s->song->title)));
    info->duration_ms = total * 1000 / RATE;
    info->frames_44100 = total;
    return s.release();
  } catch (const std::bad_alloc&) {
    copy(info->error, sizeof info->error, "the song needs more memory than there is");
    return nullptr;
  }
}

extern "C" int brt_start(brt_song* s, uint32_t rate) {
  if (!s || !s->song) return 0;
  try {
    s->rs.reset();
    s->player.reset();
    s->engine.reset(new Engine(*s->song, false));
    s->player.reset(new Player{s->engine.get()});
    if (rate == (uint32_t)RATE) {
      s->left = s->total;
    } else {
      s->rs.reset(new Resampler(RATE, (int)rate, source, s->player.get()));
      s->left = (long long)((unsigned long long)s->total * rate / RATE);
    }
    return 1;
  } catch (const std::bad_alloc&) {
    return 0;
  }
}

extern "C" uint32_t brt_render(brt_song* s, int16_t* out, uint32_t frames) {
  if (!s || !s->player) return 0;
  const uint32_t n = (long long)frames > s->left ? (uint32_t)s->left : frames;
  for (uint32_t i = 0; i < n; ++i) {
    if (s->rs) s->rs->frame(out + 2u * i);
    else s->player->frame(out + 2u * i);
  }
  s->left -= n;
  return n;
}
