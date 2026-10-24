# KPlayer hardware start under vamos (I219c)

Cause (confirmed): vamos cannot provide CIA resources, so KP_INIT returns -1 and the start is refused.

Evidence
- Trace, Amiga Selecta under vamos (`-l exec:info`, one M.K. module, keys Down/Enter): 
  `OpenResource(ciaa.resource) ignored`, `OpenResource(ciab.resource) ignored`, then `hw_handovers=1 hw_starts=0 replayer=none`.
- vamos source: amitools/vamos/lib/ExecLibrary.py:696-700, `OpenResource` logs "ignored" and `return 0` for every name.
- third_party/kplayer/kpl14.s:~618-680: OpenResource(ciaa.resource) -> beq .se; OpenResource(ciab.resource) -> beq k_err;
  k_err (`moveq #-1,d0; rts`) is the only failure return of the init.
- plugins/replayers/kplayer/adapter.c:131: `kp_call(KP_INIT,...) != 0` -> rp_start returns 0.
- src/engine/engine.c:1241-1250: rp_start 0 -> no hw_starts++, `handover_for(.., SEL_ROUTE_MIXER)` gives the output back.

Decision: exemption, not a fake. Faking ciab.resource would mean editing vamos or third_party; neither is wanted, and it would
prove nothing about the real CIA cadence. Audible playback, CIA cadence, pause, end of song stay rig-only (T56).

Test asserting the expected path (new; no existing test asserted hw_starts==0): ctest `amiga-kplayer-start-refused-vamos`
(tests/m2/kplayer_vamos_start.cmake, registered in tests/CMakeLists.txt next to amiga-console-keys-vamos). Asserts exit 0
(no crash or hang), hw_handovers==1, hw_starts==0, replayer==none, and that the trace shows `OpenResource(ciab.resource) ignored`.
