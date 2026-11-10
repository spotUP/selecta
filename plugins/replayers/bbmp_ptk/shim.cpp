// bbmp_ptk shim (row I710q): C entry points over ProTrekkr's own replay routine (third_party/bbmp_ptk, BSD 2-clause, from rePlayer 0.22.14).
// Written for Selecta against ProTrekkr's public replay API; bbmp's GPL wrapper (csrc/synth/ptk_engine.cpp) is not used. The replayer keeps
// its state in globals, so one song is open at a time (the adapter serialises with its own use of ptks_*).
#include "probe.h"
#include <include/version.h>
#include <replay/include/replay.h>
#include <files/include/files.h>
#include <windows.h>
#include <zlib.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>

// What the tracker's user interface would define for the replayer.
int AUDIO_Play_Flag;
char artist[20];
char style[20];
char SampleName[128][16][64];
int Midiprg[128];
char nameins[128][20];
int Chan_Midi_Prg[MAX_TRACKS];
char Chan_History_State[256][MAX_TRACKS];
int done = 0;

extern Uint8* Mod_Memory;
extern int Mod_Mem_Pos;
extern Synth_Parameters PARASynth[128];
extern char replayerPtkName[20];
Uint32 STDCALL Mixer(Uint8* Buffer, Uint32 Len);
int Alloc_Patterns_Pool(void);

#include <replay/include/endianness.h>
#if defined(__BIG_ENDIAN__)
// ProTrekkr's support/endianness.cpp, not part of the vendored replayer: the byte swaps its loader uses on a big-endian CPU (Amiga, I427e1).
Uint64 __Swap_64(Uint64 v) {
  return (Uint64)__Swap_32((int)(v & 0xffffffffu)) << 32 | (Uint32)__Swap_32((int)(v >> 32));
}
int __Swap_32(int v) {
  const Uint32 u = (Uint32)v;
  return (int)(u >> 24 | (u >> 8 & 0xff00u) | (u << 8 & 0xff0000u) | u << 24);
}
short __Swap_16(short v) {
  const Uint16 u = (Uint16)v;
  return (short)(Uint16)(u >> 8 | u << 8);
}
#endif
void Set_Default_Channels_Polyphony(void) {
  for (int i = 0; i < MAX_TRACKS; i++) Channels_Polyphony[i] = DEFAULT_POLYPHONY;
}

// Samples packed with a Windows codec (GSM 6.10, MP3, ATRAC3) are not decoded: the song is refused.
static int g_acm_used;
struct ptk_acm_stream { int tag; };
static ptk_acm_stream g_stream;
MMRESULT acmFormatSuggest(void*, LPWAVEFORMATEX, LPWAVEFORMATEX, DWORD, DWORD) { return 0; }
MMRESULT acmStreamOpen(HACMSTREAM* stream, void*, LPWAVEFORMATEX src, LPWAVEFORMATEX, void*, DWORD_PTR, DWORD_PTR, DWORD) {
  g_stream.tag = src->wFormatTag;
  if (!g_acm_used) g_acm_used = src->wFormatTag;
  *stream = &g_stream;
  return 0;
}
MMRESULT acmStreamSize(HACMSTREAM, DWORD input, DWORD* output, DWORD) { *output = input * 32 + 65536; return 0; }
MMRESULT acmStreamPrepareHeader(HACMSTREAM, ACMSTREAMHEADER*, DWORD) { return 0; }
MMRESULT acmStreamConvert(HACMSTREAM, ACMSTREAMHEADER* header, DWORD) { header->cbDstLengthUsed = 0; return 0; }
MMRESULT acmStreamUnprepareHeader(HACMSTREAM, ACMSTREAMHEADER*, DWORD) { return 0; }
MMRESULT acmStreamClose(HACMSTREAM, DWORD) { return 0; }

// From format version 9 on the loader unpacks the song into memory and reads it without looking at its end (patched to read through
// modland_read_mem, which zero-fills past the end); a sample length is a 32-bit number of the file (modland_sample_fits checks it).
static long g_mod_limit;
static bool g_damaged;

void modland_read_mem(void* to, long bytes) {
  if (bytes <= 0) return;
  long have = 0;
  if (Mod_Memory && Mod_Mem_Pos >= 0 && Mod_Mem_Pos < g_mod_limit) have = std::min<long>(bytes, g_mod_limit - Mod_Mem_Pos);
  if (have > 0) std::memcpy(to, Mod_Memory + Mod_Mem_Pos, (size_t)have);
  if (have < bytes) std::memset((char*)to + have, 0, (size_t)(bytes - have));
}

int modland_sample_fits(long length, ReplayerFile& in) {
  const long left = Mod_Memory ? g_mod_limit - Mod_Mem_Pos : (long)in.maxSize - (long)in.pos;
  if (length >= 0 && length <= left / 2) return 1;
  g_damaged = true;
  return 0;
}

