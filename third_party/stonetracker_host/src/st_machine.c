/*
 * st_machine.c - a minimal Amiga that runs StonePlayer_Hard.bin
 *
 * Memory map (24-bit, 2 MB chip RAM, VBR = 0):
 *   $000000  exception vectors
 *   $000400  idle loop: STOP #$2000 / BRA.S $400 - the "main program"
 *   $000408  RTE - default autovector handler
 *   $000410  STOP #$2700 / BRA.S $410 - unexpected exception (crash) trap
 *   $001000  the player's 4140 bytes of chip work memory
 *   $010000  StonePlayer_Hard.bin
 *   $040000  SPM module
 *   $060000  SPS bank, unpacked
 *   $1FFF00  supervisor stack top
 *   $BFD000  CIA-B (even addresses), $BFE001 CIA-A (odd)
 *   $DFF000  custom chips
 *
 * The machine (CPU, chips, CIAs, time, the shared Paula) is musashi-host's
 * (musashi-host/src/amiga_host.c); this file holds what is StonePlayer's:
 * the memory layout, the CPlay.c call sequence, the player patch, the
 * per-track mute and the per-track taps.
 */
#include "st_machine.h"
#include "st_dhuf.h"
#include "amiga_host.h"

#include "m68k.h"

#include <string.h>

#define RAM_SIZE      AH_RAM_SIZE
#define ADDR_IDLE     0x000400u
#define ADDR_RTE      0x000408u
#define ADDR_CRASH    0x000410u
#define ADDR_CHIPBUF  0x001000u
#define ADDR_PLAYER   0x010000u
#define ADDR_SPM      0x040000u
#define ADDR_SPS      0x060000u
#define ADDR_STACK    0x1FFF00u
#define PAULA_HZ      AH_PAULA_HZ

/* Player jump table (StonePlayer_Bin.I) */
#define SP_INSTALLPLAYER     0x00
#define SP_INSTALLMODULE     0x08
#define SP_INITPLAYER        0x0C
#define SP_STARTPLAYER       0x10
#define SP_STOPPLAYER        0x14
#define SP_SETPLAYERPOS      0x18
#define SP_SLIDESONGVOLUME   0x28
#define SP_SLIDEBALANCE      0x2C
#define SP_CHECKMODULE       0x34

/* Player variables: a5 = player + $48D8 (lea $48D8(pc),a5 in every entry). */
#define PLAYER_VARS   (ADDR_PLAYER + 0x48D8u)
#define VAR_SONG      206u
#define VAR_POSITION  208u
#define VAR_LINE      212u
#define VAR_VOICES    278u     /* 8 track structures, 108 bytes each */
#define VOICE_SIZE    108u
#define VOICE_VOLUME  26u      /* current (sample/effect) volume, word */

/* The seven instructions that read a track's volume to build its Paula or
 * mix volume (move.w aN@(26),dX). A muted track reads 0 there and only
 * there, so its effects, slides and sample positions keep running. */
static const uint32_t k_volume_read_pcs[7] = {
  0x0A40, 0x2AC8, 0x2D24, 0x2D9E, 0x35AE, 0x3624, 0x369A,
};

/* ---- run state -------------------------------------------------------- */
static int g_loaded = 0;
static int g_sampleRate = 48000;
static uint32_t g_muteMask = 0xFF;

/* ---- per-track taps (scopes / VU / channel classifiers) ------------------
 * Tracks the player sends straight to a Paula channel are tapped from that
 * channel's output - exact. Tracks it mixes in software (5-8 tracks: 3 with
 * 6 and 7 into Paula 2, 4 with 5 and 8 into Paula 3) have no separate audio
 * anywhere, so each gets a shadow voice: restarted from the track structure
 * the moment the player triggers its note (it sets byte 72), then stepped at
 * the track's live period (+22) through the same sample bytes and loop
 * (+32 start, +36 length, +40/+44 loop) at its live volume (+26 x +28 x song
 * volume). Taps only read: the main mix is unchanged. */
typedef struct {
  int active;
  double pos;          /* byte address in chip RAM */
  double remaining;    /* bytes left before the loop / end */
} ShadowVoice;
static ShadowVoice g_shadow[8];
static float g_paulaOut[4];

/* ---- bus hooks ---------------------------------------------------------- */
static int track_of_volume_read(uint32_t a) {
  const uint32_t base = PLAYER_VARS + VAR_VOICES;
  if (a < base || a >= base + 8 * VOICE_SIZE) return -1;
  const uint32_t off = a - base;
  if (off % VOICE_SIZE != VOICE_VOLUME) return -1;
  return (int)(off / VOICE_SIZE);
}

