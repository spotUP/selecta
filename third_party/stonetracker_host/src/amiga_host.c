/*
 * amiga_host.c - the minimal Amiga under every Musashi-hosted replayer
 *
 * See amiga_host.h. Moved out of stonetracker-wasm/src/st_machine.c (first
 * user) so the eagleplayer runner shares the same CPU, bus, CIAs and timing;
 * its private Paula is replaced by the shared software Paula.
 *
 * Time: each output sample runs the CPU for AH_CPU_HZ / sampleRate cycles,
 * then advances the CIAs by the E clock and the beam and Paula by the colour
 * clock. Interrupts raised during a sample are taken at the start of the
 * next CPU slice (at most one output sample of latency). Beam reads inside
 * a slice see the cycles the CPU has run so far, so raster-line waits
 * (MOVE.B $DFF006 / CMP.B $DFF006 loops) progress inside a slice too.
 */
#include "amiga_host.h"
#include "paula_soft.h"

#include "m68k.h"
#include "m68kcpu.h"

#include <string.h>

/* RAM plus a guard: a replayer that points AUDxLC near the top of chip RAM
 * makes Paula read up to 128 KB past it, which must stay inside this array. */
#define RAM_GUARD 0x20000u
uint8_t ah_ram[AH_RAM_SIZE + RAM_GUARD];

static AhHooks g_hooks;

/* ---- custom chips ------------------------------------------------------ */
static uint16_t intena, intreq, dmacon;
static uint32_t audLc[4];

/* Paula's audio state machine, as far as the CPU can see it (interrupts and
 * when a channel really stops). The samples come from the shared software
 * Paula; this decides when it starts and stops and when AUDx is requested,
 * following the chip (HRM audio state diagram; UAE audio.c audio_handler):
 *  - DMA on from idle latches LC/LEN and starts output; the interrupt comes
 *    at the next line (state 1 -> 5 in UAE), not at the write.
 *  - Every block repeat requests AUDx (the software Paula's wrap latch).
 *  - DMA off does NOT stop a playing channel: at each word boundary it
 *    requests AUDx if it is not pending, and goes idle once it is. A DMA on
 *    before it went idle continues the channel (no restart, no interrupt).
 *  - AUDxDAT written to an idle channel with AUDx not pending starts it in
 *    non-DMA mode and requests AUDx at once (UAE AUDxDAT, state 0 -> 2);
 *    it then runs word by word as above and goes idle once AUDx is pending.
 * Players depend on this: Ben Daglish's audio handler stops a voice on
 * AUDx and only re-enables its interrupt while the voice is off - an
 * interrupt at the DMA-on write killed every note it started. Digital
 * Sonix & Chrome starts every note with DMA off, period 1 and a write to
 * AUDxDAT, and busy-waits for the AUDx request it raises before turning
 * DMA on (without it the player hung on its first note, silent).
 * The byte clock runs like UAE's states 2 and 3, in DMA mode too: the first
 * byte starts the line after the start interrupt (state 5), each byte lasts
 * the period AUDxPER holds when it starts, and DMA off finishes the byte
 * playing (the player above writes period 1, then its note's period, around
 * the request, and restarts the note or not by a few colour clocks). The boundaries are caught
 * up to the CPU's time at every register access that depends on them, as
 * UAE's update_audio() does. */
enum { AUD_IDLE, AUD_STARTING, AUD_PLAYING };
typedef struct {
  int state;
  int dmaen;
  uint16_t per;        /* AUDxPER as written (0 = 65536 colour clocks) */
  double startCc;      /* when DMA went on from idle (UAE state 1 until the next line) */
  int prelude;         /* DMA started: the line before its first byte (UAE state 5) */
  int lowByte;         /* in the word's second byte (UAE state 3) */
  double byteEnd;      /* colour-clock time the current byte (or the prelude) ends */
} AudState;
static AudState aud[4];

/* One byte at the channel's period; UAE (audio.c AUDxPER) runs periods
 * under 16 as 16. */
