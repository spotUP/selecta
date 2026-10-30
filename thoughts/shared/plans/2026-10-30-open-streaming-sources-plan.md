---
date: 2026-10-30
author: Claude (Opus)
status: approved
row: I725
research: thoughts/shared/research/2026-10-30-open-streaming-sources.md
---

# Open streaming sources plan (I725)

Spotify was dropped at the I654a gate (DRM). Johan 2026-10-30: "we dont want to swap in one other source we want to add as many as possible". This plan adds every open source without DRM that the research found legal for a third-party player.

## Decisions (Johan 2026-10-30, AskUserQuestion)

- Sources in scope: SomaFM, radio-browser (covers Nectarine, SceneSat and other demoscene radios), Internet Archive, Jamendo, podcasts (RSS + Podcast Index), ccMixter.
- Out: SoundCloud (terms forbid webcasting radio apps and aggregators), Bandcamp (personal use only, no API), Free Music Archive (API 404). Audius parked until its terms are read.
- AAC and Opus decoders: yes, after the MP3/Ogg rows.
- Amiga HTTPS: probe first (does the Amiga build link tls.c, and at what CPU cost), decide after. Plain-HTTP sources ship on Amiga meanwhile.

## Constraints

- Jamendo: non-commercial use, streaming only, no copy into the library ("Applications must not be specifically designed to cache the content"), creator credit and backlink per track.
- radio-browser and Podcast Index refuse `User-Agent: selecta` (http.c:364). I725p comes first.
- Reuse: `sel_net.request` for JSON APIs, `sel_live` ring buffer and ICY parser in stream_open.c, minimp3 / stb_vorbis / dr_flac. No new HTTP client, no new ICY parser.
- Every row: Mac and Amiga test steps named; reachability test through the top-level entry point.
- Terms marked UNVERIFIED in the research doc are read and quoted in the row's commit before that source merges.

## Rows (build order)

1. I725p: per-source User-Agent and request headers on the shared HTTP client.
2. I725a: generic stream source (URL list, .pls/.m3u resolve, ICY title, MP3 + Ogg; desktop on sel_live, Amiga with I199/I721).
3. I725b: SomaFM presets from channels.json (MP3 128 default).
4. I725c: radio-browser directory source (search, tags, country, codec filter, click ping).
5. I725d: Internet Archive audio source (advancedsearch, metadata, licence filter, 429/Retry-After, MP3/Ogg derivative).
6. I725e: Jamendo source (client_id, search/browse, mp31 stream, credit + backlink, no cache).
7. I725f: podcast source (RSS parser, Podcast Index search, Range playback).
8. I725g: ccMixter source (query API, per-upload licence).
9. I725h: AAC and Opus decoders.
10. I725i: Amiga HTTPS probe and decision; gates HTTPS-only sources on Amiga.
