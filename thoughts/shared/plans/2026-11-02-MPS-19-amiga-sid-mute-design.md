---
date: 2026-11-02
topic: MPS-19 design, SID voice mute and solo on the Amiga build (playsid.library), same RP_MUTE path as MPS-2
tags: [plan, design, sid, playsid, amiga, mute, magplaysid]
status: draft, awaiting Johan's answers to section 6
---

# MPS-19 Amiga SID voice mute: design

Row MPS-19 (`thoughts/spot/todos.md`), plan `thoughts/shared/plans/2026-11-02-magplaysid-parity.md` section 3 MPS-2
and decision D9 (reversed by Johan 2026-11-02: "Add mute"). Design pass only: no code changed.

Sources read: `plugins/replayers/playsid/{adapter.c,playsid_api.h}`, `plugins/replayers/sid/{adapter.c,shim.cpp}`,
`include/selecta/replayer.h:111-116`, `src/engine/engine.c` (1060-1098, 1824-1884, 2585-2597, 10130-10165),
`third_party/libsidplayfp/src/sidemu.cpp:30-66`, playsid.library source in
`~/Code/HippoPlayer-68k/code/playsid.library/playsid.asm` (function table 180-210, init 270-285, `@SetChannelEnable`
920, `Sound` 2280-2312, `CreateFour` 2876-2892, SID write handlers 5300-5360, `writeSIDRegister` 5367-5450) and
`playsid_libdefs.i` (psb fields). `resid-68k/` in that checkout is empty (submodule not fetched), so nothing below about
the inside of the 68k reSID port is read; it is marked where it matters.

## 0. Correction to earlier docs

The inventory (`thoughts/shared/research/2026-11-01-playsid-inventory.md`) and plan MPS-2 say playsid.library has no
channel mute API. That is wrong: the library has the public LVO `SetChannelEnable` (playsid.asm:192, implementation
:920), `a0` = pointer to 4 UWORDs (voice 1, 2, 3, channel 4 = samples), stored in `psb_ChannelEnable`
(playsid_libdefs.i:122), all four set to 1 when the library is first opened (:280-283). It is honoured only by the
classic (normal) engine:
- `Sound` (:2280-2312) feeds frequency 0 to Paula channel 1/2/3 when its enable word is 0;
- `CreateFour` (:2882-2885) stops channel 4 (the PlaySID sample channel) when word 3 is 0.

`writeSIDRegister` (:5367), the path every SID write takes in reSID 6581/8580/auto and in all hardware modes
(SIDBlaster, ZorroSID, USBSID-Pico, Trinity), never reads `psb_ChannelEnable`. So in those modes the API exists but
does nothing. That gap, not a missing API, is the root cause MPS-19 fixes.

## 1. How Selecta drives playsid.library today

- `plugins/replayers/playsid/adapter.c` registers kind `SEL_RP_KIND_HW` (:405): the library plays on its own (classic
  emulation on Paula, or reSID on Paula 14-bit / AHI, or a real SID), Selecta never sees the audio. The engine renders
  silence for HW slots and polls `rp_hw_poll` (engine.c:10155-10165).
- Every library call goes through the `ps_ops` table (`playsid_api.h:24-55`); the Amiga build fills it with LVO calls
  (`playsid_amiga.c`), the host test `tests/engine/playsid_test.c` substitutes a logging stub (`s_ops`, :93).
- `rp_open` sets `info->voice_count = 0` (adapter.c:130): the engine knows no channels for SID on Amiga.
- Private reads already exist (`read_sid_regs`, C64Mem at base+346, envelopes at base+448), version gated.

## 2. Where a voice can be silenced, and the choice