static int is_volume_read_pc(uint32_t ppc) {
  if (ppc < ADDR_PLAYER) return 0;
  const uint32_t o = ppc - ADDR_PLAYER;
  for (int i = 0; i < 7; i++) if (k_volume_read_pcs[i] == o) return 1;
  return 0;
}

/* A masked track's volume reads 0 at the seven volume-building reads. */
static int mute_read16(uint32_t a, uint32_t ppc, uint16_t *v) {
  if (g_muteMask == 0xFF) return 0;
  const int t = track_of_volume_read(a);
  if (t < 0 || (g_muteMask & (1u << t)) || !is_volume_read_pc(ppc)) return 0;
  *v = 0;
  return 1;
}

static uint32_t rd32(uint32_t a) { return ah_rd32(a); }
static uint16_t rd16(uint32_t a) { return ah_rd16(a); }

/* The player set byte 72 of a track structure: a note (re)starts from +32. */
static void shadow_restart(int t) {
  const uint32_t v = PLAYER_VARS + VAR_VOICES + (uint32_t)t * VOICE_SIZE;
  ShadowVoice *sv = &g_shadow[t];
  sv->pos = rd32(v + 32) & 0x1FFFFFu;
  sv->remaining = rd32(v + 36);
  sv->active = sv->remaining > 0;
}

static void note_write8(uint32_t a, uint8_t v) {
  const uint32_t base = PLAYER_VARS + VAR_VOICES;
  if (v && a >= base && a < base + 8 * VOICE_SIZE && (a - base) % VOICE_SIZE == 72)
    shadow_restart((int)((a - base) / VOICE_SIZE));
}

/* ---- time -------------------------------------------------------------- */
static void step_sample(float *l, float *r) { ah_step(l, r, g_paulaOut); }

/* Call a player entry with the given registers and run the machine until it
 * returns to the idle loop. Time runs (the install waits on a CIA-B timer B
 * interrupt). Returns D0, or 0x7FFFFFFF if it never returned. */
static int32_t call_player(uint32_t entry, uint32_t d0, uint32_t d1, uint32_t d2, uint32_t d3,
                           uint32_t a0, uint32_t a1, uint32_t a2) {
  const uint32_t d[8] = { d0, d1, d2, d3, 0, 0, 0, 0 };
  const uint32_t a[7] = { a0, a1, a2, 0, 0, 0, 0 };
  uint32_t r = 0;
  if (ah_call(ADDR_PLAYER + entry, d, a, ADDR_IDLE, g_sampleRate * 4, &r) != 0) return 0x7FFFFFFF;
  return (int32_t)r;
}

/* The player's SOFT-interrupt mixer handler ($EF4) ends with
 *
 *   $F3E  tst.b  180(a5)        ; more work queued?
 *   $F42  bne.s  $F1A           ; yes: loop
 *   $F44  move.w #$0004,$DFF09C ; no: clear the SOFT request
 *
 * A CIA-B tick or a mix-buffer audio interrupt taken at $F42 or $F44 sees
 * 180(a5) = 0, queues its work there and sets INTREQ SOFT - which $F44 then
 * clears. 180(a5) stays non-zero, so no later interrupt requests SOFT again:
 * the tick never runs again, the song freezes on one line and the mixing
 * period climbs to $3C1 (measured: hypnosphere froze at 108 s, position 13
 * line 53, with a CIA-B interrupt pending at PC $F42). Clearing the request
 * before the last check closes the window; the instructions are the same,
 * reordered:
 *
 *   $F3E  move.w #$0004,$DFF09C
 *   $F46  tst.b  180(a5)
 *   $F4A  bne.s  $F1A
 */
static void patch_soft_interrupt_race(void) {
  static const uint8_t before[14] = { 0x4A, 0x2D, 0x00, 0xB4, 0x66, 0xD6,
                                      0x33, 0xFC, 0x00, 0x04, 0x00, 0xDF, 0xF0, 0x9C };
  static const uint8_t after[14] = { 0x33, 0xFC, 0x00, 0x04, 0x00, 0xDF, 0xF0, 0x9C,
                                     0x4A, 0x2D, 0x00, 0xB4, 0x66, 0xCE };
  uint8_t *at = ah_ram + ADDR_PLAYER + 0x0F3E;
  if (memcmp(at, before, sizeof before) == 0) memcpy(at, after, sizeof after);
}

