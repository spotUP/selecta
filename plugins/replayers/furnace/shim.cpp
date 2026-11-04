/* Furnace wrapper (row I430): runs the headless DivEngine with the dummy audio backend and pulls audio through
 * nextBuf, the same call the GUI's audio thread makes. No config directory, config file, log file or device is
 * touched (DivEngine::preInitHeadless). */
#include "shim.h"

#include "engine/engine.h"
#include "ta-log.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>

extern FILE* logOut; /* defined in the engine's log.cpp, no header declares it */

namespace {
constexpr unsigned CHUNK = 1024u;
}

/* The engine reports file errors through this GUI hook (declared in ta-utils.h); headless there is no one to show it to. */
void reportError(String what) {
    (void)what;
}

struct furnace_shim {
    DivEngine* eng = nullptr;
    bool inited = false;
    uint32_t rate = 0;
    uint32_t subsongs = 1;
    std::string info[3];
    float left[CHUNK], right[CHUNK];
    float* planes[2] = {left, right};
    bool ended = false;
};

extern "C" furnace_shim* furnace_shim_open(const uint8_t* data, size_t size, const char* name) {
    if (data == nullptr || size < 21u) {
        return nullptr;
    }
    logLevel = LOGLEVEL_ERROR;
    logOut = stderr; /* the engine's log sink is NULL until the GUI or CLI sets it */
    furnace_shim* s = new furnace_shim;
    s->eng = new DivEngine;
    s->eng->preInitHeadless();
    /* load() takes ownership of the buffer and frees it with delete[]. */
    unsigned char* copy = new unsigned char[size];
    std::memcpy(copy, data, size);
    if (!s->eng->load(copy, size, name != nullptr ? name : "")) {
        delete s->eng;
        delete s;
        return nullptr;
    }
    DivSong* song = &s->eng->song;
    s->subsongs = (uint32_t)std::max<size_t>(1u, song->subsong.size());
    s->info[0] = song->name;
    s->info[1] = song->author;
    s->info[2] = song->systemName;
    return s;
}

extern "C" int furnace_shim_start(furnace_shim* s, uint32_t rate, uint32_t subsong) {
    if (s == nullptr || rate < 8000u || subsong >= s->subsongs) {
        return 0;
    }
    if (s->inited && rate != s->rate) {
        /* The chip emulators are fixed to the rate chosen at init. */
        s->eng->quit(false);
        s->inited = false;
    }
    if (!s->inited) {
        s->eng->setAudio(DIV_AUDIO_DUMMY);
        s->eng->setConf("audioRate", (int)rate);
        s->eng->setConf("audioChans", 2);
        if (!s->eng->init()) {
            return 0;
        }
        s->inited = true;
        s->rate = rate;
    } else {
        s->eng->stop();
    }
    s->eng->changeSongP(subsong);
    s->eng->setLoops(1);
    s->ended = false;
    s->eng->play();
    return 1;
}

extern "C" const char* furnace_shim_info(const furnace_shim* s, uint32_t index) {
    return s != nullptr && index < 3u ? s->info[index].c_str() : "";
}

extern "C" uint32_t furnace_shim_subsongs(const furnace_shim* s) {
    return s != nullptr ? s->subsongs : 1u;
}

extern "C" uint32_t furnace_shim_render(furnace_shim* s, int16_t* out, uint32_t frames) {
    uint32_t done = 0;
    if (s == nullptr || !s->inited) {
        return 0;
    }
    while (done < frames && !s->ended) {
        uint32_t n = std::min<uint32_t>(frames - done, CHUNK);
        if (!s->eng->isPlaying()) {
            s->ended = true;
            break;
        }
        s->eng->nextBuf(nullptr, s->planes, 0, 2, n);
        for (uint32_t i = 0; i < n; i++) {
            float l = std::max(-1.0f, std::min(1.0f, s->left[i]));
            float r = std::max(-1.0f, std::min(1.0f, s->right[i]));
            out[2u * (done + i)] = (int16_t)std::lrintf(l * 32767.0f);
            out[2u * (done + i) + 1u] = (int16_t)std::lrintf(r * 32767.0f);
        }
        done += n;
        if (!s->eng->isPlaying()) {
            s->ended = true;
        }
    }
    return done;
}

extern "C" void furnace_shim_close(furnace_shim* s) {
    if (s == nullptr) {
        return;
    }
    if (s->inited) {
        s->eng->quit(false);
    }
    delete s->eng;
    delete s;
}
