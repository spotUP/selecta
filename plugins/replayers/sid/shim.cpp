/* libsidplayfp (reSIDfp engine) behind a C interface (row I434). Modelled on bbmp's csrc/sidshim.cpp. */
#include "shim.h"

#include <sidplayfp/SidConfig.h>
#include <sidplayfp/SidInfo.h>
#include <sidplayfp/SidTune.h>
#include <sidplayfp/SidTuneInfo.h>
#include <sidplayfp/sidplayfp.h>
#include <builders/residfp-builder/residfp.h>

#include <cstring>
#include <vector>

struct sid_shim {
    ReSIDfpBuilder builder; /* declared before the engine: the engine is destroyed first */
    sidplayfp engine;
    SidTune* tune;
    std::vector<uint8_t> image;
    std::vector<short> mix;
    size_t mixlen, mixpos;
    sid_shim() : builder("selecta"), tune(nullptr), mixlen(0), mixpos(0) {}
    ~sid_shim() { delete tune; }
};

extern "C" {

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

int sid_shim_start(sid_shim* s, uint32_t song, uint32_t rate) {
    SidConfig cfg = s->engine.config();
    cfg.frequency = rate;
    cfg.samplingMethod = SidConfig::INTERPOLATE;
    cfg.sidEmulation = &s->builder;
    if (!s->engine.config(cfg)) {
        return 0;
    }
    s->tune->selectSong(song);
    if (!s->engine.load(s->tune)) {
        return 0;
    }
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

void sid_shim_close(sid_shim* s) { delete s; }

}
