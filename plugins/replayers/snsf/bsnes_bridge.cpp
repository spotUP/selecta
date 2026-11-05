// bsnes embedding for the SNSF replayer (I705a). GPL-3.0-or-later (links the vendored bsnes core). Headless: no video,
// no input, the Platform hands the core an in-memory cartridge and a hand-built board table (LoROM or HiROM) instead
// of boards.bml, and a clean-room 64 byte boot ROM for the SPC700 (the Nintendo IPL is copyrighted and not shipped).
#include <sfc/sfc.hpp>
#include "bsnes_bridge.h"
#include <vector>
#include <cstring>

namespace {

// Clean-room SPC700 boot program, mapped at $FFC0. Implements the published port protocol: write $AA/$BB, wait for
// $CC, then blocks of (destination, count of bytes echoed through port 0); a block with port 1 = 0 is a jump.
const uint8_t kIpl[64] = {
  0xCD,0xEF, 0xBD, 0xE8,0xAA, 0xC4,0xF4, 0xE8,0xBB, 0xC4,0xF5, 0xE8,0xCC, 0x64,0xF4, 0xD0,0xFC,
  0xE4,0xF6, 0xEB,0xF7, 0xDA,0x00, 0xF8,0xF4, 0xE4,0xF5, 0xD8,0xF4, 0xF0,0x19, 0x8D,0x00,
  0x3E,0xF4, 0xF0,0xFC, 0x7E,0xF4, 0xD0,0xE8, 0xE4,0xF5, 0xD7,0x00, 0xCB,0xF4, 0xDD, 0x5D, 0xFC,
  0xD0,0xED, 0xAB,0x01, 0x2F,0xE9, 0x5D, 0x1F,0x00,0x00, 0x00,0x00, 0xC0,0xFF,
};

const char *kBoards =
  "board: LOROM\n"
  "  memory type=ROM content=Program\n"
  "    map address=00-7d,80-ff:8000-ffff mask=0x8000\n"
  "  memory type=RAM content=Save\n"
  "    map address=70-7d,f0-ff:0000-7fff\n"
  "board: HIROM\n"
  "  memory type=ROM content=Program\n"
  "    map address=00-3f,80-bf:8000-ffff\n"
  "    map address=40-7d,c0-ff:0000-ffff\n"
  "  memory type=RAM content=Save\n"
  "    map address=20-3f,a0-bf:6000-7fff\n"
  "    map address=70-7d:0000-7fff\n";

// Score a header at `base`: checksum pair, reset vector in the upper half, map mode low bit matching the layout.
int headerScore(const uint8_t *rom, size_t size, size_t base, bool hi) {
  if(size < base + 0x40) return -1;
  const uint8_t *h = rom + base;
  int score = 0;
  unsigned comp = h[0x1c] | h[0x1d] << 8, sum = h[0x1e] | h[0x1f] << 8;
  if((comp ^ sum) == 0xffff) score += 4;
  unsigned reset = h[0x3c] | h[0x3d] << 8;
  if(reset >= 0x8000) score += 2;
  if(((h[0x15] & 1) != 0) == hi) score += 1;
  return score;
}

struct Host : Emulator::Platform {
  std::vector<uint8_t> rom, sram;
  std::string manifest;
  std::vector<int16_t> queue;

  auto path(uint) -> string override { return ""; }
  auto load(uint id, string, string, vector<string>) -> Load override {
    if(id == SuperFamicom::ID::SuperFamicom) return Load(id, "NTSC");
    return {};
  }
  auto open(uint id, string name, vfs::file::mode, bool) -> shared_pointer<vfs::file> override {
    if(id == SuperFamicom::ID::System) {
      if(name == "ipl.rom") return vfs::memory::file::open(kIpl, sizeof kIpl);
      if(name == "boards.bml") return vfs::memory::file::open((const uint8_t*)kBoards, strlen(kBoards));
    } else if(id == SuperFamicom::ID::SuperFamicom) {
      if(name == "manifest.bml") return vfs::memory::file::open((const uint8_t*)manifest.data(), manifest.size());
      if(name == "program.rom") return vfs::memory::file::open(rom.data(), rom.size());
      if(name == "save.ram" && !sram.empty()) return vfs::memory::file::open(sram.data(), sram.size());
    }
    return {};
  }
  auto audioFrame(const double *samples, uint channels) -> void override {
    for(uint c = 0; c < 2; c++) {
      double v = samples[c < channels ? c : 0] * 32767.0;
      queue.push_back((int16_t)(v > 32767.0 ? 32767.0 : v < -32768.0 ? -32768.0 : v));
    }
  }
};

Host *host = nullptr;
SuperFamicom::Interface *iface = nullptr;
size_t queuePos = 0;

}

extern "C" int snsf_bridge_open(const uint8_t *rom, size_t rom_size, const uint8_t *sram, size_t sram_size, double rate) {
  snsf_bridge_close();
  if(!rom || rom_size < 0x8000) return -1;
  // Copiers add a 512 byte header; the SNSF ROM image has none, but tolerate it.
  size_t skip = (rom_size & 0x7fff) == 0x200 ? 0x200 : 0;
  rom += skip; rom_size -= skip;
  int lo = headerScore(rom, rom_size, 0x7fc0, false), hi = headerScore(rom, rom_size, 0xffc0, true);
  bool hirom = hi > lo;
  host = new Host;
  host->rom.assign(rom, rom + rom_size);
  if(sram && sram_size) host->sram.assign(sram, sram + sram_size);
  string m;
  m.append("game\n  board: ", hirom ? "HIROM" : "LOROM", "\n");
  m.append("    memory type=ROM content=Program size=0x", hex(rom_size), "\n");
  m.append("    memory type=RAM content=Save size=0x8000\n");
  host->manifest = (const char*)m;
  platform = host;
  iface = new SuperFamicom::Interface;
  if(!iface->load()) { snsf_bridge_close(); return -2; }
  iface->configure("Hacks/Entropy", "None");
  iface->configure("Hacks/PPU/Fast", "true");
  Emulator::audio.setFrequency(rate);
  iface->power();
  return 0;
}

extern "C" size_t snsf_bridge_render(int16_t *out, size_t frames) {
  size_t want = frames * 2;
  while(host && host->queue.size() - queuePos < want) iface->run();
  if(!host) { memset(out, 0, want * 2); return frames; }
  memcpy(out, host->queue.data() + queuePos, want * 2);
  queuePos += want;
  if(queuePos > (1u << 16)) { host->queue.erase(host->queue.begin(), host->queue.begin() + queuePos); queuePos = 0; }
  return frames;
}

extern "C" void snsf_bridge_close(void) {
  if(iface) { iface->unload(); delete iface; iface = nullptr; }
  platform = nullptr;
  delete host; host = nullptr; queuePos = 0;
}
