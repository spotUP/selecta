---
date: 2026-10-22
topic: bbmp music sources (collections) and how Selecta inherits them
tags: [bbmp, sources, modland, hvsc, asma, sndh, zxart, keygenmusic, cache, mirrors]
status: final
---

# bbmp music sources

Source: scratchpad clone `ext/bbmp/src/bbmp/collection/*.py` and `ext/bbmp/docs/collections.md` (read 2026-10-22). bbmp calls a music source a *collection*. One Python module per collection, all behind one interface.

## The Collection interface (`collection/base.py`)

| Member | Meaning |
|--------|---------|
| `id` | short key used in song keys and cache paths (`modland`, `hvsc`, ...) |
| `name`, `home`, `about` | display name, site URL, one-paragraph description |
| `listing(progress) -> Listing` | downloads and parses the catalogue; yields entries (path, size, md5 when known, format, artist) |
| `done()` | listing finished, release resources |
| `parse(path)` | derive format / artist / title from a path inside the collection |
| `transport()` | `FtpTransport`, `HttpTransport` or `Mirrors` (ranked list of transports) |
| `url(path)` | canonical public URL for a path |
| `path_in(url)` | inverse: does this URL belong to the collection, and which path |
| `hosted_by`, `host_path`, `guest_path`, `via_host` (optional) | a collection whose files are served by another collection (HVSC inside Modland) |
| `has_lists`, `published_lists()`, `read_list()` (optional) | the collection publishes playlists of its own |

## The six collections

| id | Site | Catalogue source | Transport | Notes |
|----|------|------------------|-----------|-------|
| modland | ftp.modland.com | `/pub/documents/allmods_md5.zip` (md5 + path), `/allmods.zip` (sizes), daily changelogs | FTP + HTTPS mirrors: modland.antarctica.no, ftp.amigascne.org/mirrors/ftp.modland.com, ftp.modland.com, modland.ziphoid.com | publishes own playlists (`has_lists`); format and artist come from the path (`/pub/modules/<format>/<artist>/<file>`) |
| hvsc | hvsc.c64.org | hosted by Modland (`hosted_by`); Songlengths.md5 from hvsc.c64.org; STIL notes | HVSC own servers preferred for newest release | formats "HVSC - Musicians / Games / Demos" |
| asma | asma.atari.org | JSON list on the site | HTTP, root `/asma/` | Atari 8-bit SAP |
| sndh | sndh.atari.org | yearly `files/sndh<year>_lf.zip`; central directory read via two HTTP Range requests, no full download | HTTP, root `/sndh/sndh_lf/` | no md5 available |
| zxart | zxart.ee | paged JSON API `/api/language:eng/export:{what}/start:{start}/limit:{n}` | HTTP, not mirrored | ZX Spectrum AY and beeper |
| keygenmusic | keygenmusic.tk | JSON list | HTTP, root `/kgm/` | keygen and cracktro music |

## Shared mechanics

- **Song key** `<id>:<path>`. Globally unique, stable, human readable, and the key for ratings, history, favourites and playlists.
- **Format names reuse Modland's** (`Protracker`, `Fasttracker 2`, `SID` ...). Replayer selection is then shared across collections: one format table maps a name to a replayer backend.
- **Catalogue storage**: one SQLite file per collection, unioned by views for browsing and search. Re-listing replaces the table in one transaction.
- **File cache** `~/.cache/bbmp/files/<id>/<path>`; md5 checked when the catalogue knows it; LRU trim to a size budget; "kept" songs are exempt from trimming.
- **Companions**: files a song needs next to it (sample sets, `.smp`/`.sam`, multi-file formats) are fetched with the song by a per-format companion rule.
- **Prefetch**: the next song in the queue is downloaded while the current one plays.
- **Mirrors**: `Mirrors` ranks its transports by measured time to first byte, retries the next on failure. Limits: 3 s to first byte, 30 s for the body.
- **Home mirror** setting (`mirror` in config or `BBMP_MIRROR` env): `HttpTransport(<mirror>/<id><root>)` is put first in every collection's mirror list. A user with a LAN copy of Modland serves every collection from it.
- **Published playlists**: Modland's own lists are read through `published_lists()` / `read_list()` and show up next to the user's playlists.

## Constraint for Amiga

Every site above is HTTPS only, except Modland FTP and the plain-HTTP amigascne mirror. A classic Amiga has no cheap TLS (AmiSSL on 020 is slow, and not every stack ships it). Decision 2026-10-22 (design doc section 16): Amiga builds reach every source through a plain-HTTP DEViLBOX mirror endpoint `/api/mirror/<source>/<path>`; desktop builds talk HTTPS directly; both honour a bbmp-style home mirror setting. The DEViLBOX endpoint is server work (ledger "Found", D9).

## What Selecta takes

Design doc section 9 defines `sel_source` with the same member set, the same song key, Modland format names, per-source SQLite catalogue, the cache and prefetch design, and `Mirrors` ranking. One C file per source under `src/library/sources/<id>.c`. Adding a source never touches the engine, the UI or the replayers.
