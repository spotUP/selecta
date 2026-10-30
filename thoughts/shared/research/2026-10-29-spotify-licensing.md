---
date: 2026-10-29
topic: What Spotify's Developer Terms and Policy allow for Selecta setups A-E (ledger I694o)
tags: [research, spotify, licensing]
status: complete (research only; not legal advice)
---

# Spotify licensing for Selecta

Not a lawyer statement; this is a reading of public documents on 2026-10-29. Primary sources: Developer Terms v10 (effective 15 May 2025) https://developer.spotify.com/terms ; Developer Policy (effective 15 May 2025) https://developer.spotify.com/policy ; quota page https://developer.spotify.com/documentation/web-api/concepts/quota-modes ; Web Playback SDK page https://developer.spotify.com/documentation/web-playback-sdk ; Soloist page https://developer.spotify.com/documentation/soloist . Secondary sources are marked (secondary). Fetches were summarised by a small model, so quotes are short and should be re-read on the live page before anything ships.

## 1. Verdict table

| Setup | Verdict | Deciding clause |
|---|---|---|
| A. Public free web app, Web Playback SDK, own Premium OAuth, Selecta's app | GREY leaning forbidden in practice | Technically the sanctioned path, but the SDK page says it "must not be used in commercial projects without Spotify's prior written approval" (non-commercial hobby is not caught), Policy III.5 and III.7 forbid integration with other services' streams and mixing, and Selecta's own client ID is capped at 5 allow-listed users in development mode. Extended mode needs a company with 250k MAU. So: allowed in kind, unusable at public scale. |
| B. Same, each user registers their own developer app | GREY | Each user's app is a separate development-mode app with the user as owner and sole user, so the 5-user cap is never hit. Nothing in the Terms forbids it, but "app owner must have Premium" applies per user (fits), and the mixing rule (III.5/III.7) still applies. No official statement either way on the pattern. |
| C. Server runs (go-)librespot per listener, holds credentials, streams PCM to browser | FORBIDDEN | Terms IV.2.1.2 bars "reverse-engineering, or extracting source code from the Spotify Platform"; IV.2.2.3 bars "stream ripping" or functions that make capture easier; Policy III.7/III.5; server-side re-streaming is also redistribution and holding user credentials. Highest-risk setup. |
| D. Mac desktop app, go-librespot locally, user's own account | FORBIDDEN by the text, tolerated in practice | Same reverse-engineering clause; librespot README itself says use is "probably forbidden". Per-user, local, no redistribution lowers enforcement risk, not the legal status. No documented ban of a librespot-only user found (secondary). Since 10 Aug 2026 Spotify has closed private playback endpoints, so it may not even work. |
| E. Connect remote only (Web API playback control), no audio through Selecta | ALLOWED (with limits) | Standard Web API use with a registered app and Premium. Limits: dev mode 5 users (or each user's own client ID, as B), attribution (Policy II.4), no caching of Spotify Content (Terms IV.3.2), and the mixed-sources rules for any UI that blends Spotify rows with other sources. |

Soloist (Spotify's official headless client, Aug 2026): Spotify docs say "Your app or script can control it through the optional WebSocket API", each user generates their own key, and "Do not redistribute Spotify Soloist archives or binaries". That is the only first-party route that gives a local audio engine; it is Linux-only at launch and plays to PipeWire/PulseAudio, not to a pipe Selecta can read (per the overview page, secondary summary). Music Assistant notes (secondary) that "Spotify's terms do not clearly allow using Soloist this way".

## 2. Quoted clauses

Developer Terms (https://developer.spotify.com/terms, v10):
- IV.2.1.2: "reverse-engineering, or extracting source code from the Spotify Platform" (prohibited).
- V.11.3: end users must be prohibited from "decompiling, reverse-engineering, disassembling" the Platform.
- IV.2.2.3: "facilitating 'stream ripping' or other functionalities that make it easier for users to capture" content (prohibited).
- IV.3.2: "Do not locally cache any Spotify Content, except as strictly necessary to enhance the performance of your SDA".
- IV.3.1.1: "you may not store, aggregate or create compilations or databases of Spotify Content".
- IV.2.1.1: no use of the Platform or Content "to train a machine learning or AI model".
- VI.7.1: "Spotify may use quotas and place use restrictions".
- The Terms define streaming as playback "using the Spotify Platform", i.e. via Spotify's SDKs and APIs; a non-Platform client is outside what the licence grants.

Developer Policy (https://developer.spotify.com/policy):
- III.5: "Do not create any product or service which is integrated with streams or content from another service."
- III.7: "Do not permit any device or system to segue, mix, re-mix, or overlap any Spotify Content with any other audio content".
- III.9: no "transfer of data to another service", except a user's personal data or playlist metadata.
- III.10: products "targeted for use by businesses" prohibited; content is personal, non-commercial.
- III.11: no product that would "mimic, or replicate or attempt to replace a core user experience of Spotify" without written permission.
- III.13: no analysing Spotify Content or Service "for any purpose" (profiles, listenership metrics).
- II.4.1 / II.4.2: content must be attributed "as being supplied and made available by Spotify" with a link back to the album, content or playlist.
- II.5: "no playback of Spotify Content without showing relevant cover art and metadata".
- IV.1: streaming only for Premium subscribers. IV.2: "commercial uses are not permitted for SDAs" (streaming apps); no selling the app, in-app payment or ads on a streaming app. Non-streaming apps may sell the app or show ads.
- VI.2 (naming): the app name should not begin with "Spot" or be confusable with Spotify.

Web Playback SDK page: requires Premium (mobile-only plans excluded); desktop and mobile browsers; "must not be used in commercial projects without Spotify's prior written approval".

## 3. Mixed sources, playlists

- Mixing: III.5 is the hard problem for Selecta's core idea (one library across sources). III.7 forbids crossfade, gapless overlap or DSP-mixing of Spotify audio with other audio. Under A/B/E, Spotify tracks sitting in the same playlist as local/YouTube/radio tracks is at best grey (III.5 reads as "integrated with streams or content from another service"); crossfading between a Spotify track and a local track is forbidden. Under C/D the audio is outside the Platform, so these clauses are moot but the reverse-engineering clause is the violation.
- Playlist read/write sync (D-SP-5 b, live two-way): the Web API playlist scopes work for new apps (the Nov 2024 cut removed Related Artists, Recommendations, Audio Features, Audio Analysis, Featured/Category playlists, 30-second previews in multi-get, algorithmic and Spotify-owned editorial playlists; own and followed user playlists are not on the list; https://developer.spotify.com/blog/2024-11-27-changes-to-the-web-api). Terms allow transferring "the metadata of the user's playlists" (III.9), which supports mirroring a user's playlists into Selecta. Storing Spotify Content in a Selecta database beyond a performance cache is limited by IV.3.1.1/IV.3.2, so rows should keep ids plus minimal display fields and refresh. Pushing non-Spotify tracks into Spotify playlists is not possible anyway.
- Branding: attribute "Spotify" as source, link back (II.4), show cover art and metadata during playback (II.5), do not alter the logo, use the official brand guidelines (not read in full here; I654b still must read https://developer.spotify.com/documentation/design).

## 4. Quota and approval path

- Development mode: "Up to 5 authenticated Spotify users", each on the app's allow-list; "The app owner must have a Spotify Premium account for apps in development mode to function." (quota page). Secondary reports of a one-client-ID-per-developer cap in 2026: not on the quota page, unverified.
- Extended quota: applications accepted only from organisations since 15 May 2025, via company email. Criteria: established business entity, active launched service, at least 250k monthly active users, key markets, commercial viability, adherence to terms. Review "can take up to six weeks". Source: quota page and https://developer.spotify.com/blog/2025-04-15-updating-the-criteria-for-web-api-extended-access . A hobby project cannot qualify, so a shared Selecta client ID stays at 5 users; D-SP-3 (a) (per-user client ID) is the only route that scales for free, and it is a workaround of the cap in spirit (grey, no clause names it).
- Commercial: streaming apps may not be sold or monetised (Policy IV.2); the SDK additionally needs written approval for commercial use. Selecta is free MIT hobby: fine on that axis.

## 5. Precedents and status of librespot

- Spotify's own README-level stance: librespot says use is "probably forbidden" (existing research). Spotifyd, ncspot, Volumio's Spotify Connect plugin, moOde, Music Assistant and Mopidy-Spotify ship or wrap librespot-class clients for years. I found no Spotify legal action or documented account ban against librespot-only users (secondary: Volumio and Roon forum threads, DMN 2021). Spotify did suspend accounts for stream-ripping tools and modded apps in 2021 (secondary), which matches IV.2.2.3.
- Spotify's real countermeasure is technical: librespot playback broke Nov 2025 (issue #1623, fixed in librespot 0.8.0, secondary); accounts created from Dec 2024 "generally cannot play through librespot" (Music Assistant provider page, secondary); since about 10 Aug 2026 private playback endpoints are closed (librespot #1737, open, workaround via device-pair flow PR #1745 and desktop client-ID token PR #1744; https://github.com/librespot-org/librespot/issues/1737). go-librespot open issues: #364 INVALID_CREDENTIALS on 0.7.1 (11 Aug 2026), #279 audio key error (Dec 2025), #432/#433 zeroconf failures (28-29 Oct 2026) (https://github.com/devgianlu/go-librespot/issues). The Go port's own README note via libremedia (secondary) says playback was broken.
- Spotify's answer is Soloist (13 Aug 2026, https://developer.spotify.com/blog/2026-08-13-introducing-spotify-soloist): an official sanctioned headless client with local WebSocket control. Music Assistant now offers Soloist beside librespot.

## 6. Recommendation

- Public web service: do not build C. Do not build A on Selecta's own client ID (5-user cap, no path to extended mode). Offer E and/or B only: user's own client ID, Connect remote plus metadata/playlist mirroring, no mixing of Spotify audio with other sources, attribution on every Spotify row. Never hold other people's Spotify credentials on a server.
- Desktop (Mac): the legal order is Soloist (sanctioned, but Linux-only and its WS API terms are unclear), then E (remote), then D (go-librespot, knowingly outside the Terms, per-user, local, and currently possibly broken). D-SP-1 (b) go-librespot-only is the one option that breaks IV.2.1.2 for every user and is also the most brittle; it should stay opt-in, labelled, with the terms notice shown at pairing, and I654a (live gate) must pass first. Spotify audio inside Selecta's DSP/crossfade/Amiga PCM proxy is both the point of (b) and what III.7 forbids on the sanctioned path, so those features cannot be offered for the sanctioned backends.
- Open items not verified: current brand guidelines text; Soloist EULA on third-party control; whether a Spotify dev-mode "one client ID per developer" cap exists; whether Spotify has ever enforced against librespot-only accounts.