static void reset_machine(void) {
  static const AhHooks hooks = { mute_read16, note_write8, NULL };
  ah_reset(g_sampleRate);
  ah_set_hooks(&hooks);
  ah_set_voice_mask(0xF);   /* per-track mute is the player's, never Paula's */
  ah_set_crash_range(ADDR_CRASH, 6);
  memset(g_shadow, 0, sizeof g_shadow);

  /* vectors: everything unexpected traps to the crash loop; autovectors
   * and the spurious interrupt return. */
  for (uint32_t v = 2; v < 256; v++) ah_wr32(v * 4, ADDR_CRASH);
  for (uint32_t v = 24; v <= 31; v++) ah_wr32(v * 4, ADDR_RTE);
  ah_wr32(0, ADDR_STACK);
  ah_wr32(4, ADDR_IDLE);
  ah_wr16(ADDR_IDLE + 0, 0x4E72); ah_wr16(ADDR_IDLE + 2, 0x2000);   /* STOP #$2000 */
  ah_wr16(ADDR_IDLE + 4, 0x60FA);                                    /* BRA.S *-4  */
  ah_wr16(ADDR_RTE, 0x4E73);                                         /* RTE */
  ah_wr16(ADDR_CRASH + 0, 0x4E72); ah_wr16(ADDR_CRASH + 2, 0x2700); /* STOP #$2700 */
  ah_wr16(ADDR_CRASH + 4, 0x60FA);
  m68k_set_reg(M68K_REG_SP, ADDR_STACK);
}

/* Selecta: StonePlayer_Hard.bin is never embedded; the adapter hands over the user's copy (system file). */
static const uint8_t *g_playerBin;
static size_t g_playerLen;
int st_machine_set_player(const uint8_t *bin, size_t len) {
  if (!bin || len < 0x1000 || len > 0x30000) return -1;   /* must fit between $10000 and the SPM at $40000 */
  g_playerBin = bin; g_playerLen = len;
  return 0;
}

int st_machine_load(const uint8_t *spm, size_t spmLen, const uint8_t *sps, size_t spsLen,
                    int song, int sampleRate) {
  g_loaded = 0;
  if (!spm || !sps || sampleRate <= 0 || !g_playerBin) return -1;
  if (spmLen < 52 || spm[0] != 'S' || spm[1] != 'P' || spm[2] != 'M') return -2;
  if (spsLen < 6 || sps[0] != 'S' || sps[1] != 'P' || sps[2] != 'S') return -3;
  if (ADDR_SPM + spmLen > ADDR_SPS) return -2;

  g_sampleRate = sampleRate;
  reset_machine();

  memcpy(ah_ram + ADDR_PLAYER, g_playerBin, g_playerLen);
  patch_soft_interrupt_race();
  memcpy(ah_ram + ADDR_SPM, spm, spmLen);

  /* The sample bank: headers as they are, data unpacked behind them. */
  const size_t nSamples = sps[5];
  const size_t hdr = 6 + nSamples * 32;
  if (spsLen < hdr) return -3;
  const int method = sps[4] & 0x0F;
  if (ADDR_SPS + hdr > ADDR_STACK - 0x1000) return -3;
  memcpy(ah_ram + ADDR_SPS, sps, hdr);
  if (method == 0) {
    if (ADDR_SPS + spsLen > ADDR_STACK - 0x1000) return -3;
    memcpy(ah_ram + ADDR_SPS + hdr, sps + hdr, spsLen - hdr);
  } else if (method == 1) {
    const int len = st_dhuf_unpacked_length(sps + hdr, spsLen - hdr);
    if (len < 0 || ADDR_SPS + hdr + (size_t)len > ADDR_STACK - 0x1000) return -3;
    if (st_dhuf_depack(sps + hdr, spsLen - hdr, hdr, ah_ram + ADDR_SPS + hdr, (size_t)len) != len) return -3;
    ah_ram[ADDR_SPS + 4] &= 0xF0;   /* now an unpacked bank */
  } else {
    return -3;   /* CrunchMania / StoneCruncher banks: not seen in the corpus */
  }

  /* spInstallPlayer(D0 = AttnFlags 68010|68020, A0 = chip work memory, A1 = VBR) */
  if (call_player(SP_INSTALLPLAYER, 0x0003, 0, 0, 0, ADDR_CHIPBUF, 0, 0) != 0) return ah_crashed() ? -5 : -4;
  /* spInstallModule(A0 = module, A1 = sample bank, A2 = 0: data follows headers) */
  if (call_player(SP_INSTALLMODULE, 0, 0, 0, 0, ADDR_SPM, ADDR_SPS, 0) != 0) return ah_crashed() ? -5 : -4;
  /* As CPlay.c: init, full volume, centre balance, song, start. */
  call_player(SP_INITPLAYER, 0, 0, 0, 0, 0, 0, 0);
  call_player(SP_SLIDESONGVOLUME, 64, 0, 0, 0, 0, 0, 0);
  call_player(SP_SLIDEBALANCE, 128, 0, 0, 0, 0, 0, 0);
  call_player(SP_SETPLAYERPOS, (uint32_t)(song > 0 ? song : 1), 0, 0, 0, 0, 0, 0);
  call_player(SP_STARTPLAYER, 0, 0, 0, 0, 0, 0, 0);
  if (ah_crashed()) return -5;
  g_loaded = 1;
  return 0;
}

