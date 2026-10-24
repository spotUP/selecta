/* AHI output defaults per CPU class (plan M2 phase 4, I125, D-M2-5): the buffer length in frames and the
 * sample rate follow the machine; the `output.buffer` pref (nonzero) overrides the length. Integer only. */
#ifndef SELECTA_AHI_CFG_H
#define SELECTA_AHI_CFG_H

#include <selecta/host.h>
#include <selecta/machine.h>

#include <stdint.h>

#define SEL_AHI_BUF_MIN 64u
#define SEL_AHI_BUF_MAX 16384u

/* Frames per AHI buffer: 68000/68010 8192, 68020 4096, 68030 2048, 68040 and up (and the desktop) 1024. */
uint32_t sel_ahi_buffer_frames(uint32_t cpu);
/* Default rate in Hz: 28150 up to the 68030, 44100 on 68040 and up (and the desktop). */
uint32_t sel_ahi_default_rate(uint32_t cpu);
/* `override` (the output.buffer pref text, "" or "0" for none) else the table; out-of-range overrides are ignored. */
uint32_t sel_ahi_pick_buffer(uint32_t cpu, const char* override);
/* CPU class of the running machine (sel_machine_probe). */
uint32_t sel_ahi_host_cpu(const sel_host* host);

#endif