// The loader unpacks with zlib and no checks and waits for ever on damaged data: unpack once here first (also checks the checksum).
static const unsigned long long kMostUnpacked = 64ull << 20;
static bool unpacks(const uint8_t* data, size_t size, char version) {
  const bool sized = version >= 'B';
  const size_t start = sized ? 13 : 9;
  if (size <= start + 8) return false;
  uint32_t expected = 0;
  if (sized) expected = (uint32_t)data[9] | (uint32_t)data[10] << 8 | (uint32_t)data[11] << 16 | (uint32_t)data[12] << 24; // little-endian in the file
  if (sized && (expected < 1024 || expected > kMostUnpacked)) return false;
  z_stream z;
  std::memset(&z, 0, sizeof z);
  if (inflateInit(&z) != Z_OK) return false;
  std::vector<uint8_t> out(1 << 16);
  z.next_in = const_cast<Bytef*>(data + start);
  z.avail_in = (uInt)(size - start);
  int result = Z_OK;
  unsigned long long total = 0;
  while (result == Z_OK) {
    z.next_out = out.data();
    z.avail_out = (uInt)out.size();
    result = inflate(&z, Z_NO_FLUSH);
    total += out.size() - z.avail_out;
    if (total > kMostUnpacked) break;
  }
  inflateEnd(&z);
  if (result != Z_STREAM_END) return false;
  if (sized && total != expected) return false;
  g_mod_limit = (long)(sized ? expected : std::min<unsigned long long>(total, (size - 9) * 10ull));
  return true;
}

// The replayer divides by the tempo and indexes tables with what the song says: reject songs with values outside the tables.
static bool plausible_song() {
  if (Beats_Per_Min < 1 || Beats_Per_Min > 999 || Ticks_Per_Beat < 1 || Ticks_Per_Beat > 32) return false;
  if (Song_Length < 1 || Song_Length > MAX_SEQUENCES) return false;
  for (int i = 0; i < Song_Length; ++i) {
    const int p = pSequence[i];
    if (p < 0 || p >= MAX_PATTERNS || patternLines[p] < 1 || patternLines[p] > MAX_ROWS) return false;
  }
  // an instrument number no instrument has (128 to 254; 255 is none) would index past the instrument tables: such notes lose it
  for (int p = 0; p < MAX_PATTERNS; ++p)
    for (int row = 0; row < MAX_ROWS; ++row)
      for (int t = 0; t < MAX_TRACKS; ++t) {
        Uint8* cell = RawPatterns + p * PATTERN_LEN + row * PATTERN_ROW_LEN + t * PATTERN_BYTES;
        for (int i = 0; i < MAX_POLYPHONY; ++i)
          if (cell[PATTERN_INSTR1 + i * 2] >= MAX_INSTRS && cell[PATTERN_INSTR1 + i * 2] != 255) cell[PATTERN_INSTR1 + i * 2] = 255;
      }
  return true;
}

static bool g_open, g_ended;

extern "C" {

// 1 on success. title gets the song name; *ntk is 1 for a NoiseTrekker song (format versions 1 and 2).
int ptks_open(const uint8_t* data, size_t size, char* title, size_t title_size, int* ntk) {
  if (!ptks_probe(data, size)) return 0;
  const char version = (char)data[7];
  *ntk = version == '1' || version == '2';
  if ((version == '9' || version >= 'A') && !unpacks(data, size, version)) return 0;
  Ptk_InitDriver();
  if (!Alloc_Patterns_Pool()) return 0;
  g_acm_used = 0;
  g_damaged = false;
  if (!Load_Ptk({data, size})) return 0;
  if (g_damaged || !plausible_song() || g_acm_used || Song_Length <= 0) return 0;
  size_t i = 0;
  for (; i < sizeof replayerPtkName && i + 1 < title_size && replayerPtkName[i]; ++i) title[i] = replayerPtkName[i];
  while (i && (title[i - 1] == ' ' || title[i - 1] == '\r' || title[i - 1] == '\n')) --i;
  title[i] = 0;
  if (*ntk) {
    // NoiseTrekker songs drive ProTrekkr's mixer far over full scale: their synth instruments have no volume (version 1) and the master
    // volume was made for another mixer. Taken down 14 dB so that the mixer's clipping stays out of most of them.
    if (version == '1') for (int k = 0; k < 128; ++k) PARASynth[k].glb_volume = 64;
    mas_vol *= 0.2f;
  }
  done = 0;
  Ptk_Play();
  g_open = true;
  g_ended = false;
  return 1;
}

// Stereo float frames at 44100 Hz; fewer than asked means the song has ended.
int ptks_render(float* out, int frames) {
  if (!g_open || g_ended) return 0;
  const int got = (int)Mixer((Uint8*)out, (Uint32)frames);
  if (got < frames) g_ended = true;
  return got;
}

void ptks_close(void) { g_open = false; }

}
