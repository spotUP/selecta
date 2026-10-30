---
date: 2026-10-30
topic: Spotify backend gate (I654a)
status: in progress
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

## Results

- Login with Premium: pending (Johan).
- Playback through the API server and macOS output: pending.
- Credential persistence across restart: pending.
