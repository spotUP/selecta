/* RP_SAMPLE_GET for the openmpt adapter (row I97, Q2 default).
 *
 * libopenmpt's public API names samples but exposes no sample data, so this
 * file reads the sample table of the CSoundFile behind an openmpt_module. It
 * lives in the plugin build next to the vendored library and compiles against
 * its internal headers; libopenmpt's own sources are not modified. Two
 * couplings to the pinned version (0.8.4) are made here and nowhere else:
 *   1. struct openmpt_module is defined in libopenmpt_c.cpp, not in a header,
 *      so its layout is repeated below (first fields up to `impl`);
 *   2. module_impl::m_sndFile is protected, reached through a derived class
 *      (the standard pointer-to-member access).
 * Called by adapter.c on the thread that owns the module (like the tracker
 * cells hook). */
#include "common/stdafx.h"

#include "libopenmpt/libopenmpt_internal.h"
#include "libopenmpt/libopenmpt.h"
#include "libopenmpt/libopenmpt.hpp"
#include "libopenmpt/libopenmpt_impl.hpp"

#include <selecta/replayer.h>

#include "soundlib/Sndfile.h"

#include <cstring>

namespace {

/* Layout copy of struct openmpt_module in libopenmpt_c.cpp (0.8.4). */
struct module_layout {
    openmpt_log_func logfunc;
    void* loguser;
    openmpt_error_func errfunc;
    void* erruser;
    int error;
    const char* error_message;
    openmpt::module_impl* impl;
};

struct sound_file_access : openmpt::module_impl {
    static OpenMPT::CSoundFile& get(openmpt::module_impl& m) {
        return *(m.*(&sound_file_access::m_sndFile));
    }
};

} // namespace

extern "C" int selecta_openmpt_sample_get(openmpt_module* mod, uint32_t n, sel_rp_sample* out);

/* MOD-family samples play at the PAL C-3 rate. */
#define SEL_MOD_BASE_RATE 8287u

extern "C" int selecta_openmpt_sample_get(openmpt_module* mod, uint32_t n, sel_rp_sample* out) {
    OpenMPT::CSoundFile& snd = sound_file_access::get(*reinterpret_cast<module_layout*>(mod)->impl);
    const OpenMPT::SAMPLEINDEX count = snd.GetNumSamples();
    out->count = count;
    if (n < 1u || n > count) {
        return 0;
    }
    const OpenMPT::ModSample& s = snd.GetSample(static_cast<OpenMPT::SAMPLEINDEX>(n));
    if (s.pData.pSample == nullptr || s.nLength == 0u) {
        return 0;
    }
    const bool looped = s.uFlags[OpenMPT::CHN_LOOP] && s.nLoopEnd > s.nLoopStart;
    out->data = s.pData.pSample;
    out->length = s.nLength;
    out->bits = (s.uFlags[OpenMPT::CHN_16BIT]) ? 16u : 8u;
    out->channels = (s.uFlags[OpenMPT::CHN_STEREO]) ? 2u : 1u;
    out->loop_start = looped ? s.nLoopStart : 0u;
    out->loop_length = looped ? s.nLoopEnd - s.nLoopStart : 0u;
    if (snd.GetType() == OpenMPT::MOD_TYPE_MOD) {
        out->rate = SEL_MOD_BASE_RATE;
    } else {
        out->rate = s.nC5Speed != 0u ? s.nC5Speed : 8363u;
    }
    out->name = snd.GetSampleName(static_cast<OpenMPT::SAMPLEINDEX>(n));
    return 1;
}
