/* SPDX-License-Identifier: GPL-3.0-or-later */
/*  In-process entry for bbmp-darkwave (row I710h): the helper's main() as a C API, no child process.
    Written for Selecta from bbmp-darkwave.cpp.

    The sound is DarkWave Studio 4.1.6's own: its studio (studio.cpp, a port) and the
    machines of its ES-CoreMachines (built from its source with the windows taken out).
    DarkWave Studio is Copyright (C) 2004-2012 Daniel Werner, http://www.experimentalscene.com/

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include <pmmintrin.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "dw_engine.h"
#include "studio.h"

namespace {

const int kBlock = 512;
darkwave::Studio* g_studio = nullptr;   // left standing when replaced: some of the machines' parts are taken down wrongly
                                        // by their own code (bbmp-darkwave leaves it too)
uint32_t g_rate = 0;
int g_passes = 1;
bool g_ready = false, g_ending = false;
long long g_tail = 0, g_silent = 0;
std::vector<double> g_left, g_right;
std::vector<int16_t> g_pcm;   // one block, interleaved
uint32_t g_pcm_pos = 0, g_pcm_len = 0;   // frames left in g_pcm

inline int16_t clip(double x) {
  if (!(x == x)) return 0;
  return x < 0.0 ? (x <= -1.0 ? -32768 : (int16_t)(x * 32768.0)) : (x >= 1.0 ? 32767 : (int16_t)(x * 32767.0));
}

void copy(char* dst, size_t n, const std::string& s) { std::snprintf(dst, n, "%s", s.c_str()); }

}  // namespace

extern "C" void dw_close(void) {
  g_studio = nullptr;
  g_ready = false;
}

extern "C" int dw_open(const uint8_t* data, size_t size, uint32_t rate, dw_info* info) {
  dw_close();
  std::memset(info, 0, sizeof *info);
  if (rate < 8000 || rate > 192000) {
    copy(info->error, sizeof info->error, "unsupported sample rate");
    return 0;
  }
  if (size == 0 || size > (16u << 20)) {
    copy(info->error, sizeof info->error, "not a DarkWave Studio project");
    return 0;
  }
  try {
    darkwave::Studio* studio = new darkwave::Studio((int)rate, kBlock);
    const std::string why = studio->Load(std::vector<uint8_t>(data, data + size));
    if (!why.empty()) {
      copy(info->error, sizeof info->error, why);
      return 0;
    }
    if (!studio->MissingMachines.empty()) {
      std::string list;
      for (auto& m : studio->MissingMachines) list += (list.empty() ? "" : ", ") + m;
      copy(info->error, sizeof info->error, "the project needs machines this player doesn't have: " + list);
      return 0;
    }
    // A project's sequence is often a bar or two that the studio's Loop button repeats; the loop is not saved in the project.
    // What is shorter than half a minute is played as many times as it takes to fill one.
    const long long once = studio->SongSamples();
    const long long enough = 30LL * rate;
    studio->Passes = once > 0 && once < enough ? (int)((enough + once - 1) / once) : 1;
    if (const char* passes = std::getenv("BBMP_DARKWAVE_PASSES"))
      studio->Passes = std::max(1, std::min(1000, std::atoi(passes)));
    g_passes = studio->Passes;
    const long long length = once * studio->Passes;
    info->duration_ms = (long)(length * 1000 / rate);
    info->once_frames = once;
    info->passes = studio->Passes;
    std::snprintf(info->format, sizeof info->format, "%d.%d", studio->MajorVersion, studio->MinorVersion);
    std::snprintf(info->tempo, sizeof info->tempo, "%g BPM, %g ticks per beat", studio->TheHost.BPM, studio->TheHost.TPB);
    if (studio->Passes > 1)
      std::snprintf(info->sequence, sizeof info->sequence, "%.1f s, played %d times (the studio's Loop)", (double)once / rate,
                    studio->Passes);
    else
      std::snprintf(info->sequence, sizeof info->sequence, "%.1f s", (double)once / rate);
    copy(info->comment, sizeof info->comment, studio->Comment);
    g_studio = studio;
    g_rate = rate;
    g_ready = false;
    return 1;
  } catch (const std::bad_alloc&) {
    copy(info->error, sizeof info->error, "the project needs more memory than there is");
    return 0;
  }
}

extern "C" int dw_start(void) {
  if (!g_studio) return 0;
  // as the studio's audio thread would: numbers too small to matter are zero
  _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
  _MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);
  g_left.assign(kBlock, 0.0);
  g_right.assign(kBlock, 0.0);
  g_pcm.assign(kBlock * 2, 0);
  // The program's machines run from the moment the project is loaded and Play is pressed later: a second of that here.
  int idle = (int)(g_rate / kBlock);
  if (const char* e = std::getenv("BBMP_DARKWAVE_IDLE")) idle = std::max(0, std::min(1 << 16, std::atoi(e)));
  g_studio->Idle(idle, g_left.data(), g_right.data());
  g_ending = false;
  g_tail = g_silent = 0;
  g_pcm_pos = g_pcm_len = 0;
  g_ready = true;
  return 1;
}

extern "C" uint32_t dw_render(int16_t* out, uint32_t frames) {
  if (!g_ready) return 0;
  const double quiet = 8.0 / 32768.0;
  const long long rate = g_rate, tailMax = 10LL * rate, fadeFrom = 7LL * rate, quietFor = rate / 2;
  uint32_t done = 0;
  while (done < frames) {
    if (g_pcm_len == 0) {
      if (g_ending && (g_tail >= tailMax || g_silent >= quietFor)) break;
      const bool playing = g_studio->Work(g_left.data(), g_right.data());
      if (!playing) g_ending = true;   // what sounds after the sequence is the end of the song
      for (int i = 0; i < kBlock; i++) {
        double l = g_left[i], r = g_right[i];
        if (g_ending) {
          const long long at = g_tail + i;
          if (at >= fadeFrom) {
            const double gain = std::max(0.0, 1.0 - (double)(at - fadeFrom) / (tailMax - fadeFrom));
            l *= gain;
            r *= gain;
          }
          if (std::fabs(l) < quiet && std::fabs(r) < quiet) ++g_silent; else g_silent = 0;
        }
        g_pcm[i * 2] = clip(l);
        g_pcm[i * 2 + 1] = clip(r);
      }
      if (g_ending) g_tail += kBlock;
      g_pcm_pos = 0;
      g_pcm_len = kBlock;
    }
    const uint32_t n = std::min(g_pcm_len, frames - done);
    std::memcpy(out + (size_t)done * 2, g_pcm.data() + (size_t)g_pcm_pos * 2, (size_t)n * 4);
    g_pcm_pos += n;
    g_pcm_len -= n;
    done += n;
  }
  return done;
}
