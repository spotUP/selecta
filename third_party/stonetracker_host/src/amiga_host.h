/*
 * amiga_host.h - the one minimal Amiga every Musashi-hosted replayer runs on
 *
 * Musashi's 68020 (third-party/musashi) with 2 MB of chip RAM, the custom
 * chips a replayer touches (INTENA/INTREQ/DMACON, the beam counters, VBlank,
 * Paula through the shared software Paula tools/asm68k-to-c/runtime/
 * paula_soft.c) and both CIAs (timers A/B, ICR, TOD). Time advances one
 * output sample at a time: the CPU runs its share of cycles, then the CIAs,
 * the beam and Paula advance by the same span and raise their interrupts.
 *
 * Users: stonetracker-wasm (the authors' no-OS StonePlayer) and
 * eagleplayer-wasm (UADE's sound core `score` driving any eagleplayer, see
 * eagle_runner.h). Ledger: thoughts/shared/plans/2026-10-05-musashi-replayer-host.md
 */
#ifndef AMIGA_HOST_H
#define AMIGA_HOST_H
#include <stddef.h>
#include <stdint.h>

#define AH_RAM_SIZE   0x200000u        /* chip RAM, mirrored nowhere */
#define AH_PAULA_HZ   3546895.0        /* PAL colour clock */
#define AH_CIA_HZ     709379.0         /* PAL E clock */
#define AH_CPU_HZ     28375160.0       /* 8 x colour clock (68020 card class) */
#define AH_LINE_CC    227              /* colour clocks per PAL line */
#define AH_FRAME_LINES 313             /* PAL long frame, non-interlaced */

extern uint8_t ah_ram[];

static inline uint16_t ah_rd16(uint32_t a) { return (uint16_t)((ah_ram[a] << 8) | ah_ram[a + 1]); }
static inline uint32_t ah_rd32(uint32_t a) { return ((uint32_t)ah_rd16(a) << 16) | ah_rd16(a + 2); }
static inline void ah_wr16(uint32_t a, uint16_t v) { ah_ram[a] = (uint8_t)(v >> 8); ah_ram[a + 1] = (uint8_t)v; }
static inline void ah_wr32(uint32_t a, uint32_t v) { ah_wr16(a, (uint16_t)(v >> 16)); ah_wr16(a + 2, (uint16_t)v); }

/* Optional hooks a replayer layers on the bus. All may be NULL. */
typedef struct {
  /* A 16-bit RAM read by the instruction at `ppc`: return 1 and set *v to
   * replace the value (StoneTracker's per-track mute), 0 to read RAM. */
  int (*ram_read16)(uint32_t addr, uint32_t ppc, uint16_t *v);
  /* After a byte write to RAM (StoneTracker's note-trigger tap). */
  void (*ram_write8)(uint32_t addr, uint8_t v);
  /* TRAP #n is about to be taken. Return 1 to skip the exception. */
  int (*trap)(int n);
  /* After a write to a custom-chip register ($DFF000 + reg), with the PC of
   * the writing instruction (register taps, traces). */
  void (*custom_write)(uint32_t reg, uint16_t v, uint32_t ppc);
} AhHooks;

/* Clear RAM and every chip, reset the CPU (68020, supervisor, IPL 0) and set
 * the output rate. Hooks are kept until replaced. */
void ah_reset(int sampleRate);
void ah_set_hooks(const AhHooks *hooks);

/* Advance the machine by one output sample. `l`/`r` get Paula's stereo mix
 * (ch 0+3 left, 1+2 right, halved); `voices` (NULL or 4 floats) each voice. */
void ah_step(float *l, float *r, float *voices);

/* Call a routine: push `returnAddr`, load registers (d[0..7], a[0..6]; NULL
 * keeps them), jump to `entry`, run time until the PC is back at
 * `returnAddr` with the stack balanced. Returns 0 and sets *d0, or -1 if it
 * did not return within `maxSamples` output samples or crashed. */
int ah_call(uint32_t entry, const uint32_t *d, const uint32_t *a, uint32_t returnAddr,
            int maxSamples, uint32_t *d0);

/* An address range the PC must never enter (a crash trap). ah_crashed()
 * reports it. */
void ah_set_crash_range(uint32_t addr, uint32_t len);
int ah_crashed(void);

/* Mixer mute/solo: bit N set = Paula voice N audible. */
void ah_set_voice_mask(uint32_t mask);

/* CPU timing as UADE's: every instruction 4 colour clocks (uademain.c
 * m68k_speed = 4), instead of the 68020's own cycle counts. For players
 * whose timing races the chips the way UADE runs them (the eagleplayer
 * runner: UADE is its reference). Takes effect at the next ah_reset(). */
void ah_set_uade_timing(int on);

/* Machine time in colour clocks since reset (diagnostics, beam). */
double ah_colour_clocks(void);

/* Timer underflows that raised a CIA interrupt (ICR mask bit set) since
 * reset: cia 0 = CIA-A, 1 = CIA-B; timer 0 = A, 1 = B. CIA-A timer B is
 * the clock score calls a player's DTP_Interrupt from (UADE counts the same
 * timer as its player tick, cia.c uade_wasm_on_player_tick). */
uint32_t ah_cia_timer_irqs(int cia, int timer);

/* Custom-chip register state for tests. */
uint16_t ah_intena(void);
uint16_t ah_intreq(void);
uint16_t ah_dmacon(void);
#endif