| Candidate | Classic mode | reSID / hardware modes | Verdict |
|---|---|---|---|
| A. `SetChannelEnable` LVO (public, exists) | true per-voice silence on Paula, digi channel stopped | ignored today | Use for classic now |
| B. Mask the voice's SID writes inside `writeSIDRegister`, driven by the same `psb_ChannelEnable` | n/a (classic returns before it) | works for reSID and for real SIDs | Use: library patch |
| C. Selecta-side register masking (patch the library's `.JMP` handler table or code in RAM) | | | Rejected: writes happen inside the library's 6502 emulation; patching its code from outside is version-fragile and breaks every other user of the shared library |
| D. Poke `C64Mem+$D4xx` from Selecta | | | Rejected: reSID gets each write through `jsr sid_write` at write time; C64Mem is only a shadow, poking it changes nothing audible and the 6502 rewrites it next frame |
| E. Paula/AHI output channel | per voice in classic only | reSID mixes all 3 voices into one 14-bit stream per SID | Rejected: no per-voice stream in reSID mode; classic already covered by A |

**Chosen technique: one API, `SetChannelEnable`, in every mode.** Selecta calls the existing LVO. The library patch
(B) makes `writeSIDRegister` honour `psb_ChannelEnable` with exactly the masks libsidplayfp uses for the host
(`sidemu::writeReg`, sidemu.cpp:34-56), which is what MPS-2 already ships:

- control register of a muted voice (`$04`, `$0B`, `$12`, plus `+$20`/`+$40` for SID 2/3): `data &= $0E` (waveform
  and gate cleared, test/ring/sync kept);
- volume register `$18` when the sample channel is muted: `data |= $0F` (master volume pinned to 15, so a
  volume-register digi becomes a constant and is silent).

So "register masking" is used, but in the layer that owns the SID writes (the library), behind its own public API,
and it is the same algorithm as the host. The D9 objection was to a Selecta-side hack (row C); that stays rejected.

### Audible accuracy (both host and Amiga, since the algorithm is shared)

- Ring mod and sync on other voices: unaffected. They depend on the source voice's oscillator (accumulator MSB), which
  keeps running: frequency registers and the ring/sync/test bits are not masked.
- Filter: routing (`$17`) and cutoff/resonance are untouched, so the other voices' filter is unchanged. The muted voice
  stops feeding the filter (waveform 0), which is what a mute means.
- Mute takes effect at once (waveform cleared, envelope enters release with no waveform: silent). Unmute takes effect
  at the voice's next control write, normally the next note: a held note does not come back mid-note. Same on host.
- Voice 3 side effect: muting voice 3 changes `OSC3` ($1B) and `ENV3` ($1C) readback (waveform 0, gate off). Tunes that
  read them (random numbers, envelope-driven filter sweeps) can play differently while voice 3 is muted. Same on host
  (libsidplayfp has the identical flaw). The 3OFF bit ($18 bit 7) is not a substitute: it only cuts voice 3 when it
  is not routed through the filter.
- Digi (channel 3): the `$18 |= $0F` trick mutes volume-register digis only (the common Mahoney/Galway-style 4-bit
  samples); waveform- or pulse-width-based digis are voice output and stay audible unless their voice is muted. Side
  effect: master-volume fades are lost while the digi channel is muted. Classic mode instead stops the PlaySID sample
  channel 4 (`CreateFour`), takes effect at the next sample start (unverified: read from code, not heard).
- Classic mode voice mute is a Paula frequency 0 per channel: exact silence, no side effects on other voices (sync in
  the classic engine reads the source voice's registers from C64Mem, which are not changed).
- Hardware modes: the masks go to the real chip, so mute works on SIDBlaster/ZorroSID/USBSID-Pico/Trinity too
  (cannot be proven on the rig, as for I434b).

## 3. CPU cost (estimates from the code, nothing measured)

- Classic mode: zero added per-frame cost; `Sound` already tests the enable words every frame.
- reSID/hardware modes, library patch: one `tst.l` of a mask word plus a not-taken branch at the top of the masking
  block, about 4-6 cycles per SID write on a 68020 with the instruction cache warm; when a channel is muted, a register
  index compare chain of about 20-30 cycles per write. For scale, the same function already saves and restores 15
  registers per write (`movem.l d0-a6`, about 120 cycles on 68020), so the patch adds under 5% (unmuted) to 25%
  (muted) to the per-write overhead, and nothing to reSID's per-sample work, which dominates.
- Per frame, 68020 at 14 MHz (about 280,000 cycles per 50 Hz frame): a normal tune does 20-50 writes per frame, so
  at most about 1,500 cycles = 0.5% while muted, about 300 cycles = 0.1% unmuted. A volume-register digi tune
  (up to about 16,000 `$18` writes per second) costs at most about 0.5 M cycles/s = 3.5% while muted, about 0.1 M
  cycles/s = 0.7% unmuted.
- 68060 at 50 MHz: same instruction count, about 1 cycle each: below 0.1% in every case.
- Selecta side: one LVO call per mute change (a memory copy of 8 bytes), on the audio task between render calls.

## 4. Changes

Step A (Selecta only, works with the library Johan's users have today, classic mode):

1. `src/engine/engine.c:2592`: resolve `SEL_RP_MUTE` for `SEL_RP_KIND_HW` as well as RENDER. `render_slot`
   (:10130-10145) already runs the mute diff before the HW silence loop (:10155), so the same loop and the same
   `rp_mute_calls` sentinel serve HW slots with no other engine change. HW slots return before `sel_mixer_init` and
   `route_direct` (:1869-1881), so a non-zero `voice_count` does not create mixer voices or a direct route.
2. `include/selecta/replayer.h:111`: doc says "RENDER and HW kinds"; a HW replayer that plays on its own clock
   needs the hook for the same reason a RENDER one does.
3. `plugins/replayers/playsid/playsid_api.h`: add
   `void (*set_channel_enable)(const uint16_t enable[4]);` (LVO `SetChannelEnable`, a0) and
   `int (*mute_in_mode)(uint32_t om);` or a pure helper `ps_mode_can_mute(om, lib_version, lib_revision)` in
   `playsid_map.c` (classic always; reSID/hardware only from the patched library revision, step B).
4. `plugins/replayers/playsid/playsid_amiga.c`: implement `set_channel_enable` with the LVO (offset from `playsid_lib.i`,
   do not count the function table by hand) and expose lib_Version/lib_Revision for the gate.
5. `plugins/replayers/playsid/adapter.c`:
   - `ps_state` gains `uint16_t enable[4]` (all 1) and `uint32_t mute_mask`;
   - `rp_mute(state, channel, muted)`: channel = chip*4 + voice as on host; ignore channel >= 4*chips; update the mask,
     rebuild `enable[]` for SID 1 and call `set_channel_enable` when playing; count `sel_playsid_mute_sets`
     (adapter-level sentinel next to `sel_playsid_reg_fills`);
   - `rp_start`: re-apply the mask after `AllocEmulResource`/`StartSong` (a subsong restart must keep the mute, as
     the host shim re-applies at every start, shim.cpp:79);
   - `stop`/`rp_close`/`rp_hw_release`: restore all four enables to 1. The library base is shared (HippoPlayer or
     another player may open it next) and `psb_ChannelEnable` survives as long as the library stays open;
   - `rp_open`: `info->voice_count = ps_mode_can_mute(...) ? 4 * ps_sid_count(...) : 0` (the count sizes SOLO and
     the mute UI, as on host, adapter.c sid:113-115); register `set(&g_hooks[19], SEL_RP_MUTE, ...)` and grow
     `g_hooks` by one (check the array size and terminator).

Step B (playsid.library patch, sent to its maintainer, then Selecta raises the gate):

6. `playsid.asm` `writeSIDRegister` (:5367), after the `OM_NORMAL` early return and before the hardware/reSID
   dispatch: if the mute mask is non-zero, apply the two masks above per chip (`d7 & $1F` gives the register, `d7 >> 5`
   the chip). Covers reSID and every hardware mode in one place.
7. Multi-SID: `psb_ChannelEnable` has 4 words, SID 1 only. Add `ULONG psb_ChannelMask` (12 bits: chip*4+voice, 1 =
   muted) appended at the end of the base (before `psb_SIZEOF`, so the private offsets 346 and 448 Selecta reads do
   not move) and a new LVO `SetChannelMask(d0)` appended after `SetResidBoost` (table end, :206). `SetChannelEnable`
   keeps working by updating bits 0-3. Bump `lib_Revision`; Selecta's gate keys on it.
8. Report to the maintainer (not a mute item, found while reading): in the display-data block near :1656-1665 the
   enable tests read `2(a1)`, `4(a1)`, `6(a1)` (the display buffer) instead of `2(a0)`, `4(a0)`, `6(a0)` (the enable array), so the
   scope envelopes of voices 2-4 ignore `SetChannelEnable`. Display only, not audio.

## 5. Proof

One reachability test, through the engine top level, on host with the existing stub layer:

- `tests/engine/playsid_test.c` (ctest `engine-playsid-native-adapter`, CMakeLists.txt:894-901): new case with
  `tests/fixtures/playsid/mono_Commando.hdr` in classic mode: create the engine, play, send `MUTE 1 on`; assert
  `stats.rp_mute_calls == 1` (the MPS-2 sentinel, engine.h:528), stub log shows `SetChannelEnable {0,1,1,1}` exactly
  once and `sel_playsid_mute_sets` moved; `SOLO 2` gives `{0,1,0,0}`; `MUTE 1 off` after `MUTE 1 on` gives
  `{1,1,1,1}`; `MUTE 4 on` gives word 3 = 0 (digi); stop/close leaves `{1,1,1,1}`; subsong change re-applies.
- Same file, unit level: `ps_mode_can_mute` table (classic yes; reSID no below the patched revision, yes from it);
  2SID fixture `2sid_Sparkster_Lakeside.hdr` reports voice_count 8 and, with the patched revision stub, `MUTE 6 on`
  calls `SetChannelMask` with bit 5.
- Amiga cross build (`amiga-build`): compile of `playsid_amiga.c` with the new LVO call; no 68k unit harness exists
  for the library itself, so step B's masking is proven in the library's own repo and on the rig.
- Rig check, one run at the end of the batch (rig 2 fs-uae, `--ram 256`, purpose text
  "MPS-19 session: playsid voice mute check"), audio silent: a real HVSC tune, Rob Hubbard "Commando"
  (MUSICIANS/H/Hubbard_Rob/Commando.sid, the tune the fixture header comes from; copy the real file, not the
  fixture). Classic mode: `MUTE 1 on` -> status stats `rp_mute_calls=1` and the scope for voice 1 goes flat while 2
  and 3 move (text check through the viz/status output, image only through a subagent). reSID mode (step B library
  only): `GetResidAudioBuffer` energy drops with `MUTE 1 on` and returns after the next note with `MUTE 1 off`.
  Johan's ears are the final check only if he asks to listen.

## 6. Open questions for Johan

1. Library change route. Recommended: send the step B patch (`writeSIDRegister` masking + `SetChannelMask`) to the
   playsid.library maintainer and ship nothing of ours; alternative: Selecta bundles a patched fork (needs the
   written permission under `thoughts/shared/licences/` first and the GPL-2 notice for reSID).
2. Ship step A now (classic mode mute via the existing LVO) before the library patch lands. Recommended: yes, it is
   real mute for classic mode today; reSID mode hides mute until the patched library is installed.
3. Old library in reSID or hardware mode: recommended hide mute (voice_count 0, mute controls grey, tooltip
   "needs playsid.library <revision>"); alternative: accept MUTE and do nothing audible (misleading).
4. Multi-SID (chips 2 and 3): recommended include in the library request (`SetChannelMask`), so 2SID/3SID match the
   host's 8/12 channels; alternative: SID 1 only.
5. Voice 3 mute changes OSC3/ENV3 readback in reSID mode (tune logic may change). Recommended: accept, identical to
   the host (libsidplayfp) behaviour, note it in the mute tooltip docs only.

### Answers (Johan, 2026-11-02)

1. Library route: the step B patch goes to Johan's friend who maintains playsid.library with the original author's
   permission: https://github.com/erique/playsid.library. Selecta bundles no fork. Opening a PR or issue there is
   outward-facing: prepare the patch, Johan approves before anything is sent.
2. Ship step A (classic mode mute) now: yes.
3. Old library in reSID or hardware mode: hide mute.
4. Chips 2 and 3: include in the library patch.
5. Voice 3 readback change: accepted (default, same as the host behaviour).
