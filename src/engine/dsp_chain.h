/* The engine's DSP chain (design doc section 5, plan M1.3 I99): a fixed
 * array of SEL_DSP_CHAIN_SLOTS slots between the mixer and the output. The
 * pump thread opens, configures and closes plugins; the audio thread only
 * runs the slots whose `on` flag is set. */
#ifndef SELECTA_ENGINE_DSP_CHAIN_H
#define SELECTA_ENGINE_DSP_CHAIN_H

#include <selecta/dsp.h>

typedef struct sel_dsp_slot {
    const char* id; /* registry plugin id, NULL: slot free */
    sel_dsp_open_fn open;
    sel_dsp_process_fn process;
    sel_dsp_close_fn close;
    sel_dsp_param_fn param;
    void* state;
    uint32_t rate;
    volatile uint32_t on; /* pump writes, audio reads */
} sel_dsp_slot;

typedef struct sel_dsp_chain {
    sel_dsp_slot slot[SEL_DSP_CHAIN_SLOTS];
    uint32_t blocks; /* audio thread: process calls that ran at least one slot */
} sel_dsp_chain;

/* Pump thread. Opens plugin `id` (hooks list of a DSP descriptor) at `rate`
 * into a free slot, or switches the already open slot on. 1: running. */
int sel_dsp_chain_enable(sel_dsp_chain* c, const sel_host* host, const char* id, const sel_tag* hooks,
                         uint32_t rate);
/* Pump thread. Switches the slot off; the plugin stays open for the next enable. */
void sel_dsp_chain_disable(sel_dsp_chain* c, const char* id);
/* Pump thread. 1 when a slot for `id` is on. */
int sel_dsp_chain_is_on(const sel_dsp_chain* c, const char* id);
/* Pump thread. Reopens every opened slot at a new rate (the slots go off). */
void sel_dsp_chain_set_rate(sel_dsp_chain* c, const sel_host* host, uint32_t rate);
/* Pump thread. Closes every slot. */
void sel_dsp_chain_free(sel_dsp_chain* c);
/* Audio thread. Runs the slots that are on, in order, in place. */
void sel_dsp_chain_process(sel_dsp_chain* c, int16_t* stereo, uint32_t frames);

#endif
