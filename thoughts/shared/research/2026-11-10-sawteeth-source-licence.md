---
date: 2026-11-10
topic: Sawteeth (.st, SWTD/SWTT) player source and licence, for ledger row I1242e
status: research
---

# Sawteeth player source and licence

## Comparison

| Candidate | URL | Licence | Language | Float needed | Formats | State |
|---|---|---|---|---|---|---|
| pulkomandy/Sawteeth (original Norberg source, lib/) | https://github.com/pulkomandy/Sawteeth | MIT, per-file header "Copyright 2001, Jonas Norberg" | C++ (BeOS classes, new/delete), CMake | Yes, `float` throughout | SWTD and SWTT, file versions up to 1200 (loader rejects newer; special cases below 900, 910, 1200) | Dormant, last push 2020-12-12 |
| NostalgicPlayer Sawteeth agent | https://github.com/neumatho/NostalgicPlayer (Source/Agents/Players/Sawteeth/) | MIT repo licence; file header is a looser "may be used in any software as long the license of NostalgicPlayer is keep" | C# (port of the original) | Same algorithm, C# float | Loader in SawteethWorker.cs (SWTD/SWTT handling not checked line by line) | Active, last push 2026-11-10 |
| DEViLBOX (local) Sawteeth WASM + TS parser | local only, ~/Code/DEViLBOX | Repo is GPL-3.0 (LICENSE) | WASM blob, TS parser (650 lines) | Not applicable | Parser handles SWTD only | Active locally |
| UADE, libxmp, libopenmpt, JS ports | not found | n/a | n/a | n/a | n/a | No evidence they support Sawteeth |

## pulkomandy/Sawteeth (the original source)

Player code lives in `lib/source/` and `lib/headers/` (BeOS GUI is separate in `Source/` and not needed): `player.cpp` (204 lines), `insply.cpp` (348), `wave.cpp` (402), `lfo.cpp` (27), `ins_part_channel.cpp` (266), `song.cpp` (1394, loader and saver), `txt.cpp` (206, text format reader), plus headers (about 450 lines). Total about 3,440 lines.

Licence quote, from `lib/source/player.cpp` and `lib/headers/stSong.h` (read via https://raw.githubusercontent.com/pulkomandy/Sawteeth/HEAD/lib/source/player.cpp): "Copyright 2001, Jonas Norberg. Distributed under the terms of the MIT Licence." README (https://github.com/pulkomandy/Sawteeth): "Originally developed by Jonas and Arvid Norberg and released in 2000, it is now open-sourced under the MIT licence." The GitHub API reports no licence file (`license: null`); no LICENSE file in the tree. So the grant is the source-header line plus README, with no separate licence text.

Code facts: dependencies are libc/libm only in the player (`string.h`, `stdlib.h`, `math.h`, `inttypes.h`, `stdio` for stderr diagnostics); BeOS types come from `types.h` (28 lines). Float use is pervasive: about 223 float/double/math hits; per-channel render buffer is `new float[spspal]`, waveform tables are `static float sint[513]`, `trit[513]`; filters, ADSR and LFO are float. On a 68020 without FPU this needs soft-float or a fixed-point rewrite; with a 68881/68882/68040/68060 FPU it builds as-is. Loader: `case 'SWTD'` binary, `case 'SWTT'` text (song.cpp lines 411-412), `ST_CURRENT_FILE_VERSION 1200`.

## NostalgicPlayer

Repo README: "NostalgicPlayer is licensed under the MIT License" (https://github.com/neumatho/NostalgicPlayer). Sawteeth is in its supported-format table (`.st`). Files: `Sawteeth.cs`, `SawteethWorker.cs`, `Implementation/{InsPly,Lfo,Player,Wave}.cs`, `Containers/*.cs`. Header of Player.cs and SawteethWorker.cs: "This source, or parts thereof, may be used in any software as long the license of NostalgicPlayer is keep. See the LICENSE file for more information." (https://raw.githubusercontent.com/neumatho/NostalgicPlayer/HEAD/Source/Agents/Players/Sawteeth/Implementation/Player.cs). It is a derived port of the same algorithm, so it adds nothing for a C port except a second reading of the loader; it is a managed-language port, not buildable for 68k.

## DEViLBOX (local copy, read-only check)

`/Users/spot/Code/DEViLBOX/public/sawteeth/Sawteeth.wasm` plus JS glue, `src/lib/import/formats/SawteethParser.ts` (SWTD only, cites NostalgicPlayer SawteethWorker.cs as reference), `src/engine/sawteeth/SawteethEngine.ts`. Repo licence GPL-3.0. No C/C++ source of the Sawteeth player was found in DEViLBOX src; the WASM provenance (likely built from the pulkomandy source) is unverified. GPL-3 makes it unsuitable as a code source for an MIT project. Local grep outside DEViLBOX: no other copy found (HippoPlayer, Reference, Emulators greps were started but not finished; see Unverified).

## Unverified

- Whether the UADE, libxmp, libopenmpt, bbmp or HippoPlayer trees contain Sawteeth: web searches (UADE, xmp, OpenMPT, NostalgicPlayer queries) returned nothing; I did not open their repositories. Format lists of those projects were not read.
- No pure JavaScript port found by search; not exhaustive.
- Whether the original Sawteeth MIT grant is stated by Jonas Norberg himself elsewhere (original 2000 site not found); the only evidence is the 2001 source header and the pulkomandy README.
- NostalgicPlayer SawteethWorker.cs: SWTT support and supported file versions not read.
- Local grep of ~/Code/HippoPlayer, HippoPlayer-68k, Reference, Emulators, hippoplayer.se timed out at 120 s (one search left running in background, result not read). ~/Code/bbmp and ~/Code/NostalgicPlayer do not exist.
- Last commit date for NostalgicPlayer is the pushed_at value (2026-11-10), not the Sawteeth folder's last change.

## Recommendation

Port from pulkomandy/Sawteeth `lib/` (MIT, header "Copyright 2001, Jonas Norberg"). It is the original, the only candidate that is plain C++ at about 3,400 lines with libm-only dependencies, it covers both SWTD and SWTT up to file version 1200, and MIT is compatible with an MIT project and a separate player pack (keep the copyright header and add the MIT text to the pack, since the repo ships no licence file). Decisive trade-off: the code is float-heavy, so it fits 68k only with an FPU target or after a fixed-point rewrite of Wave/InsPly/Player (about 1,000 lines); a 68020 with fast RAM and no FPU needs that rewrite or soft-float. Runner-up: NostalgicPlayer's C# port (MIT with a "keep the licence" header), useful only as a cross-check of the loader; not a build source. Not usable as source: DEViLBOX (GPL-3, WASM blob, SWTD only).
