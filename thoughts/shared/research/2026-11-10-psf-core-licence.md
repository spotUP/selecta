---
date: 2026-11-10
topic: PS1 PSF/PSF1 playback core (R3000 + SPU + BIOS/HLE) source and licence, for ledger row I1257
status: research
---

# PS1 PSF core licence review

Statements without a primary source are marked unverified. Sources were read through raw.githubusercontent.com and the GitHub API; nothing was downloaded into the repo.

## What is already in the tree (read from the tree)

| Path | What | Licence | Where recorded |
|---|---|---|---|
| third_party/psflib | kode54 psflib, PSF container parser (C) | MIT, header "Copyright (c) 2012-2020 Christopher Snowhill" | `third_party/psflib/COPYING`, `VERSION.txt` |
| third_party/bbmp_spu | only the P.E.Op.S. PS1 SPU (peops/*.c, from nmlgc/aosdk eng_psf/peops, commit e359a6e) plus the bbmp spu_engine.cpp wrapper; no R3000 CPU | GPL-2.0-or-later (peops/License.txt = COPYING); SDK headers BSD (`AOSDK-license.txt`) | `third_party/bbmp_spu/VERSION.txt` |
| third_party/highly_quixotic | QSound core (Z80 + DSP) | GPL-3.0 | `VERSION.txt`, `COPYING` |
| plugins/replayers/qsf, snsf | QSF and SNSF replayers using psflib | GPL module | `plugins/replayers/*/CMakeLists.txt` |

Licence handling pattern: each vendored directory carries a `VERSION.txt` (upstream URL, commit, licence, consequence) plus the licence text as `COPYING`. GPL cores are built only as loadable modules behind `SELECTA_GPL_PLUGINS=ON`, never linked into the MIT core, and not built for Amiga (ledger row I704 records the exemption). There is no 2SF/USF/GSF plugin in the tree beyond snsf and qsf. No PS1 plugin exists: no `plugins/replayers/psf`.

Consequence: the SPU half of a PS1 core is already vendored (GPL). The missing half is the R3000 CPU, the PS1 hardware glue (timers, DMA, IRQ) and BIOS services, plus an adapter.

## Candidates

| Candidate | Source / last activity | Licence (as stated) | Lang, FPU | Size | BIOS | PSF2 | Accuracy notes |
|---|---|---|---|---|---|---|---|
| Highly Experimental core (Neill Corlett, kode54 repo) | https://github.com/kode54/Highly_Experimental, pushed 2014-02-28, directory `Core/` | **No licence stated.** GitHub API `license: null`; tree has no LICENSE/COPYING; `psx.c`, `r3000.c`, `spu.c` headers carry no licence line | C, integer, no FPU | r3000.c 764, psx.c 463, spu.c 812, spucore.c 1205, iop.c 1205 lines (about 4.5k lines) plus bios.c blob | **Yes, needs a Sony BIOS.** Readme.txt: "The unfortunate dirty secret here is that Sony BIOS is used" (SCPH1001, later a stripped PS2 5.0 BIOS); "Making this stuff 100% legal (via IOP kernel and PS1 BIOS HLE) is on the to-do list." `bios.c` holds the image | Yes (IOP emulation, vfs.c) | Most accurate PSF1/PSF2 engine; "r3000.c all C, all slow" (wait-loop detection compensates) |
| Audio Overload SDK PSF engine (nmlgc/aosdk fork) | https://github.com/nmlgc/aosdk, pushed 2023-03-14, `eng_psf/` | Mixed, GitHub reports "Other". AOSDK-license.txt (copy in tree): BSD for AO glue ("Copyright (c) 2007-2009, R. Belmont and Richard Bannister"); **MAME licence for the R3000 core** (`psx.c`: "PSX CPU emulator for the MAME project written by smf"; MAME licence text: "Redistributions may not be sold, nor may they be used in a commercial product or activity."); GPL for peops/peops2 SPU | C, integer | psx.c 3159, psx_hw.c 3515, eng_psf.c 340, eng_psf2.c 679 lines | **No BIOS image.** psx_hw.c: "Some BIOS services including exception handling (via HLE)", "HLE emulation of IOP operating system, including multithreading"; HLE is original AO code under the SDK licences | Yes (eng_psf2.c loads psf2.irx and runs the IOP HLE) | Lower accuracy than HE; known as acceptable for most PSF/PSF2 |
| sexypsf (PCSX-derived; Aminet 68k port exists) | upstream not located by me; Aminet: https://aminet.net/mus/play/sexypsf_68k.readme (v0.4.8, "68k port of Fredrik Wikstrom's OS4 port of Raphael Assenat's CLI sexypsf") | GPL-2 (Gentoo and GameBrew listings, secondary sources; unverified at primary source) | C, integer | unverified | Unverified; sexypsf is reported to use PCSX-style HLE BIOS (unverified) | No (PSF1 only) | PSF1 only, older, known pitfalls (unverified) |
| UPSE | https://github.com/xbmc/audiodecoder.upse (Kodi wrapper, pushed 2026-09-28); upstream upse not located | Unverified (Debian lists it as free software; GPL expected, unverified) | C, unverified | unverified | HLE BIOS reportedly (unverified) | PSF1 and MiniPSF; PSF2 unverified | Description: "concentrates on sound correctness" (Debian, secondary) |
| lazyusf-style siblings by kode54 | psflib (in tree, MIT); lazyusf2, vgmstream fork etc. | Not PS1 cores: no R3000 PS1 CPU. Relevant only as container reader | - | - | - | - | - |
| PCSX-derived full cores (pcsx_rearmed, beetle-psx) | https://github.com/libretro/pcsx_rearmed (pushed 2026-10-29), https://github.com/libretro/beetle-psx-libretro (pushed 2026-11-01) | GPL-2.0 (GitHub API spdx_id) | C/C++, integer | Large; full console incl. GPU, CD, GTE, far more than needed | Both ship an HLE BIOS option; its licence not checked (unverified) | No | Full-console cores; much dead weight for audio-only; pcsx_rearmed has ARM dynarec irrelevant here |
| mednafen PSX | mednafen.github.io | GPL-2 (mednafen overall; unverified for PSX module) | C++ | Large, full console | Needs real BIOS (unverified) | No | Not an audio-only core; not checked in detail |

## Findings

1. Highly Experimental is the right engine technically (PSF1 and PSF2, accurate, plain C) but is unusable as vendored code: it has no licence at all (all rights reserved by default) and requires a Sony BIOS image, which Selecta cannot ship or build. The author's own readme confirms the BIOS problem is unresolved. Cog/Foobar distribute it via kode54 under terms I could not confirm (forge.losno.co mirror returned HTTP 502; unverified whether Cog adds a licence file).
2. AOSDK's PSF engine is the only candidate that is a complete, self-contained, BIOS-free PSF1+PSF2 player in plain C. Its licence is a mix: the R3000 core derives from MAME, under the old non-commercial MAME licence. That clashes with MIT core and with any commercial use, and AOSDK-license.txt itself says "legally combining GPL with BSD and MAME licensed software is unclear". Selecta already vendors the GPL SPU from the same SDK and deliberately left MAME-licensed files out (bbmp_spu/VERSION.txt: "No MAME-licensed (non-commercial) SDK file is vendored"), so adding psx.c would reverse that decision.
3. sexypsf and UPSE are GPL-family and BIOS-HLE but PSF1-focused; their primary-source licence lines and sizes were not confirmed.
4. PCSX-derived full cores are GPL-2.0 with documented licence, but about 10x the needed code.

## Recommendation

| Option | What | Decisive trade-off |
|---|---|---|
| **Recommended: GPL module, sexypsf or pcsx-lineage HLE core (after primary-source licence check), plus bbmp_spu peops SPU and psflib** | New `plugins/replayers/psf`, built behind SELECTA_GPL_PLUGINS like qsf; no BIOS image shipped | Clean GPL, no BIOS, same gating as qsf; but PSF1 only and not on Amiga (GPL gate, row I704). Needs one more research step: locate the upstream sexypsf/UPSE source and quote its licence lines (not done here) |
| Alternative A: AOSDK `eng_psf` (psx.c + psx_hw.c) as GPL module | Best functional coverage: PSF1 and PSF2, HLE, no BIOS | MAME non-commercial clause in psx.c forbids commercial use and conflicts with "release MIT + player pack"; legal sign-off needed before any code, and AOSDK itself calls the mix "unclear" |
| Alternative B: Highly Experimental | Best accuracy, PSF2 | No licence plus Sony BIOS dependency; blocked until the author states a licence and an HLE BIOS exists. Ask kode54 only if the user wants that route |

Not recommended: write a new R3000 + HLE core from scratch (maybe 3k lines, clean MIT, Amiga-capable with no FPU) unless GPL gating is unacceptable; it is the only route to an MIT/Amiga PS1 player but the largest effort. Mention it to the user as a scope decision, not a menu of shims.

## Unverified or not done

- Upstream licence lines and file sizes for sexypsf, UPSE, mednafen PSX were not read at primary source.
- Whether psflib consumers (foo_psf, Cog) have a separate HE licence grant.
- Amiga cost (R3000 interpreter on 68k) not measured.