static double byte_cc(int ch) {
  const uint16_t p = aud[ch].per;
  return p == 0 ? 65536.0 : p < 16 ? 16.0 : (double)p;
}

/* ---- CIA --------------------------------------------------------------- */
typedef struct {
  uint8_t regs[16];
  uint16_t taLatch, taCount, tbLatch, tbCount;
  uint8_t cra, crb;
  uint8_t icrData, icrMask;
  uint32_t tod, todLatch;   /* 24-bit event counter, latched on a high-byte read */
  int todLatched, todStopped;
  double eAcc;
  uint32_t irqs[2];         /* timer A / B underflows that raised the interrupt (ICR mask set) */
} Cia;
static Cia ciaA, ciaB;

/* ---- time --------------------------------------------------------------- */
static int g_sampleRate = 48000;
static double g_cpuPerSample, g_ciaPerSample, g_ccPerSample;
static double g_cpuAcc;
static double g_cc;            /* colour clocks at the start of the current sample */
static int g_sliceCycles;      /* cycles granted to the slice running now (0 = none) */
static uint32_t g_frame, g_line;
static uint32_t g_crashAddr, g_crashLen;
static int g_crashed;
static int g_uadeTiming;

/* ======================================================================== */
static void update_ipl(void) {
  int level = 0;
  if (intena & 0x4000) {
    const uint16_t p = intena & intreq & 0x3FFF;
    if (p & 0x2000) level = 6;
    else if (p & 0x1800) level = 5;
    else if (p & 0x0780) level = 4;
    else if (p & 0x0070) level = 3;
    else if (p & 0x0008) level = 2;
    else if (p & 0x0007) level = 1;
  }
  m68k_set_irq((unsigned)level);
}

/* Colour clocks now, including the cycles the running slice has used. */
static double now_cc(void) {
  double cc = g_cc;
  if (g_sliceCycles > 0) cc += (double)m68k_cycles_run() * (AH_PAULA_HZ / AH_CPU_HZ);
  return cc;
}

static void beam(uint32_t *vpos, uint32_t *hpos) {
  const double frameCc = (double)AH_LINE_CC * AH_FRAME_LINES;
  const double cc = now_cc();
  const double inFrame = cc - frameCc * (double)(uint64_t)(cc / frameCc);
  const uint32_t pos = (uint32_t)inFrame;
  *vpos = pos / AH_LINE_CC;
  *hpos = pos % AH_LINE_CC;
}

/* ---- Paula: register writes go to the shared software Paula ------------ */
static void aud_set_lc(int ch) {
  paula_set_sample_ptr(ch, (const int8_t *)(ah_ram + (audLc[ch] & (AH_RAM_SIZE - 2))));
}


/* Non-DMA mode up to colour-clock time `now`: at the end of each word AUDx
 * is requested, or, already pending, the channel goes idle (UAE
 * audio_handler case 3). */
static void aud_words_until(int ch, double now) {
  AudState *a = &aud[ch];
  const uint16_t ip = (uint16_t)(0x0080u << ch);
  if (a->state == AUD_STARTING) {
    /* The first line start after DMA went on: AUDx, and the line before
     * the first byte (UAE state 1 -> 5 at hsync). */
    const double line = ((double)(uint64_t)(a->startCc / AH_LINE_CC) + 1.0) * AH_LINE_CC;
    if (line > now) return;
    a->state = AUD_PLAYING;
    a->prelude = 1;
    a->byteEnd = line + AH_LINE_CC;
    intreq |= ip;
  }
  while (a->state == AUD_PLAYING && a->byteEnd <= now) {
    if (a->prelude) {
      a->prelude = 0;
      a->lowByte = 0;
    } else if (a->dmaen) {
      a->lowByte ^= 1;   /* DMA mode: the byte clock only (requests come from block starts) */
    } else if (!a->lowByte) {
      a->lowByte = 1;
    } else {
      if (intreq & ip) {
        a->state = AUD_IDLE;
        paula_dma_write((uint16_t)(1 << ch));
        break;
      }
      intreq |= ip;
      a->lowByte = 0;
    }
    a->byteEnd += byte_cc(ch);
  }
}

