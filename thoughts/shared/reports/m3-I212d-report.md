# I212d: render --rate 22050 reports rendered_frames=44100

Verdict: not a bug. No code change.

## Cause
`apps/selecta-cli/render.c` line 307: `total = seconds * rate`. For `--seconds 2 --rate 22050` that is 44100 frames,
which is exactly 2.0 s at 22050 Hz. At `--rate 44100` the same command pumps 88200 frames. The value 44100 is the
arithmetic coincidence 2 * 22050, not the 44.1 kHz source count. render_calls=87 is ceil(44100/512).

## Desktop evidence (build/host, Debug)
- sine.mp3 (0.34 s fixture): `--rate 22050` gives rendered_frames=7488, WAV header 0x5622 (22050 Hz); `--rate 44100`
  gives rendered_frames=14976, header 0xAC44 (44100 Hz). The replayer honours the rate (frames scale 2:1).
- ambient.mod (tracker replayer, 2 s): `--rate 22050` gives mixed_frames=44100, WAV 176444 bytes = 44100 frames of
  16-bit stereo = 2.0 s at 22050 Hz. Same behaviour, so it is not format-specific.
- The requested regression expectation (22050 frames at 22050 Hz for 2 s) is wrong: 22050 frames at 22050 Hz is 1 s.

## Equal wall time at both rates
Decode dominates on 020 (RTF 60 to 75, see m3-I194-rig-timing-report.md): the decoder decodes the same 2 s of source
either way, and the resample/write step is small. This is the expected cost profile, not a rate-ignored symptom.
Whether the decoders could skip work at a lower output rate is a speed question, tracked by I212c.

## Done
No code, tests or goldens changed. Suggest closing I212d as not-a-bug; the I194 timing table is valid as written.
