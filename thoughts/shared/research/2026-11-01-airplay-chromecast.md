---
date: 2026-11-01
topic: AirPlay (RAOP, AirPlay 2) and Chromecast output on Mac and Amiga 68060 (ledger I892; feeds I894, I895, I896, I899)
tags: [research, output-plugin, airplay, raop, chromecast, castv2, mdns, amiga, 060]
status: complete (desk research, no code, no builds; claims marked UNVERIFIED were not confirmed against a source or a measurement)
---

# AirPlay and Chromecast output

Design doc: `thoughts/shared/plans/2026-10-22-architecture-design.md`, F-row "Network outputs: Chromecast, AirPlay, UPnP (desktop)" (line 66) and the output plugin contract (line 263: `SEL_OUT_NAME`, `SEL_OUT_SINK`, `SEL_OUT_RATES`, `SEL_OUT_CHANNELS`, `SEL_OUT_LATENCY_MS`, `SEL_OUT_OPEN/CLOSE/SET_SOURCE`, pull model `sel_pcm_pull`).

## Verdict in five lines

1. The reference repo https://github.com/openairplay/open-airplay is Java/PHP/shell, a sender for photos and desktop mirroring to Apple TV (AirPlay 1 era). It is protocol reading material only; no usable C, licence not stated on the page. Do not depend on it.
2. Mac AirPlay (1 and 2, including HomePod): do NOT implement the protocol. Select the AirPlay device as the Core Audio system output (AirPlay 2 pairing and FairPlay stay inside macOS). Own code is only needed for Chromecast.
3. Amiga AirPlay: RAOP (AirPlay 1) sender, portable C, 060 first. Reaches AirPort Express, shairport-sync, Sonos (UNVERIFIED which firmware still accepts it), many speakers. AirPlay 2-only devices (HomePod) are not reachable.
4. Chromecast: CASTV2 sender plus a small HTTP server inside Selecta. One C implementation shared by Mac and Amiga. TLS is the Amiga blocker.
5. Both network outputs buffer seconds; they need `SEL_OUT_LATENCY_MS` reported honestly and a scope delay.

## 1. AirPlay