static void aud_catch_up(void) {
  const double now = now_cc();
  for (int i = 0; i < 4; i++) aud_words_until(i, now);
}

static void dmacon_write(uint16_t v) {
  aud_catch_up();
  if (v & 0x8000) dmacon |= (v & 0x7FFF); else dmacon &= (uint16_t)~v;
  /* A channel's DMA is its enable bit AND the master DMAEN. */
  for (int i = 0; i < 4; i++) {
    AudState *a = &aud[i];
    const int now = (dmacon & 0x200) && (dmacon & (1 << i));
    if (now == a->dmaen) continue;
    a->dmaen = now;
    const uint16_t bit = (uint16_t)(1 << i);
    if (now) {
      if (a->state == AUD_IDLE) {
        a->state = AUD_STARTING;
        a->startCc = now_cc();
        paula_dma_write((uint16_t)(0x8000 | bit));   /* latch LC/LEN, start output */
        (void)paula_poll_block_start(i);              /* AUDx comes at the next line */
      }
      /* still playing in non-DMA mode: it simply continues */
    } else if (a->state == AUD_STARTING || (a->state == AUD_PLAYING && a->prelude)) {
      a->state = AUD_IDLE;   /* UAE: DMA off in state 1 or 5 stops the channel */
      paula_dma_write(bit);
    }
    /* AUD_PLAYING otherwise finishes the byte it is playing (byteEnd). */
  }
}

/* Per output sample, after Paula rendered it. */
static void aud_advance(void) {
  for (int i = 0; i < 4; i++) {
    AudState *a = &aud[i];
    const uint16_t ip = (uint16_t)(0x0080u << i);
    if (a->state == AUD_IDLE) continue;
    if (a->state == AUD_PLAYING && a->dmaen) {
      if (paula_poll_block_start(i)) intreq |= ip;   /* block repeat */
      if (!paula_is_active(i)) { a->state = AUD_IDLE; continue; }   /* empty latch */
    }
    aud_words_until(i, g_cc);
  }
}

static void custom_write16(uint32_t reg, uint16_t v) {
  if (g_hooks.custom_write) g_hooks.custom_write(reg, v, m68ki_cpu.ppc);
  switch (reg) {
    case 0x096: dmacon_write(v); break;
    case 0x09A: if (v & 0x8000) intena |= (v & 0x7FFF); else intena &= (uint16_t)~v; update_ipl(); break;
    case 0x09C: aud_catch_up(); if (v & 0x8000) intreq |= (v & 0x7FFF); else intreq &= (uint16_t)~v; update_ipl(); break;
    default:
      if (reg >= 0x0A0 && reg < 0x0E0) {
        const int ch = (int)((reg - 0x0A0) >> 4);
        switch (reg & 0x0F) {
          case 0x0: audLc[ch] = (audLc[ch] & 0x0000FFFFu) | ((uint32_t)v << 16); aud_set_lc(ch); break;
          case 0x2: audLc[ch] = (audLc[ch] & 0xFFFF0000u) | (v & 0xFFFEu); aud_set_lc(ch); break;
          case 0x4: paula_set_length(ch, v); break;
          case 0x6: aud[ch].per = v; paula_set_period(ch, v); break;
          case 0x8: paula_set_volume(ch, (uint8_t)((v & 0x7F) > 64 ? 64 : (v & 0x7F))); break;
          case 0xA: {   /* AUDxDAT */
            aud_catch_up();
            AudState *a = &aud[ch];
            const uint16_t ip = (uint16_t)(0x0080u << ch);
            if (a->state == AUD_IDLE && !a->dmaen && !(intreq & ip)) {
              a->state = AUD_PLAYING;
              a->prelude = 0;
              a->lowByte = 0;
              a->byteEnd = now_cc() + byte_cc(ch);
              intreq |= ip;
              update_ipl();
            }
            break;
          }
          default: break;
        }
      }
      break;  /* ADKCON, copper, blitter, display: not sound */
  }
}

