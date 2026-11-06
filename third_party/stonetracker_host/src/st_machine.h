/*
 * st_machine.h - a minimal Amiga for StonePlayer_Hard.bin
 *
 * The authors' no-OS StoneTracker player (StonePlayer V1.98, Michael
 * Lavaire / Emmanuel Marty) runs on Musashi's 68020 core with the three
 * pieces of hardware it touches: Paula (4 audio DMA channels, INTENA/INTREQ,
 * DMACON), CIA-B (timer A = tick, timer B = DMA restart one-shot) and the
 * VPOSR PAL bit. Decision record:
 * thoughts/shared/research/2026-10-05_stonetracker-replayer.md
 */
#ifndef ST_MACHINE_H
#define ST_MACHINE_H
#include <stddef.h>
#include <stdint.h>

/* The user's StonePlayer_Hard.bin (kept by pointer, must outlive the machine). Required before st_machine_load. */
int st_machine_set_player(const uint8_t *bin, size_t len);

/* Load an SPM module and its SPS sample bank (packed or not), install the
 * player and start song `song` (1-based) from position 0. Returns 0, or
 * -1 bad arguments, -2 not an SPM module, -3 not an SPS bank / bad pack,
 * -4 the player refused (install or module), -5 the CPU crashed. */
int st_machine_load(const uint8_t *spm, size_t spmLen, const uint8_t *sps, size_t spsLen,
                    int song, int sampleRate);

/* Render interleaved stereo float frames. Returns frames written. */
int st_machine_render(float *out, int frames);

/* As st_machine_render, plus 8 planar per-track float buffers in `ch`,
 * `stride` floats apart (>= frames). Tracks on a Paula channel are that
 * channel's output; software-mixed tracks are shadow voices (st_machine.c). */
int st_machine_render_channels(float *out, float *ch, int frames, int stride);

/* Bit N set = track N+1 (0-7) audible. */
void st_machine_set_mute_mask(uint32_t mask);

void st_machine_stop(void);

/* Player position (song, position, line) from spGetPlayerData. */
int st_machine_get_position(void);
int st_machine_get_line(void);
int st_machine_get_song(void);

/* 1 once the song has ended: the player's VL_SongEnd fired, or it came back to a position it already played. Poll often (about 50 Hz). */
int st_machine_song_ended(void);

/* Diagnostics: 1 if the CPU hit an unexpected exception. */
int st_machine_crashed(void);
#endif