### RAOP (AirPlay 1) flow
Sources: Go/Elixir/C client write-ups ([gist](https://gist.github.com/zxjinn/5174149), [goair](https://beta.pkg.go.dev/github.com/tongxingwy/goair), [Elixir session](https://preview.hex.pm/preview/airplay/show/lib/air_play/session.ex)); the canonical unofficial spec is openairplay/airplay-spec (page not fetched, a URL guess returned 404: UNVERIFIED details below come from the client write-ups).
1. Discover `_raop._tcp` (TXT: `cn` codecs, `et` encryption types, `sr`, `ss`, `pw`, `tp`).
2. RTSP over TCP: `OPTIONS` (with `Apple-Challenge`, device answers `Apple-Response`, an RSA signature over challenge + IP + MAC), `ANNOUNCE` (SDP: `rtpmap 96 AppleLossless`, `fmtp` with frame size 352, bit depth 16, channels 2, rate 44100; `rsaaeskey` = AES-128 key RSA-OAEP-encrypted with the well-known public AirPort key, `aesiv`), `SETUP` (Transport header: client `control_port`, `timing_port`; device returns server, control, timing ports), `RECORD` (RTP-Info seq/rtptime, `Audio-Latency` returned), `SET_PARAMETER` (volume in dB -30..0, -144 = mute; progress; metadata), `FLUSH`, `TEARDOWN`.
3. Audio: RTP over UDP, one packet per 352 frames (about 8 ms at 44.1 kHz), payload ALAC, AES-128-CBC with fixed IV per packet, only whole 16-byte blocks encrypted, trailing bytes clear. Control port: sync packets about once a second (RTP time to NTP time), retransmit requests from the receiver (sender keeps a ring of recent packets and answers). Timing port: NTP-style request/response.
4. Receiver buffers about 2 s by default (shairport-sync classic: 2.0-2.25 s; AirPlay 2 mode about 0.5 s per its README). This is the latency to report.
5. Unencrypted: the Elixir client omits `rsaaeskey/aesiv` for unencrypted ALAC (UNVERIFIED which receivers accept it; shairport-sync accepting it is likely but its README does not say). Uncompressed ALAC (escape frames) needs no real compressor: header + raw PCM, so encode cost is a byte shuffle.

### Which receivers still take RAOP
- shairport-sync: classic AirPlay 1 build supported; AirPlay 2 build also exists ([README](https://github.com/mikebrady/shairport-sync)). Use the classic build as the test fixture.
- AirPort Express: yes (original target). Sonos and HomePod: UNVERIFIED, do not promise. Modern Apple TV/HomePod require pairing for RAOP since iOS 10.2 (libraop README notes pairing with Apple TV is required). Treat them as "not supported" until a real device test.

### AirPlay 2
Needs HomeKit (HAP) pairing (SRP-6a + Ed25519 + X25519 + ChaCha20-Poly1305 + HKDF; transient pairing for PIN-less devices) and, on many devices, FairPlay (v2/v3) or MFi authentication. Community projects implement the receiver side with transient pairing and FairPlay v3 ([airplay2-receiver](https://www.github.com/ckdo/airplay2-receiver)); `pair_ap` is a C pairing client under MIT/BSD ([pair_ap](https://github.com/ejurgensen/pair_ap), bundled in shairport-sync). Whether a third party can stream to a HomePod without Apple's FairPlay sender keys is UNVERIFIED (no source found); treat as unreachable. Reachable at all on Mac only through the system (see below). On Amiga: out of the question (HAP crypto set on 060 is possible, FairPlay is not).

### Licences (against Selecta, which has no LICENSE file yet)
- libraop (philippe44): MIT per its `LICENSE.txt`; README says it also needs the macosforge ALAC codec (Apache 2.0 UNVERIFIED) and OpenSSL/Curve25519 submodules. C, active (403 commits), has `-l` latency and NTP start options. Best base to read or port; the OpenSSL dependency must be replaced on Amiga (RSA once and AES-CBC are small).
- shairport-sync: MIT plus bundled third-party (alac.c BSD-style Hammerton, tinysvcmdns, pair_ap). Receiver; use only as a test fixture, not linked.
- raop_play, pyatv: reference only (pyatv is Python, MIT UNVERIFIED).
- cidermill-airplay (Rust, AirPlay 2 with HAP): GPL-3.0-or-later, not usable.

## 2. Chromecast (CASTV2)
Sources: [node-castv2](https://github.com/thibauts/node-castv2), [PROTOCOL.md](https://docs.rs/crate/gcast/0.1.5/source/PROTOCOL.md); LAUNCH/LOAD details are from memory of node-castv2-client (UNVERIFIED here).
- TLS 1.2 to port 8009, self-signed device certificate (no chain check needed for the control link; a verifier can optionally run the device-auth challenge, not required to play).
- Framing: 4-byte big-endian length + protobuf `CastMessage` (protocol_version, source_id, destination_id, namespace, payload_type STRING/BINARY, payload_utf8/binary). Only one message type plus the small `DeviceAuthMessage`; a hand-written protobuf encoder for those fields is about 150 lines, no protobuf library needed.
- Sequence: CONNECT on `urn:x-cast:com.google.cast.tp.connection` to `receiver-0`; heartbeat PING/PONG about every 5 s on `...tp.heartbeat`; `LAUNCH` appId `CC1AD845` (Default Media Receiver) on `...receiver`; wait for RECEIVER_STATUS with transportId; CONNECT to it; `LOAD` on `urn:x-cast:com.google.cast.media` with `contentId` = http URL, `contentType`, `streamType: LIVE` or BUFFERED; then PLAY/PAUSE/SEEK/volume via `SET_VOLUME` on the receiver namespace.
- The receiver pulls the URL from Selecta: a tiny HTTP/1.1 server (one connection, chunked or open-ended body). Format: WAV (PCM 44.1/16/2, 176 KB/s) is simplest, FLAC or MP3 cuts bandwidth; Default Media Receiver plays LPCM WAV UNVERIFIED (MP3/AAC/FLAC/WAV are on Google's supported list from memory). On Amiga serve PCM WAV (no encoder CPU).
- Buffering: receiver prebuffers; expect 2-5 s, not controllable (UNVERIFIED, measure). Seek and pause are transport-level; Selecta cannot sample-sync to it. Report measured latency; scope delay line.
- Licence: no C library worth linking found in this pass; write ours (MIT-compatible by construction). Chromium `cast_channel.proto` is BSD-3 for reading the schema.

## 3. Discovery (shared with I894)
- Services: `_raop._tcp` (AirPlay 1 audio, name `MAC@Name`), `_airplay._tcp` (AirPlay 2/video), `_googlecast._tcp` (TXT `id`, `fn` friendly name, `md` model, `ca` capabilities).
- Mac: `dns_sd.h` (`DNSServiceBrowse` + `DNSServiceResolve` + `DNSServiceGetAddrInfo`), system library, no licence issue; same call set serves I894's `_smb._tcp`, `_webdav._tcp`, `_daap._tcp`. Plug into the host as one `sel_host.discover` service, not per plugin.
- Amiga: no mDNS responder stack is needed for a sender. Build one 12-byte DNS header + PTR question for `_raop._tcp.local`, send as UDP to 224.0.0.251:5353 with the QU bit (unicast response wanted) from an ephemeral port, read PTR/SRV/TXT/A records from the replies with a small name-decompression parser (about 300 lines C). Requires `IP_ADD_MEMBERSHIP` only if listening on 5353; unicast-reply mode avoids it (UNVERIFIED that every Amiga stack, Roadshow vs Miami, supports the multicast send; test on rig). Fallback: manual IP pref. `tinysvcmdns` (in shairport-sync, BSD-style) is a responder, not needed.

## 4. Amiga 060 cost (all estimates, UNVERIFIED until measured per I892 note)
Data rate: 44.1 kHz s16 stereo = 176.4 KB/s, 125 packets/s of 1408 bytes payload.
- Uncompressed ALAC: copy and byte-swap, trivial on every CPU. A real ALAC compressor is optional and not worth it on Amiga.
- AES-128-CBC: 176 KB/s = 11 K blocks/s. A plain C table AES on 68060 at 50 MHz is roughly 50-100 cycles/byte-class (about 1-2 MB/s?) giving 10-20% CPU; the DCTelnet `src/crypto/aes` is already 68k-tuned (I899 survey) and may do better. On 68020/030 (about 14-25 MHz, no cache-friendly tables) it is likely 4-8x slower and takes most of the CPU on top of the mix: UNVERIFIED, measure. Hence: encrypted RAOP is 060 only.
- RSA: one 2048-bit public-exponent (e = 65537) operation per session for the AES key, plus one private-key-free Apple-Response check (device signs; sender only verifies, optional and skippable). Public op is 17 modular squarings, milliseconds on 060, under a second on 020 even without asm. Reuse DCTelnet `rsa/bignum` (68k asm `bn_muladd.s`).
- RTSP/UDP/mDNS: negligible beyond bsdsocket.library call overhead.
- Chromecast TLS: Amiga build is `SEL_NO_TLS` (src/library/CMakeLists.txt:38, src/library/net/tls.c). Options: (a) AmiSSL port (OpenSSL, Apache 2.0 for 3.x; needs AmiSSL installed by the user, 020+ works, handshake ECDHE-RSA seconds on 020, sub-second on 060); (b) new minimal TLS 1.2 client on the DCTelnet primitives (aes, sha256, x25519, rsa, bignum) which needs AES-GCM (GHASH) or ChaCha20-Poly1305, HMAC/HKDF, ECDSA/RSA parse, and X.509 skipping. Because the Cast certificate is self-signed and we skip verification, option (b) is smaller than a general TLS, and the stream itself is HTTP (no TLS) so only about 1 KB/s of control traffic goes through it. Decision belongs to I899; this note recommends (b) only if I899 measures it; else (a).
- What runs on 020: mDNS discovery, RTSP, unencrypted RAOP (if the receiver accepts it), HTTP server for Cast. What does not: encrypted RAOP (AES per packet) and Cast TLS handshake in a reasonable time without AmiSSL. Cast control after the handshake is trivial.
- Audio source: native Paula players give no PCM; PCM comes from the I888n/nuade render path. That render path's cost is added on top of the above; budget on the rig.

## 5. Where it plugs in
Repo: `include/selecta/output.h` (pull model: output pulls interleaved s16 stereo through `sel_pcm_pull`), tags in `include/selecta/tags.def` lines about 95-125 (`SEL_OUT_*`: `LATENCY_MS`, `RATE_FOR`, `CLOCKLESS`, `UNPACED`, stats tags). Existing outputs under `plugins/outputs/`: ahi, coreaudio, null, paula, paula14, voices_test, wav. Engine entry: `include/selecta/engine.h` `sel_engine_set_output`, output swap (I532) for voice-only songs, so every format reaches a PCM network sink through the mixer.
- Fit: a network output is a normal PCM sink. It has its own clock only through the network, so it is closest to `wav`/`null` (CLOCKLESS pumped) or to a thread that paces on a monotonic timer; recommend an own sender thread (Mac) and the shell-paced pump (Amiga, like `UNPACED`).
- Latency: `SEL_OUT_LATENCY_MS` is a static tag today (coreaudio says 20). Network latency is 2000+ ms and known only after RECORD (`Audio-Latency`) or by measurement. Needs either a runtime query (new tag `SEL_OUT_LATENCY_QUERY`, PTR) or setting the tag at open; the viz/scope path must delay scope frames by it. Proposed in row A.
- Volume: RAOP `SET_PARAMETER volume` and Cast `SET_VOLUME` are device volume, so these outputs should set `SEL_OUT_DIRECT_VOLUME` semantics (F31) and route the volume command to the device; software gain stays 1.0.
- Transport: pause = stop pumping + `FLUSH` (RAOP) or PAUSE (Cast); seek = FLUSH + new RTP start. Gapless inside a session is free with RAOP (continuous RTP), not with Cast (each track a LOAD, or one endless stream URL: preferred, the stream never ends).
- Output swap (I532) applies: the AirPlay/Cast sinks declare `SEL_SINK_PCM` only.
- Mac AirPlay via the system: coreaudio output gets a device chooser that lists output devices including AirPlay ones (Core Audio exposes AirPlay routes as devices after the user picks them in system routing; UNVERIFIED that a device list alone shows unconnected AirPlay speakers without AVRoutePickerView). Test on Mac before relying on it; fallback is a RAOP sender on Mac too (same C code as Amiga).

## Recommendation per platform
- Mac: AirPlay through system audio routing (an AVRoutePickerView button in the desktop UI, coreaudio plugin unchanged except latency query). Chromecast: shared C CASTV2 sender with TLS from the system (SecureTransport/Network.framework or the existing Mac TLS in `tls.c`) and the shared HTTP server. If the system route proves unusable, fall back to the shared RAOP sender.
- Amiga 060: RAOP sender (encrypted, uncompressed ALAC), Chromecast after I899 decides TLS. 020/030: unencrypted RAOP only, later, if measurements allow; Cast not.
- AirPlay 2-only (HomePod) from Amiga: not supported, state it in the UI.

## Risks
- Receivers vary: RAOP acceptance on current Sonos/HomePod is unverified, only AirPort Express and shairport-sync are safe bets. Demand a real-device matrix from Johan.
- Latency of 2-5 s breaks scope/oscilloscope sync and gapless feel; needs the delay line and a UI label ("network output delayed").
- Cast TLS on Amiga is the largest unknown (I899).
- Network-card throughput on the rig (1.4 Mbit/s is small) and UDP resend jitter under an 060 that also renders nuade: measure.
- ALAC licence of the macosforge codec only matters if we ship a real compressor; uncompressed frames avoid it.
- Selecta has no LICENSE file; MIT code (libraop, pair_ap) is compatible with any choice, GPL (cidermill, some shairport pieces) is not.

## Proposed row split
- A. Output latency query + scope delay + device-volume routing in the engine/viz (needed by all below; host test with the null output reporting 2000 ms).
- B. Shared discovery service: `sel_host.discover` browse API; Mac `dns_sd` backend; Amiga hand-made multicast query backend (shared with I894, host test against a fake responder).
- C. Portable RAOP sender core (RTSP, SDP, RSA key wrap, AES-CBC, uncompressed ALAC, control/timing/resend), host test vs a shairport-sync classic fixture comparing received PCM md5; sentinel for sent packets.
- D. Mac: AirPlay system-route selection in coreaudio output + UI button; real-device check by Johan.
- E. Portable CASTV2 sender core + embedded HTTP stream server (WAV), host test vs a CASTV2 fixture; Mac TLS backend.
- F. Amiga RAOP output plugin (existing I895, depends on A, B, C and I888n); AES speed and net throughput measurement first.
- G. Amiga Cast output plugin (existing I896, depends on A, B, E, I899, I888n).
- H. Measurement row: AES-CBC MB/s, RSA time, UDP throughput on rig 060 and 020 (feeds F and the 020 question); can run first.

## Open questions for Johan
1. Mac AirPlay: accept system routing (AirPlay 2 and HomePod work, no protocol code) instead of an own sender?
2. Which real receivers do you own (AirPort Express, Sonos, HomePod, Chromecast model)? That sets the test matrix.
3. Amiga Cast TLS: AmiSSL (user installs it) or new TLS on DCTelnet primitives (I899)?

## Decisions (Johan, 2026-11-01)

- Mac AirPlay uses macOS system audio routing. Selecta writes no AirPlay sender for the Mac.
- Test receivers: a HomePod and an Apple TV, both AirPlay 2. Whether either accepts the Amiga RAOP (AirPlay 1) sender is unverified and is the first thing row H measures.
- No Chromecast device is available, so the Cast rows are tested against a local fixture receiver only.
- The TLS layer for the Amiga Cast control link is chosen by the I899 comparison.
