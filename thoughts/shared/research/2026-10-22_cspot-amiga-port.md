---
date: 2026-10-22
topic: Feasibility of porting cspot (Spotify Connect, ESP32) to AmigaOS 3.x as a native Selecta source plugin
tags: [selecta, spotify, cspot, amiga, 68k, tremor, sp2, source-plugin]
status: done
---

# cspot on AmigaOS 3.x (row SP2)

Sources: I cloned github.com/feelfreelinux/cspot (master) and github.com/feelfreelinux/bell (develop, the platform library cspot depends on) and read the code, plus the GitHub API for issues and dates. Design sections 5, 9 and 10 were read for fit (source = `sel_source` + PCM source/decoder plugin, one exported `sel_plugin_entry`).

## Verdict

Feasible with conditions. Not a port of the code as it stands; a port of the protocol layer (about 17 cspot files) onto a new small platform layer, with a rewritten C++ subset, a 32-bit-multiply Tremor build, and a licence decision. Effort estimate: 8 to 14 weeks for one developer to reach "plays a Spotify track on a 68060/50", most of it platform work and debugging against a live Spotify, not the Spotify protocol itself.

Top blocker: licence plus unmaintained upstream. cspot is GPL-3.0-or-later and its platform library bell has no licence file at all. Second: the toolchain (gcc 6.5 libstdc++ lacks C++17 library types and the code is built as C++20).

## 1. Licences