static uint16_t custom_read16(uint32_t reg) {
  uint32_t v, h;
  switch (reg) {
    case 0x002: return dmacon & 0x07FF;
    case 0x004: beam(&v, &h); return (uint16_t)(0x2000 | ((v >> 8) & 1));  /* ECS Agnus id $20, PAL */
    case 0x006: beam(&v, &h); return (uint16_t)(((v & 0xFF) << 8) | (h & 0xFF));
    case 0x01C: return intena;
    case 0x01E: aud_catch_up(); return intreq;
    default: return 0;
  }
}

/* ---- CIA ---------------------------------------------------------------- */
static void cia_raise(Cia *c, uint8_t bit, int isB) {
  c->icrData |= bit;
  if (c->icrMask & bit) {
    if (bit & 0x03) c->irqs[bit >> 1]++;
    intreq |= isB ? 0x2000 : 0x0008;   /* EXTER (level 6) / PORTS (level 2) */
    update_ipl();
  }
}

static void cia_write(Cia *c, int reg, uint8_t v, int isB) {
  c->regs[reg] = v;
  switch (reg) {
    case 0x4: c->taLatch = (uint16_t)((c->taLatch & 0xFF00) | v); break;
    case 0x5:
      c->taLatch = (uint16_t)((c->taLatch & 0x00FF) | (v << 8));
      if (!(c->cra & 0x01)) c->taCount = c->taLatch;
      if (c->cra & 0x08) { c->taCount = c->taLatch; c->cra |= 0x01; }   /* one-shot: write starts */
      break;
    case 0x6: c->tbLatch = (uint16_t)((c->tbLatch & 0xFF00) | v); break;
    case 0x7:
      c->tbLatch = (uint16_t)((c->tbLatch & 0x00FF) | (v << 8));
      if (!(c->crb & 0x01)) c->tbCount = c->tbLatch;
      if (c->crb & 0x08) { c->tbCount = c->tbLatch; c->crb |= 0x01; }
      break;
    /* TOD: writing the high byte stops it, the low byte starts it again. */
    case 0x8: if (!(c->crb & 0x80)) { c->tod = (c->tod & 0xFFFF00u) | v; c->todStopped = 0; } break;
    case 0x9: if (!(c->crb & 0x80)) c->tod = (c->tod & 0xFF00FFu) | ((uint32_t)v << 8); break;
    case 0xA: if (!(c->crb & 0x80)) { c->tod = (c->tod & 0x00FFFFu) | ((uint32_t)v << 16); c->todStopped = 1; } break;
    case 0xD:
      if (v & 0x80) c->icrMask |= (v & 0x7F); else c->icrMask &= (uint8_t)~v;
      if (c->icrData & c->icrMask) { intreq |= isB ? 0x2000 : 0x0008; update_ipl(); }
      break;
    case 0xE:
      if (v & 0x10) c->taCount = c->taLatch;   /* force load strobe */
      c->cra = v & (uint8_t)~0x10;
      break;
    case 0xF:
      if (v & 0x10) c->tbCount = c->tbLatch;
      c->crb = v & (uint8_t)~0x10;
      break;
    default: break;
  }
}

static uint8_t cia_read(Cia *c, int reg) {
  switch (reg) {
    case 0x4: return (uint8_t)c->taCount;
    case 0x5: return (uint8_t)(c->taCount >> 8);
    case 0x6: return (uint8_t)c->tbCount;
    case 0x7: return (uint8_t)(c->tbCount >> 8);
    /* TOD: reading the high byte latches all three until the low byte. */
    case 0x8: { const uint32_t t = c->todLatched ? c->todLatch : c->tod; c->todLatched = 0; return (uint8_t)t; }
    case 0x9: return (uint8_t)((c->todLatched ? c->todLatch : c->tod) >> 8);
    case 0xA: c->todLatch = c->tod; c->todLatched = 1; return (uint8_t)(c->tod >> 16);
    case 0xD: {
      uint8_t r = c->icrData;
      if (c->icrData & c->icrMask) r |= 0x80;
      c->icrData = 0;              /* reading clears */
      return r;
    }
    case 0xE: return c->cra;
    case 0xF: return c->crb;
    default: return c->regs[reg];
  }
}