/* One output sample of a mixed track's shadow voice. */
static float shadow_sample(int t) {
  ShadowVoice *sv = &g_shadow[t];
  const uint32_t v = PLAYER_VARS + VAR_VOICES + (uint32_t)t * VOICE_SIZE;
  if (!sv->active || rd32(v + 36) == 0) return 0.0f;   /* cut / one-shot over */
  const uint16_t period = rd16(v + 22);
  if (!period) return 0.0f;
  const uint32_t at = (uint32_t)sv->pos;
  const int8_t s = at < RAM_SIZE ? (int8_t)ah_ram[at] : 0;
  double vol = (double)rd16(v + VOICE_VOLUME) * (double)rd16(v + 28) / 4096.0
             * (double)rd16(PLAYER_VARS + 148) / 64.0;
  if (vol > 1.0) vol = 1.0;
  const double step = PAULA_HZ / (double)period / (double)g_sampleRate;
  sv->pos += step;
  sv->remaining -= step;
  if (sv->remaining <= 0.0) {
    const uint32_t loopLen = rd32(v + 44);
    if (!loopLen) { sv->active = 0; }
    else {
      const double over = -sv->remaining;
      const double into = over - loopLen * (double)(uint32_t)(over / loopLen);
      sv->pos = (rd32(v + 40) & 0x1FFFFFu) + into;
      sv->remaining = loopLen - into;
    }
  }
  return (float)(s / 128.0 * vol);
}

/* Paula channel a track plays on directly, or -1 if the player mixes it. */
static int direct_channel(int t, int voices) {
  if (t == 0 || t == 1) return t;
  if (t == 2) return voices < 6 ? 2 : -1;
  if (t == 3) return voices < 5 ? 3 : -1;
  return -1;
}

static void render(float *out, float *tracks, int frames, int stride) {
  if (!g_loaded) {
    memset(out, 0, (size_t)frames * 2 * sizeof(float));
    if (tracks) memset(tracks, 0, (size_t)stride * 8 * sizeof(float));
    return;
  }
  for (int i = 0; i < frames; i++) {
    step_sample(&out[2 * i], &out[2 * i + 1]);
    if (!tracks) continue;
    const int voices = rd16(PLAYER_VARS + 202);
    for (int t = 0; t < 8; t++) {
      float v = 0.0f;
      if (t < voices) {
        const int c = direct_channel(t, voices);
        v = c >= 0 ? g_paulaOut[c] : shadow_sample(t);
        if (!(g_muteMask & (1u << t))) v = 0.0f;
      }
      tracks[t * stride + i] = v;
    }
  }
}

int st_machine_render(float *out, int frames) { render(out, NULL, frames, 0); return frames; }

int st_machine_render_channels(float *out, float *tracks, int frames, int stride) {
  if (stride < frames) stride = frames;
  render(out, tracks, frames, stride);
  return frames;
}

void st_machine_set_mute_mask(uint32_t mask) { g_muteMask = mask & 0xFF; }

void st_machine_stop(void) {
  if (g_loaded) call_player(SP_STOPPLAYER, 1 /* VOF_CUTDMA */, 0, 0, 0, 0, 0, 0);
  g_loaded = 0;
}

int st_machine_get_position(void) { return rd16(PLAYER_VARS + VAR_POSITION); }
int st_machine_get_line(void) { return rd16(PLAYER_VARS + VAR_LINE); }
int st_machine_get_song(void) { return rd16(PLAYER_VARS + VAR_SONG); }
int st_machine_crashed(void) { return ah_crashed(); }
