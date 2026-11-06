// paula_soft.h — Amiga Paula chip emulator, ONE copy for every software-Paula
// replayer (asm68k-to-c output and the hand-written C engines alike). Engines
// compile tools/asm68k-to-c/runtime/paula_soft.c directly; never copy it.
// Output: F32 stereo interleaved at the configured output rate
// (default PAULA_RATE_PAL = 28150 Hz).
#pragma once
#include <stdint.h>

#define PAULA_RATE_PAL   28150
#define PAULA_RATE_NTSC  28836
#define PAULA_CLOCK_PAL  3546895.0f
#define PAULA_CLOCK_NTSC 3579545.0f
#define PAULA_CHANNELS   4

// ── Registers ──────────────────────────────────────────────────────────────
// AUDxLC / AUDxLEN writes land in the channel's registers and are LATCHED, as
// on the chip: copied into the playing buffer when the channel's DMA goes from
// off to on, and again every time the buffer runs out (the repeat). A write
// while a channel plays sets what plays NEXT, never cuts the current pass.
void paula_set_sample_ptr(int ch, const int8_t* data);   // AUDxLC
void paula_set_length(int ch, uint16_t len_words);       // AUDxLEN (words)
void paula_set_period(int ch, uint16_t period);          // AUDxPER (immediate)
void paula_set_volume(int ch, uint8_t vol);              // AUDxVOL 0-64 (immediate)
// DMACON: $8xxx sets, $0xxx clears the low four audio bits. Setting the bit of
// a channel that already runs changes nothing (edge-triggered, as the chip).
void paula_dma_write(uint16_t dmacon);

// ── audio.device layer (MaxTrax harness) ───────────────────────────────────
// Loop mode: 1 (default) = the chip's own repeat; 0 = one-shot: at the end of
// the buffer the channel swaps to the queued follow-on, or stops.
void paula_set_loop(int ch, int loop);
// Queue a follow-on buffer for a one-shot channel. period (0 = keep current)
// and vol (0-64) are applied atomically on the swap, so there is no gap.
void paula_set_next(int ch, const int8_t* data, uint16_t len_words, int loop,
                    uint16_t period, uint8_t vol);
// 1 while the channel's DMA runs, 0 once it stopped (one-shot ended / off).
int paula_is_active(int ch);
// Audio-block-done signal: 1 (and cleared) if a one-shot buffer ended since
// the last poll, whether it then swapped to the follow-on or stopped.
int paula_poll_completion(int ch);
// Stop one channel and drop its queued follow-on (CMD_FLUSH note-off).
void paula_channel_dma_off(int ch);

// ── Setup / mixer ──────────────────────────────────────────────────────────
// Clears all channel state. The per-channel user gain (mute/solo) is NOT
// cleared: it belongs to the mixer, and reset runs on every song load.
void paula_reset(void);
void paula_set_clock(float paula_clock);      // default PAL
void paula_set_output_rate(float rate);       // default PAULA_RATE_PAL
void paula_set_channel_gain(int ch, float gain);  // 0 = mute, 1 = unity

// Render `frames` of F32 stereo interleaved audio (ch0+3 left, ch1+2 right,
// each side halved). Returns frames written.
int paula_render(float* buffer, int frames);

// As paula_render, plus each voice's post-gain output: voices[ch * stride + i]
// (voices may be NULL). For per-voice outputs (scopes, dub sends, isolation).
int paula_render_voices(float* buffer, float* voices, int stride, int frames);

// AUDx interrupt source: 1 (and cleared) if the channel latched AUDxLC/AUDxLEN
// since the last poll - when its DMA started and at every repeat. The chip
// raises the channel's audio interrupt there; a host with a CPU (musashi-host)
// turns this into INTREQ AUDx.
int paula_poll_block_start(int ch);

// Per-channel peak levels (0-1) since the last call; reading resets them.
void paula_get_channel_levels(float* out4);
// Cumulative note starts per channel (DMA off->on edges); zeroed by paula_reset.
void paula_get_note_start_counts(uint32_t* out, int n);

// ── Scope / trace / debug ──────────────────────────────────────────────────
// Per-channel oscilloscope rings (int16, post-gain), written by paula_render.
uintptr_t paula_scope_ptr(int ch);
int       paula_scope_len(void);
int       paula_scope_pos(void);
// Raw register values, for song-level lock tests.
uint16_t  paula_reg_period(int ch);
uint8_t   paula_reg_volume(int ch);
uint32_t  paula_reg_len_bytes(int ch);
uintptr_t paula_reg_sample_ptr(int ch);
uint16_t  paula_last_dmacon(void);
// [dma, vol, step, pos, playing len, has buffer, has LC register, LEN register]
void paula_debug_state(int ch, float* out8);

// ── Note capture (pattern import from a running replayer) ──────────────────
// Records period+volume changes per channel per tick.
#define PAULA_CAPTURE_MAX 16384
typedef struct {
    uint32_t tick;
    uint16_t period;
    uint8_t  volume;
    uint8_t  channel;
} PaulaNoteEvent;
void paula_capture_start(void);
void paula_capture_stop(void);
void paula_capture_tick(void);  // once per replayer tick
int  paula_capture_count(void);
const PaulaNoteEvent* paula_capture_buffer(void);