static void cia_tick_timer(Cia *c, uint16_t *count, uint16_t latch, uint8_t *cr,
                           uint8_t bit, int isB, uint32_t ticks) {
  if (!(*cr & 0x01)) return;
  while (ticks > 0) {
    const uint32_t untilUnder = (uint32_t)*count + 1u;   /* counts latch..0, underflows next */
    if (ticks < untilUnder) { *count = (uint16_t)(*count - ticks); return; }
    ticks -= untilUnder;
    *count = latch;
    cia_raise(c, bit, isB);
    if (*cr & 0x08) { *cr &= (uint8_t)~0x01; return; }   /* one-shot stops */
  }
}

static void cia_advance(Cia *c, int isB) {
  c->eAcc += g_ciaPerSample;
  const uint32_t ticks = (uint32_t)c->eAcc;
  if (!ticks) return;
  c->eAcc -= ticks;
  cia_tick_timer(c, &c->taCount, c->taLatch, &c->cra, 0x01, isB, ticks);
  cia_tick_timer(c, &c->tbCount, c->tbLatch, &c->crb, 0x02, isB, ticks);
}

static void cia_tod_pulse(Cia *c) {
  if (!c->todStopped) c->tod = (c->tod + 1) & 0xFFFFFFu;
}

/* ---- Musashi bus -------------------------------------------------------- */
static uint8_t io_read8(uint32_t a) {
  if ((a & 0xFFF000u) == 0xBFD000u && !(a & 1)) return cia_read(&ciaB, (int)((a >> 8) & 0xF));
  if ((a & 0xFFF000u) == 0xBFE000u && (a & 1)) return cia_read(&ciaA, (int)((a >> 8) & 0xF));
  if ((a & 0xFFF000u) == 0xDFF000u) {
    const uint16_t w = custom_read16(a & 0x1FEu);
    return (uint8_t)((a & 1) ? w : (w >> 8));
  }
  return 0;
}

static void io_write8(uint32_t a, uint8_t v) {
  if ((a & 0xFFF000u) == 0xBFD000u && !(a & 1)) { cia_write(&ciaB, (int)((a >> 8) & 0xF), v, 1); return; }
  if ((a & 0xFFF000u) == 0xBFE000u && (a & 1)) { cia_write(&ciaA, (int)((a >> 8) & 0xF), v, 0); return; }
  if ((a & 0xFFF000u) == 0xDFF000u) custom_write16(a & 0x1FEu, (uint16_t)((v << 8) | v));
}

unsigned int m68k_read_memory_8(unsigned int a) {
  a &= 0xFFFFFFu;
  if (a < AH_RAM_SIZE) return ah_ram[a];
  return io_read8(a);
}

unsigned int m68k_read_memory_16(unsigned int a) {
  a &= 0xFFFFFFu;
  if (a + 1 < AH_RAM_SIZE) {
    if (g_hooks.ram_read16) {
      uint16_t v;
      if (g_hooks.ram_read16(a, m68ki_cpu.ppc, &v)) return v;
    }
    return ah_rd16(a);
  }
  if ((a & 0xFFF000u) == 0xDFF000u) return custom_read16(a & 0x1FEu);
  return (unsigned)((io_read8(a) << 8) | io_read8(a + 1));
}

unsigned int m68k_read_memory_32(unsigned int a) {
  return (m68k_read_memory_16(a) << 16) | m68k_read_memory_16(a + 2);
}

void m68k_write_memory_8(unsigned int a, unsigned int v) {
  a &= 0xFFFFFFu;
  if (a < AH_RAM_SIZE) {
    ah_ram[a] = (uint8_t)v;
    if (g_hooks.ram_write8) g_hooks.ram_write8(a, (uint8_t)v);
    return;
  }
  io_write8(a, (uint8_t)v);
}

