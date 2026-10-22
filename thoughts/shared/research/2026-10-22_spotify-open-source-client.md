---
date: 2026-10-22
status: done
tags: [research, spotify, source-plugin]
---

# Real Spotify playback through an open-source client

Question: can Selecta stream real Spotify audio into its own engine via an open-source client library? Budget-limited research (about 12 tool calls); items marked (unverified) were not confirmed from a primary page.

## Verdict

Feasible on desktop, fragile by nature, impossible natively on 68020/AGA. Recommended shape: a sidecar (go-librespot or librespot) writing PCM to a named pipe, wrapped by a SEL_PLUGIN_KIND_SOURCE plugin that speaks the sidecar's control API and exposes the pipe as a PCM stream.

## Candidate table

| Project | Language | Licence | Maintenance | PCM we can pull | Connect | Auth today |
|---|---|---|---|---|---|---|
| librespot | Rust | MIT (https://github.com/librespot-org/librespot) | v0.8.0 released 10 Nov (2025 per moOde report); v0.7.0 24 Aug, v0.7.1 31 Aug (https://github.com/librespot-org/librespot/releases) | pipe and subprocess backends (same releases page); Symphonia decoder since 0.5.0 | yes, receiver | OAuth, zeroconf, device-style tokens; --password errors out (https://forge.k3s.fr/github-mirrors/librespot/src/commit/4f9151c6429e314c5916fdcd7f75ec5478624652) |
| go-librespot | Go | GPL-3.0 | active (release date not shown on page) (unverified) | named pipe, s16le/s32le/f32le; REST + WebSocket API on port 3678 (https://github.com/devgianlu/go-librespot) | yes | zeroconf, interactive OAuth, device pairing code (spotify.com/pair), access token |
| spotifyd | Rust (librespot based) | GPL-3.0 (https://github.com/Spotifyd/spotifyd) | 922 commits, 60 open issues; date unverified | pipe backend (unverified, inherited from librespot) | yes, daemon | Premium required; method unverified |
| raspotify | packaging of librespot | (unverified) | (unverified) | via librespot | yes | via librespot |
| ncspot, spotify-player | Rust TUIs on librespot | (unverified) | ncspot moved to OAuth with librespot 0.5.0 (https://github.com/hrkfdn/ncspot/releases) | not designed for PCM export | player only | OAuth |
| libspotify | C | proprietary | dead: disabled 16 May 2022 (https://developer.spotify.com/blog/2022-04-12-libspotify-sunset) | n/a | n/a | n/a |
| librespot C FFI | - | - | none found; librespot README mentions no C bindings (https://github.com/librespot-org/librespot) | - | - | - |

All of them require Spotify Premium (librespot README, go-librespot README, spotifyd README above). Spotify's own sunset notice points to the Web Playback SDK (browser only) as the supported streaming path.

## Spotify-side breakage risk

- Password login removed in the 2024 OAuth2 change (commit link above). Confirmed.
- 2024: login5 introduced for access tokens (librespot 0.6.0, Oct; PR #1344 merged 19 Oct 2024).
- Nov 2025: Spotify backend change broke Connect, fixed in librespot 0.8.0 (https://moodeaudio.org/forum/showthread.php?pid=6950).
- 10 Aug 2026: login5 INVALID_CREDENTIALS, playback fails while the Web API still works. Issue #1737 opened 13 Aug 2026 and still open when checked; maintainer says Spotify closed private endpoints used for playback. Workarounds are in unmerged-or-unknown PRs #1744 (desktop client ID) and #1745 (device authorization flow); the device pair flow works for the reporter (https://github.com/librespot-org/librespot/issues/1737). Today is 22 Oct 2026, so this class of breakage is current, and the fix lag is at least 10 weeks for the upstream release (unverified whether a release has since landed).
- Terms: the librespot README itself says use is "probably forbidden" by Spotify. Spotify developer terms only permit streaming apps via the Spotify Platform on approved devices, forbid reverse engineering the Platform, and forbid stream ripping (https://developer.spotify.com/terms). Account-ban risk is real but not quantified (unverified).
- Web API changes 27 Nov 2024: new apps lost audio features, audio analysis, recommendations, related artists, 30 s previews (https://www.digitalmusicnews.com/?p=308667). The Web API provides metadata and Connect control only; the libspotify sunset notice directs audio streaming to the Web Playback SDK. Web API alone cannot deliver audio: consistent with the sources, no page states it in one sentence.

## Integration shapes, ranked

1. (b) Sidecar (recommended). Run go-librespot (named pipe, REST/WebSocket control, pairing-code login suits our UI) or librespot (pipe backend). The source plugin spawns/supervises it, reads PCM from the pipe, and feeds the engine as a replayer-style stream. Works on desktop. Isolates Rust/Go, TLS and Spotify churn from our C99 core; swapping or patching the client does not touch our ABI. Pipe caveat: go-librespot fails if no reader is connected unless audio_output_pipe_wait_for_reader is true. Licence: GPL-3.0 sidecar is a separate process, so it does not infect our code; ship it as an optional download. librespot (MIT) is the licence-cleaner alternative; choose go-librespot first for the control API and device pairing, librespot as fallback.
2. (a) Link librespot as a library via C FFI. No existing C bindings; we would write and maintain a Rust cdylib exposing a C API against librespot's breaking-change-heavy Rust API (0.5, 0.7 and 0.8 each broke it), and need Rust 1.85+ in the build. Same Spotify breakage exposure as (b) with a much higher maintenance cost and no isolation. Reject unless a process boundary is unacceptable.
3. (c) Web API plus official Connect remote control only. No audio into our engine, so it fails the "real" requirement. Useful only as a metadata/library layer (search, playlists) alongside (b); note the late-2024 endpoint removals.

## Amiga verdict (68020/AGA)

Not feasible natively. Spotify needs TLS 1.2+ with modern ciphers, a Rust/Go client, Vorbis/AAC-class decoding, and OAuth; none fits a 68020 (reasoning from the architecture, no Amiga source consulted). Only viable route: proxy from a host. A desktop or Pi runs the sidecar and serves decoded PCM (or a lower-rate stream) over the network to an Amiga source plugin that is just a TCP/pipe PCM reader; the Amiga then needs only raw sample playback. That is a separate, optional Amiga feature, and it is Premium-account-on-a-host, not standalone.

## Recommendation

Take shape (b): a Spotify source plugin in the desktop tier that supervises go-librespot over its pipe and REST/WebSocket API, with librespot as a swappable fallback. Main risk: Spotify closes private playback endpoints without notice (10 Aug 2026 outage, issue #1737 open for weeks), so the feature can be dead for weeks with only upstream able to fix it, plus ToS and account-ban exposure. Mitigate by labelling it experimental, sidecar pinned and updatable independently of the player, and a hard failure path that does not affect other sources. Rough size: about 2 to 3 weeks for the desktop plugin (process supervision, pipe-to-PCM stream, control mapping, login UI, tests via a fake sidecar); Amiga network proxy about 1 more week on top (estimates, unverified).

## Open items

- Check whether #1744/#1745 merged and whether a post-0.8.0 librespot or go-librespot release shipped.
- Confirm go-librespot's latest release date and its current status against the Aug 2026 breakage.
- Check spotifyd/raspotify licence and pipe-backend details from primary pages.
