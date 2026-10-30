---
date: 2026-10-30
topic: Spotify backend gate (I654a)
status: gate failed for go-librespot 0.10.5
---

# Spotify backend gate (I654a)

Plan: thoughts/shared/plans/2026-10-29-spotify-source-plan.md (D-SP-1: go-librespot only).

## Setup

- go-librespot 0.10.5 from the Homebrew bottle (`brew install go-librespot`), binary
  /opt/homebrew/opt/go-librespot/bin/go-librespot. Source clone of v0.10.5 kept in
  ~/Code/Spotify-gate/go-librespot for reading only.
- Gate config dir ~/Code/Spotify-gate/run/config.yml: `credentials.type: device_auth`,
  device name "Selecta gate", API server on 127.0.0.1:3678.
- Start: `go-librespot --config_dir ~/Code/Spotify-gate/run`. API server comes up first, then a
  device id, then a pairing line: "to complete authentication visit https://spotify.com/pair?code=XXXXXX".
  Startup to pairing prompt took about 1 second on 2026-10-30.

## Results (2026-10-30)

- Login: PASS. Johan paired through spotify.com/pair at 01:28; log shows "authenticated AP" and
  "authenticated Login5" for the Premium account. Credentials persisted in run/state.json.
- Playback: FAIL. `POST /player/play {"uri":"spotify:track:4cOdK2wGLETKBW3PvgPWqT"}` returns 200, then
  every track fails: "failed retrieving audio key: failed retrieving aes key with code 1". 51 refusals
  in a few seconds as the player skipped through the autoplay context. No audio reached the output.
- Cause: Spotify refuses the legacy AES audio-key request (code 1). go-librespot's working path is a
  PlayPlay license request (player/player.go:666), but the shipped PlayPlay plugin is a stub
  (playplay/impl/impl.go: IsSupported false, "playplay plugin not provided"). Upstream issue #317
  (closed: "Legacy AES audio key refused (code 1) ... needs PlayPlay") and #279 (open, updated
  2026-10-26: new accounts and OAuth login fail the same way; the official client works on the same
  account) confirm it is not an account or credential problem.
- PlayPlay is Spotify's key obfuscation (DRM). Supplying an implementation means circumventing it;
  that is not something Selecta can build or ship. So go-librespot 0.10.5 does not pass the gate.

## Remaining routes (for the D-SP-1 decision)

- Official Spotify playback: Web Playback SDK (browser, Premium) or controlling the official desktop
  client through Spotify Connect / Web API. Audio stays in Spotify's own player; Selecta controls it.
- Wait for an upstream go-librespot release that plays without a user-supplied PlayPlay plugin.
