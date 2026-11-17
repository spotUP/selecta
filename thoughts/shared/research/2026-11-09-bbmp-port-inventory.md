---
date: 2026-11-09
row: I1223
bbmp_pin: 19d87098 (github.com/creep-ltx/bbmp, cloned shallow, deleted after)
status: inventory, read-only
---

# bbmp port inventory (I1223)

Method: bbmp `src/bbmp/backends/formats.py` (BACKEND_BY_FORMAT, backend names) and `docs/replayers.md` (helper table, Modland counts) against `plugins/replayers/*/adapter.c` extension tables, the 2026-11-05 audit (`2026-11-05-bbmp-own-format-audit.md`) and the todos.md rows I706-I710. bbmp's library-backed formats (uade, adplug, openmpt, gme, xmp, sidplayfp, zxtune, vgm, kss, furnace, pxtone, organya, mdx, fmp/pmd, qsf, snsf, sndh, ym, sc68, klystrack, sunvox old form) all have a Selecta host. All other own helpers (about 60 formats: musicline, fac, psycle, iigs, nez, console, misc, synth, pc, retro, pcmusic, chip, adlib, tia, synder, t0ast, bero, axs, chipnsfx, ay fls/fxm/strc, cym, polly) have a bbmp_* or library plugin on main.

Verification: counts come from the backend names in formats.py (dict inverted, 100+ backends) and the I710 row list; a plugin dir existing is not "ported" (arkos and stonetracker are partial, see table).

## Not fully ported: 7 players

Ordered by Modland song count (user value).

| # | Player | Formats / ext | Modland songs | Licence | Host / Amiga | Size (bbmp) | Selecta host | Ledger row | State |
|---|---|---|---|---|---|---|---|---|---|
| 1 | Beepola | .bbsong (ZX Spectrum beeper) | 105 | clean-room GPL 3+, needs ROM data from maker | host (C++ step machines); Amiga unknown | csrc/ay/beepola_engine.cpp 3092 lines | bbmp_ay (add ext) | I709b (needs I751d downloader) | open, not started |
| 2 | Starkos | .sks (Amstrad/Spectrum) | 65 | Arkos Tracker 3 MIT (JUCE is GPL) | host; Amiga possible if JUCE-free | AT3 C++ port, >1000 lines | plugins/replayers/arkos | I433 | partial: arkos dir has at3_song.c/.h and CMakeLists only, no adapter.c, no extension claimed |
| 3 | NerdTracker 2 | .ned | 54 | giftware, non-commercial only; replay binary is a system file | host only | csrc/retro/ned_engine.cpp 491 lines + 6502/binary | new bbmp_ned (or bbmp_retro family) | I710o | BLOCKED on I710ga6 sysfile UI (done 9f380e2d); re-check, unblock |
| 4 | Stonetracker | .spm, .sps (Amiga) | 32 (table), 64 (row) | third-party StonePlayer_Hard.bin, not open source, redistributable only whole | host via Musashi-less 68k path; Amiga: native 68k possible | stonetracker_player.asm 352 lines | plugins/replayers/stonetracker (ext spm) | I710ae, I710aea, I710aeb, I710aed | partial: plugin claims spm only, no .sps companion, sysfile+unpack open |
| 5 | iXalance | .ixs | 24 | Wothke port CC BY-NC-SA 4.0 (unusable); bbmp ixs_engine.cpp MIT-tagged | host | csrc/retrovert/ixs_engine.cpp 1884 lines | none yet (new plugin) | I708b | PARKED: waiting for author's licence answer |
| 6 | ArkosTracker | .aks | 2 | as Starkos | as Starkos | as Starkos | arkos | I433 | same plugin as row 2 |
| 7 | BP SoundMon 1 | .bp (SoundMon 1 layout) | 1 | UADE players, GPL | Amiga via nuade | none (UADE label) | nuade probe | I710av | open: probe rejects the one file |

Also partial (plugin exists, a mode missing, not a whole player): SunVox modern projects need libsunvox (I706b, I706 parent still `[ ]`; old form plays via I706a). Ken's AdLib Music (41 songs, adplug, needs instrument file): not verified here whether the sysfile path is wired; check under I751d.

## Per-plugin Amiga gaps (already ledgered, not new players)

bbmp_ptk I710qa, bbmp_onyx I710ra, bbmp_smufi I710ta, bbmp_sbstudio I710va, bbmp_nez I710wa, bbmp_mgb I710xa, bbmp_shroom I710aaa, bbmp_skale I710aba, bbmp_tss I710afa, StoneTracker I710aea.

## Proposed ledger rows

Existing rows are authoritative for scope; these add the missing pieces only.

- I1223a: Arkos plugin adapter: write plugins/replayers/arkos/adapter.c on at3_song.c/.h (claim .aks and .sks), fixture from Modland Starkos (65) and ArkosTracker (2); reachability test through the engine entry. Closes I433 and half of row 2/6.
- I1223b: Beepola: re-check I709b blockers (I751d downloader for zx48.rom, p1d-samples.bin, svg-table-tail.bin) and start the port into bbmp_ay (.bbsong); highest song count of the open players.
- I1223c: NerdTracker 2: unblock I710o now that I710ga6 is done; port ned_engine.cpp as plugin with the system-file declaration (name, sha256, hippoplayer.se mirror).
- I1223d: Stonetracker finish: add .sps companion handling and sysfile install (I710aeb LHA unpack) so both .spm and .sps play; fold in I710aea Amiga and I710aed single-Paula.
- I1223e: SoundMon 1: extend the nuade soundmon probe to accept SoundMon 1 layout (I710av), one-file fixture.
- I1223f: iXalance: no work until I708b licence answer; keep PARKED, record the author-contact date.
- I1223g: SunVox modern projects: promote I706b (libsunvox desktop plugin) after the licence read; keep I706a for Amiga/web.
- I1223h: Verify Ken's AdLib Music (41, instrument file) end to end through adplug plus the I751 sysfile route; add a row only if it fails.