void m68k_write_memory_16(unsigned int a, unsigned int v) {
  a &= 0xFFFFFFu;
  if (a + 1 < AH_RAM_SIZE) { ah_wr16(a, (uint16_t)v); return; }
  if ((a & 0xFFF000u) == 0xDFF000u) { custom_write16(a & 0x1FEu, (uint16_t)v); return; }
  io_write8(a, (uint8_t)(v >> 8));
  io_write8(a + 1, (uint8_t)v);
}

void m68k_write_memory_32(unsigned int a, unsigned int v) {
  m68k_write_memory_16(a, v >> 16);
  m68k_write_memory_16(a + 2, v & 0xFFFF);
}

unsigned int m68k_read_disassembler_8(unsigned int a) { return m68k_read_memory_8(a); }
unsigned int m68k_read_disassembler_16(unsigned int a) { return m68k_read_memory_16(a); }
unsigned int m68k_read_disassembler_32(unsigned int a) { return m68k_read_memory_32(a); }

/* M68K_TRAP_CALLBACK (compile define): TRAP #n before its exception. */
int ah_trap_callback(int n) { return g_hooks.trap ? g_hooks.trap(n) : 0; }

/* ---- time --------------------------------------------------------------- */
static void advance_beam(double ccStart, double ccEnd) {
  /* Lines and frames crossed during this sample: CIA-B TOD counts lines
   * (HSYNC), CIA-A TOD and VERTB count frames. */
  const uint64_t lineEnd = (uint64_t)(ccEnd / AH_LINE_CC);
  while ((uint64_t)g_line < lineEnd) {
    g_line++;
    cia_tod_pulse(&ciaB);
    if (g_line % AH_FRAME_LINES == 0) {
      g_frame++;
      cia_tod_pulse(&ciaA);
      intreq |= 0x0020;   /* VERTB */
    }
  }
  (void)ccStart;
}

void ah_step(float *l, float *r, float *voices) {
  g_cpuAcc += g_cpuPerSample;
  const int cycles = (int)g_cpuAcc;
  g_cpuAcc -= cycles;
  if (cycles > 0) {
    g_sliceCycles = cycles;
    m68k_execute(cycles);
    g_sliceCycles = 0;
  }
  const uint32_t pc = m68k_get_reg(NULL, M68K_REG_PC);
  if (g_crashLen && pc >= g_crashAddr && pc < g_crashAddr + g_crashLen) g_crashed = 1;

  cia_advance(&ciaB, 1);
  cia_advance(&ciaA, 0);
  const double ccStart = g_cc;
  g_cc += g_ccPerSample;
  advance_beam(ccStart, g_cc);

  float st[2];
  paula_render_voices(st, voices, 1, 1);
  aud_advance();
  update_ipl();
  *l = st[0];
  *r = st[1];
}

int ah_call(uint32_t entry, const uint32_t *d, const uint32_t *a, uint32_t returnAddr,
            int maxSamples, uint32_t *d0) {
  uint32_t sp = m68k_get_reg(NULL, M68K_REG_SP);
  sp -= 4;
  ah_wr32(sp, returnAddr);
  m68k_set_reg(M68K_REG_SP, sp);
  if (d) for (int i = 0; i < 8; i++) m68k_set_reg((m68k_register_t)(M68K_REG_D0 + i), d[i]);
  if (a) for (int i = 0; i < 7; i++) m68k_set_reg((m68k_register_t)(M68K_REG_A0 + i), a[i]);
  m68ki_cpu.stopped = 0;
  m68k_set_reg(M68K_REG_PC, entry);
  float l, r;
  for (int n = 0; n < maxSamples; n++) {
    ah_step(&l, &r, NULL);
    if (g_crashed) return -1;
    const uint32_t pc = m68k_get_reg(NULL, M68K_REG_PC);
    if (pc >= returnAddr && pc < returnAddr + 6 && m68k_get_reg(NULL, M68K_REG_SP) == sp + 4) {
      if (d0) *d0 = m68k_get_reg(NULL, M68K_REG_D0);
      return 0;
    }
  }
  return -1;
}

