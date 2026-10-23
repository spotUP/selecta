/* Sample export writers (plan M1.3 phase 5, I98). Portable C, no float.
 * Both writers stream through a sink so the caller owns the file. */
#ifndef SEL_LIBRARY_EXPORT_H
#define SEL_LIBRARY_EXPORT_H

#include <stdint.h>

#include <selecta/replayer.h>

typedef int (*sel_export_sink)(void* ctx, const void* bytes, uint32_t len); /* 1 = ok */

/* Mono PCM WAV: 8-bit unsigned (stored signed + 128) or 16-bit little endian. */
int sel_export_wav(const sel_rp_sample* s, sel_export_sink sink, void* ctx);
/* IFF 8SVX: signed 8-bit body (16-bit data keeps its high byte), VHDR chunk. */
int sel_export_svx(const sel_rp_sample* s, sel_export_sink sink, void* ctx);

#endif