| Component | Licence | Source |
|---|---|---|
| cspot | GPL-3.0-or-later (header in LICENSE.md: "either version 3 of the License, or any later version"). GitHub API reports NOASSERTION because the file is non-standard. | https://github.com/feelfreelinux/cspot/blob/master/LICENSE.md |
| bell (platform lib, required) | No LICENSE file in the repo, GitHub reports none. Issue #184 (Aug 2026, closed) complained about "unlicensed bell". Treat as all-rights-reserved until the author clarifies. | https://github.com/feelfreelinux/bell ; https://github.com/feelfreelinux/cspot/issues/184 |
| Tremor (bell/external/tremor) | Xiph.org BSD-3-clause style, "Copyright (c) 2002, Xiph.org Foundation" | COPYING in bell/external/tremor |
| nanopb | zlib-style ("provided as-is ... permission is granted to anyone") | bell/external/nanopb/LICENSE.txt |
| mbedTLS | not vendored in bell; taken from ESP-IDF or `BELL_EXTERNAL_MBEDTLS_DIR`. Upstream licence is Apache-2.0 or GPL-2.0-or-later (unverified, from memory; check https://github.com/Mbed-TLS/mbedtls) | bell/CMakeLists.txt lines 115-147 |
| Others in bell/external | cJSON MIT, fmt MIT, alac Apache-2.0, opus BSD, libhelix-mp3 RealNetworks RPSL/RCSL (restrictive, not needed for Spotify), civetweb MIT (unverified), nlohmann_json MIT and mdnssvc (submodules, not checked out; licences unverified) | LICENSE files in each directory |

Consequence: a GPL-3 plugin loaded as a separate binary is acceptable only if Selecta's own licence is GPL-compatible; the design should state this. The bell gap is solvable by not using bell (see 4).

## 2. Maintenance

- cspot last commit: 2024-07-12 ("Update README.md (#170)"). Repo last pushed 2025-09-30 (metadata only). No release tags. Repo is not archived. Source: GitHub API repos/feelfreelinux/cspot and commits.
- bell last commit on develop: 2024-12-23 ("Close TCP socket on read error"). Source: clone of bell.
- Open issues: 39. Relevant: #181 (2026-03-15) "library status" asking whether it still works against Spotify, no answer; #180 (2026-03-14) "Cannot fetch CDN Url" (the failure on the playback path, no fix); #179 (2026-01-02) "New Spotify API restrictions"; #177 (2024-12-17) "Spotify authorization declined when providing username and password". https://github.com/feelfreelinux/cspot/issues
- login5: cspot already uses it, `AccessKeyFetcher.cpp` posts to https://login5.spotify.com/v3/login with `protobuf/login5.pb.h`. So the 2024 move away from keymaster did not break it by design. I found no issue titled as login5 breakage.
- Aug 2026 outage: librespot issue #1737 ("play.rs example INVALID_CREDENTIALS since 2026-08-10", open) is about logging in with an externally supplied access token; the thread says Spotify closed old access keys ahead of the "Soloist" release, and workarounds were the device-pair flow (#1745) and desktop client ID flow (#1744). https://github.com/librespot-org/librespot/issues/1737 . cspot does not log in with an external token (it uses Zeroconf/blob credentials then AP auth plus login5), so it may not be hit by the same path, but nothing shows cspot was tested after 10 Aug 2026. Status: unverified. Nobody has reported cspot working or broken since; #181 is open and unanswered.
- Net: treat cspot as unmaintained. We would own it, and every Spotify protocol change.

## 3. C++ standard and features

- CMake sets C++20: `set(CMAKE_CXX_STANDARD 20)` in bell/CMakeLists.txt:78, cspot/cspot/CMakeLists.txt:9, targets/cli/CMakeLists.txt:19. No -std flag beyond that.
- Counts across bell/main, cspot/cspot, targets (grep of `std::` names): shared_ptr 110, unique_ptr 102, make_unique 54, scoped_lock 46 (C++17), mutex 42, atomic 26, function 22, string_view 15 (C++17), unique_lock 7, std::regex 5, chrono (about 15), variant 2 (C++17), optional 2 (C++17), condition_variable 1, random_device 1. No std::filesystem, no std::any, no coroutines or concepts found.
- bebbo m68k-amigaos-gcc 6.5.0b: libstdc++ 6 has no string_view, scoped_lock, variant or optional (these arrived in gcc 7); C++14 language is fine. Also std::thread/mutex/condition_variable need gthreads, which the Amiga targets do not provide (unverified for the exact bebbo build; check `__GTHREADS`). std::regex works but is large and slow.
- Fix path: the 5 hot spots are mechanical (scoped_lock -> lock_guard, string_view -> const char*+len or a 30-line local class, variant/optional -> 2 local uses). A newer bebbo gcc (13.x exists in the amiga-gcc repo, unverified for our install) would remove most of it, but the design pins 6.5 (section 12). Cost: about 1 week.

## 4. Platform surface

Platform abstraction lives in bell, not cspot: `bell/main/platform/{apple,esp,linux,win32}` hold only MDNS and a WrappedSemaphore; the rest is shared code that assumes POSIX:

- Sockets: `bell/main/io` (TCPSocket, TLSSocket, UDPSocket, SocketStream, HTTPClient) use BSD sockets, `fcntl`, `select`/`poll`. Maps to bsdsocket.library (already needed by the design's HTTP client, section 16). `ioctlsocket`/`IoctlSocket` differs; no poll() on bsdsocket (use WaitSelect).
- Threads: BellTask.h uses `pthread_create`/`pthread_detach` (esp_pthread on ESP32); cspot has a TrackPlayer thread, a Mercury session reader and a queue task, about 3 to 4 threads. Amiga: write a thin task wrapper over exec `CreateNewProc`, mutex over SignalSemaphore, condition variable over signals.
- Mutex/atomics: std::mutex 42 uses, std::atomic 26. On 68k a plain 32-bit atomic is fine with Forbid/Permit or a spin on `CAS`; 68000/010 have no CAS (68020+ do), acceptable since target is 68020+.
- Timers: std::chrono system_clock and steady_clock; replace with timer.device; clock epoch differs (Amiga 1978) so the Unix-time-dependent fields (Spotify timestamps) need an offset.
- Randomness: one `std::random_device` use plus mbedTLS DRBG for the 768-bit Diffie-Hellman key; seed from timer + VHPOSR/CIA noise, weak, so mix in network and input timing. Security note, not a blocker.
- File storage: credentials blob only (cspot's CLI writes a JSON file). Trivial on AmigaDOS.
- mDNS/Zeroconf (so the phone sees the Amiga as a device): bell uses mdnssvc; needs UDP multicast on bsdsocket (Roadshow/Miami support varies, unverified). Fallback: log in with a stored blob captured once on a desktop, then use Spotify Connect "transfer playback" via the Spotify app, which still needs Zeroconf or a dealer connection. Without mDNS the user cannot select the Amiga as a Connect target, a UX gap.
- Crypto: Diffie-Hellman 768-bit, RSA signature verification of the AP server key, Shannon stream cipher (small, own code), AES-128-CTR for audio decrypt (bell/main/utilities/aes.c is plain C, fine), SHA-1, HMAC, PBKDF2, TLS 1.2 to login5/spclient/CDN. TLS cost: mbedTLS or AmiSSL; AmiSSL needs OS 3.2-era setups and AmiSSL 5 requires 68020+ (unverified); simplest is mbedTLS compiled into the plugin. Handshake ECDHE/RSA on a 68060 is about 1 to 3 s (estimate), fine for one connection per track.
- Protobuf: nanopb, pure C, builds anywhere.
- Rough port shape: keep cspot/src (17 files), replace bell with about 1500 to 2500 lines of Amiga platform code (sockets, tasks, locks, time, HTTP/1.1 + chunked, JSON). Dropping bell also removes the licence gap, but then cspot's use of bell types (Crypto, BinaryReader, HTTPClient, BellTask, CircularBuffer) must be re-implemented or substituted; cspot includes bell headers throughout (unverified count; do a header grep before planning).

## 5. Memory

- README for the ESP32 target: "typical ESP32 RAM size is 160-520kb, which is not enough to run CSpot. ... select a ESP32 with PSRAM", WROVER confirmed, WROOM not. https://github.com/feelfreelinux/cspot/blob/master/targets/esp32/README.md
- BellTask defaults `runOnPSRAM = true` for task stacks (bell/main/utilities/include/BellTask.h:26-28), so thread stacks live in PSRAM.
- Fixed buffers in CDNAudioFile.h: HTTP buffer 14 KB, Opus/Vorbis header buffer 8 KB, seek margin 4 KB. Track player PCM ring buffers and the codec/TLS state are the rest.
- I found no measured heap figure in the repo (unverified). Estimate for the Amiga port: 2 to 4 MB of Fast RAM peak (TLS 2 x 40 KB, Tremor setup tables about 100 to 300 KB for 44.1 kHz stereo, 14 KB HTTP, PCM ring about 256 KB, stacks 4 x 32 KB, protobuf/JSON working sets). That fits any 68060 machine; it does not fit a 2 MB 68020.

## 6. Tremor speed on 68k

Measured data found: essentially none.
- Aminet VorbisPlayer (Jools "BuZz" Smyth): command-line Ogg player "based on Tremor ... low quality mode", 68060 only, "realtime playback for you 68060 users (just about anyway)", not for multitasking. https://aminet.net/mus/play/VorbisPlayer.readme (no clock, bitrate or CPU figure).
- Aminet oggv_dec 1.0 (Lorence Lombardo, 2013), m68k-amigaos, based on VorbisPlayer source, Tremor; "approximately 4 times faster than my fastest oggdec" (relative, CPU not named); FPU builds gave little gain; LQ builds "not much faster". https://se.aminet.net/mus/play/oggv_dec.readme
- No Vampire or Emu68 Tremor benchmark found. Xiph's hardware wiki, which may hold ARM MIPS figures, failed to load (expired certificate): https://wiki.xiph.org/VorbisHardware (unverified).
- For scale: AmigaAMP on a 68060/50 with AHI used 42 to 47% CPU for 44.1 kHz stereo MP3 (bitrate unstated); the FAQ says full-quality Layer 3 needs at least a 68060/50. https://amigaamp.de/AHIresults.shtml , https://amigaamp.de/faq.shtml

Key technical point (from my knowledge of the 68060, not read in a source this session; verify against the MC68060 user manual): the 68060 does not implement the 32x32 to 64-bit MULS.L/MULU.L forms; they trap to the 68060.library software emulation, which is slow. Full-accuracy Tremor uses 64-bit products (MULT32/MULT31) throughout, so on a real 060 it needs the low-accuracy build, which uses 32-bit multiplies. That is almost certainly why VorbisPlayer is "low quality mode" and why it is "just about" realtime. Vampire 68080 and Emu68 (JIT to ARM) execute the 64-bit multiply natively, so the full-accuracy build is fine there.

Estimates (labelled estimate; no direct measurement, derived from VorbisPlayer's "just about realtime" on 060 plus typical Tremor cost of roughly 25 to 40 MIPS for 128 to 160 kbps stereo on ARM-class cores, which is itself unverified):
- 68060/50, low-accuracy Tremor, 160 kbps (Spotify "high"): about 70 to 100% of one CPU. Marginal; leaves nothing for UI, AES decrypt (about 20 KB/s at 160 kbps, trivial) and the TLS/CDN stream. Not comfortable.
- 68060/50, 320 kbps (Spotify "very high"): about 1.3 to 1.8 times realtime needed, i.e. not realtime. Only 96 kbps and 160 kbps are realistic; 96 kbps (about 60 to 70% CPU, estimate) is the safe tier.
- Vampire 68080 (roughly 3 to 4 times an 060/50 per clock in integer code, estimate): 160 kbps about 20 to 30% CPU, 320 kbps about 40 to 60%.
- Emu68 on Pi 4 class hardware: JIT at roughly 1 GHz-class equivalent, 160 and 320 kbps both under 10 to 15% CPU (estimate).
- Mitigation: decode at half rate is not possible with Vorbis; but Tremor's "lowmem"/"LQ" and a hand-written 68k asm MDCT/windowing (the hot loops are `mdct.c` and `floor1`/residue) could win 1.3 to 1.5 times (estimate). Plan the SP2 spike to measure on the real 060 before committing.

## 7. Verdict and effort

Feasible with conditions:
1. Fast 68k only: Vampire and Emu68 comfortably; 68060/50 only at 96 and 160 kbps, with low-accuracy Tremor, and no headroom (estimate).
2. A licence decision: GPL-3 plugin (needs Selecta's licence to allow it) and a bell replacement.
3. Accept that upstream is dormant and may already be broken after Aug 2026; a desktop test of cspot against live Spotify (about 1 day) must come first and gate everything.
4. Premium account required for the device (same as any Connect client).

Effort estimate (weeks, one developer, Amiga and Spotify access assumed):
- 1 day: build cspot CLI on a desktop, confirm it still logs in and plays today. Gate.
- 1 week: C++14 subset port plus build with bebbo 6.5 (host-side libs mbedTLS, nanopb, Tremor LQ).
- 2 to 3 weeks: Amiga platform layer (bsdsocket, tasks, locks, time, HTTP/TLS), no mDNS.
- 1 to 2 weeks: Zeroconf/mDNS or a different login route; Connect device visibility.
- 1 to 2 weeks: Tremor LQ tuning on a real 060, PCM source plugin wiring, buffering.
- 2 to 4 weeks: debugging against live Spotify, protocol drift, reachability test per the project rule.
Total 8 to 14 weeks (estimate). The proxy route in the Decision section (desktop or Pi sidecar streaming PCM) covers slow Amigas and also any fast Amiga if the gate in step 1 fails.

## Open items for the ledger

- Run the desktop gate test (cspot CLI login and playback on Linux/macOS today).
- Measure Tremor LQ on a real 68060/50 with a 160 kbps and a 96 kbps Vorbis file (VorbisPlayer from Aminet is the quickest baseline).
- Ask the cspot maintainers (issue #184 thread) for a bell licence.
- Check `-std=c++14` compile of cspot/src with bebbo gcc 6.5 after the 5 library substitutions; check `__GTHREADS`.