void ah_reset(int sampleRate) {
  if (sampleRate > 0) g_sampleRate = sampleRate;
  g_cpuPerSample = AH_CPU_HZ / g_sampleRate;
  g_ciaPerSample = AH_CIA_HZ / g_sampleRate;
  g_ccPerSample = AH_PAULA_HZ / g_sampleRate;
  memset(ah_ram, 0, sizeof ah_ram);
  memset(audLc, 0, sizeof audLc);
  memset(aud, 0, sizeof aud);
  memset(&ciaA, 0, sizeof ciaA);
  memset(&ciaB, 0, sizeof ciaB);
  ciaA.taLatch = ciaA.tbLatch = ciaB.taLatch = ciaB.tbLatch = 0xFFFF;
  ciaA.taCount = ciaA.tbCount = ciaB.taCount = ciaB.tbCount = 0xFFFF;
  intena = intreq = dmacon = 0;
  g_cpuAcc = 0.0;
  g_cc = 0.0;
  g_sliceCycles = 0;
  g_frame = g_line = 0;
  g_crashed = 0;
  g_crashAddr = g_crashLen = 0;

  paula_reset();
  paula_set_clock((float)AH_PAULA_HZ);
  paula_set_output_rate((float)g_sampleRate);

  m68k_init();
  m68k_set_cpu_type(M68K_CPU_TYPE_68020);
  if (g_uadeTiming) {
    /* Every instruction costs 4 colour clocks, as in UADE (uademain.c
     * m68k_speed = 4: newcpu.c replaces each instruction's cycle count by
     * 4, in colour clocks). */
    static uint8_t uadeCycles[0x10000], uadeExceptions[256];
    const uint8_t four = (uint8_t)(4.0 * AH_CPU_HZ / AH_PAULA_HZ + 0.5);
    if (!uadeCycles[0]) {
      memset(uadeCycles, four, sizeof uadeCycles);
      /* An exception instruction (TRAP, CHK, ...) is one instruction; an
       * interrupt costs nothing (UAE Interrupt() takes no cycles). */
      memset(uadeExceptions, four, sizeof uadeExceptions);
      memset(uadeExceptions + 24, 0, 8);   /* spurious + autovectors 1-7 */
      memset(uadeExceptions + 64, 0, 192); /* user interrupt vectors */
    }
    m68ki_cpu.cyc_instruction = uadeCycles;
    m68ki_cpu.cyc_exception = uadeExceptions;
    /* No per-operand extras (shift count, MOVEM registers, branch taken). */
    m68ki_cpu.cyc_bcc_notake_b = m68ki_cpu.cyc_bcc_notake_w = 0;
    m68ki_cpu.cyc_dbcc_f_noexp = m68ki_cpu.cyc_dbcc_f_exp = 0;
    m68ki_cpu.cyc_scc_r_true = 0;
    m68ki_cpu.cyc_movem_w = m68ki_cpu.cyc_movem_l = 0;
    m68ki_cpu.cyc_shift = 0;
  }
  m68k_pulse_reset();
  m68k_set_reg(M68K_REG_SR, 0x2000);   /* supervisor, all levels open */
  m68k_set_irq(0);
}

void ah_set_hooks(const AhHooks *hooks) {
  if (hooks) g_hooks = *hooks; else memset(&g_hooks, 0, sizeof g_hooks);
}

void ah_set_crash_range(uint32_t addr, uint32_t len) { g_crashAddr = addr; g_crashLen = len; }
int ah_crashed(void) { return g_crashed; }

void ah_set_voice_mask(uint32_t mask) {
  for (int i = 0; i < 4; i++) paula_set_channel_gain(i, (mask >> i) & 1 ? 1.0f : 0.0f);
}

double ah_colour_clocks(void) { return now_cc(); }
void ah_set_uade_timing(int on) { g_uadeTiming = on ? 1 : 0; }
uint16_t ah_intena(void) { return intena; }
uint32_t ah_cia_timer_irqs(int cia, int timer) {
  const Cia *c = cia ? &ciaB : &ciaA;
  return c->irqs[timer ? 1 : 0];
}
uint16_t ah_intreq(void) { return intreq; }
uint16_t ah_dmacon(void) { return dmacon; }
