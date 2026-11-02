/* libsidplayfp (reSIDfp engine) behind a C interface (row I434). Modelled on bbmp's csrc/sidshim.cpp. */
#include "shim.h"

#include <sidplayfp/SidConfig.h>
#include <sidplayfp/SidInfo.h>
#include <sidplayfp/SidTune.h>
#include <sidplayfp/SidTuneInfo.h>
#include <sidplayfp/sidplayfp.h>
#include <builders/residfp-builder/residfp.h>

#include "openroms.h"

#include <cstring>
#include <vector>

struct sid_shim {
    ReSIDfpBuilder builder; /* declared before the engine: the engine is destroyed first */
    sidplayfp engine;
    SidTune* tune;
    std::vector<uint8_t> image;
    std::vector<short> mix;
    size_t mixlen, mixpos;
    uint32_t cfg_applied;
    sid_shim() : builder("selecta"), tune(nullptr), mixlen(0), mixpos(0), cfg_applied(0) {}
    ~sid_shim() { delete tune; }
};

extern "C" {

const uint8_t* sid_shim_builtin_kernal(void) { return sid_openrom_kernal; }
const uint8_t* sid_shim_builtin_basic(void) { return sid_openrom_basic; }
const uint8_t* sid_shim_builtin_chargen(void) { return sid_openrom_chargen; }

sid_shim* sid_shim_open(const uint8_t* data, size_t size) {
    /* libsidplayfp indexes &buf[dataOffset] without checking it; refuse a damaged header first. */
    if (size < 0x76 || (std::memcmp(data, "PSID", 4) != 0 && std::memcmp(data, "RSID", 4) != 0) ||
        (((size_t)data[6] << 8) | data[7]) >= size) {
        return nullptr;
    }
    sid_shim* s = new sid_shim();
    s->image.assign(data, data + size);
    s->tune = new SidTune(s->image.data(), (uint_least32_t)size);
    if (!s->tune->getStatus()) {
        delete s;
        return nullptr;
    }
    s->mix.resize(1u << 16);
    return s;
}

int sid_shim_start(sid_shim* s, uint32_t song, uint32_t rate, const sid_shim_cfg* c) {
    SidConfig cfg = s->engine.config();
    cfg.frequency = rate;
    cfg.samplingMethod = SidConfig::INTERPOLATE;
    cfg.sidEmulation = &s->builder;
    cfg.forceSidModel = c->model != 0;
    cfg.defaultSidModel = c->model == 2 ? SidConfig::MOS8580 : SidConfig::MOS6581;
    cfg.forceC64Model = c->clock != 0;
    cfg.defaultC64Model = c->clock == 2 ? SidConfig::NTSC : SidConfig::PAL;
    /* Auto digi boost follows the chip: on for an 8580 (forced, else the tune's own model). */
    bool is8580 = c->model != 0 ? c->model == 2 : s->tune->getInfo()->sidModel(0) == SidTuneInfo::SIDMODEL_8580;
    cfg.digiBoost = c->digiboost == 1 || (c->digiboost == 0 && is8580);
    if (!s->engine.config(cfg)) {
        return 0;
    }
    s->engine.setRoms(c->kernal, c->basic, c->chargen);
    s->tune->selectSong(song);
    if (!s->engine.load(s->tune)) {
        return 0;
    }
    for (unsigned i = 0; i < s->tune->getInfo()->sidChips(); i++) {
        s->engine.filter(i, c->filter != 0);
    }
    s->cfg_applied++;
    s->engine.initMixer(true);
    s->mixlen = s->mixpos = 0;
    return 1;
}

uint32_t sid_shim_songs(const sid_shim* s) { return s->tune->getInfo()->songs(); }
uint32_t sid_shim_start_song(const sid_shim* s) { return s->tune->getInfo()->startSong(); }

const char* sid_shim_info(const sid_shim* s, uint32_t index) {
    const SidTuneInfo* info = s->tune->getInfo();
    return index < info->numberOfInfoStrings() ? info->infoString(index) : "";
}

const char* sid_shim_format(const sid_shim* s) { return s->tune->getInfo()->formatString(); }

uint32_t sid_shim_render(sid_shim* s, int16_t* out, uint32_t frames) {
    size_t want = (size_t)frames * 2u, got = 0;
    while (got < want) {
        if (s->mixpos >= s->mixlen) {
            int n = s->engine.play(5000);
            if (n < 0) {
                break;
            }
            s->mixlen = s->engine.mix(s->mix.data(), (unsigned)n);
            s->mixpos = 0;
            if (s->mixlen == 0) {
                continue;
            }
        }
        size_t take = s->mixlen - s->mixpos;
        if (take > want - got) {
            take = want - got;
        }
        std::memcpy(out + got, s->mix.data() + s->mixpos, take * sizeof(short));
        s->mixpos += take;
        got += take;
    }
    return (uint32_t)(got / 2u);
}

uint32_t sid_shim_cfg_applied(const sid_shim* s) { return s->cfg_applied; }

void sid_shim_close(sid_shim* s) { delete s; }

}
