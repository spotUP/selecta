/* In-process TIATracker engine (I429): plain C interface over the tracker's song and replay code (third_party/tiatracker,
 * GPL-2.0-only) and Stella's TIA sound. Replaces bbmp-tiatracker's stdout protocol. */
#ifndef SELECTA_TIATRACKER_ENGINE_H
#define SELECTA_TIATRACKER_ENGINE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct tia_engine tia_engine;
/* Parses and validates a .ttt (JSON) and measures its length; NULL when it is not a playable song. */
tia_engine* tia_open(const uint8_t* data, size_t size);
void tia_close(tia_engine* e);
const char* tia_title(const tia_engine* e);
const char* tia_author(const tia_engine* e);
uint32_t tia_duration_ms(const tia_engine* e);
/* Restarts the song from its first row at an output rate. */
int tia_start(tia_engine* e, uint32_t rate);
/* Interleaved stereo 16-bit; returns the frames written (fewer than asked at the end of the song). */
uint32_t tia_render(tia_engine* e, int16_t* out, uint32_t frames);
#ifdef __cplusplus
}
#endif
#endif
