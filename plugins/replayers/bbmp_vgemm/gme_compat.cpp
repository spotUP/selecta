/* The gme.h calls of bbmp's vgemm_engine.cpp over libvgm (row I710ah). The engine hands over a plain VGM image (YM2612 + SN76489 writes). */
#include "gme/gme.h"

#include <string.h>
#include <vector>

#include "player/playera.hpp"
#include "player/vgmplayer.hpp"
#include "utils/DataLoader.h"
#include "utils/MemoryLoader.h"

struct Music_Emu {
    PlayerA player;
    std::vector<unsigned char> image;
    DATA_LOADER* loader = nullptr;
    int rate = 0;
    bool started = false;
};

gme_err_t gme_open_data(const void* data, long size, Music_Emu** out, int rate) {
    *out = nullptr;
    if (data == nullptr || size <= 0 || rate <= 0) {
        return "bad argument";
    }
    Music_Emu* e = new Music_Emu();
    e->rate = rate;
    e->image.assign(static_cast<const unsigned char*>(data), static_cast<const unsigned char*>(data) + size);
    e->player.RegisterPlayerEngine(new VGMPlayer);
    e->loader = MemoryLoader_Init(e->image.data(), static_cast<UINT32>(e->image.size()));
    if (e->loader == nullptr) {
        gme_delete(e);
        return "out of memory";
    }
    DataLoader_SetPreloadBytes(e->loader, 0x100);
    if (DataLoader_Load(e->loader) != 0 || e->player.LoadFile(e->loader) != 0) {
        gme_delete(e);
        return "Wrong file type for this emulator";
    }
    *out = e;
    return nullptr;
}

void gme_ignore_silence(Music_Emu*, int) {}

gme_err_t gme_start_track(Music_Emu* e, int) {
    if (e->player.SetOutputSettings(static_cast<UINT32>(e->rate), 2, 16, 4096) != 0) {
        return "output settings";
    }
    PlayerA::Config c = e->player.GetConfiguration();
    c.masterVol = 0x10000;
    c.loopCount = 1;
    c.fadeSmpls = 0;
    c.endSilenceSmpls = 0;
    c.pbSpeed = 1.0;
    e->player.SetConfiguration(c);
    if (e->player.Start() != 0) {
        return "start failed";
    }
    e->started = true;
    return nullptr;
}

gme_err_t gme_play(Music_Emu* e, int count, short* out) {
    if (!e->started) {
        return "not started";
    }
    int frames = count / 2, done = 0;
    while (done < frames) {
        UINT32 want = static_cast<UINT32>(frames - done);
        if (want > 4096u) {
            want = 4096u;
        }
        UINT32 got = e->player.Render(want * 4u, out + 2 * done) / 4u;
        if (got == 0u) {
            break;
        }
        done += static_cast<int>(got);
    }
    if (done < frames) {
        memset(out + 2 * done, 0, static_cast<size_t>(frames - done) * 4u);
    }
    return nullptr;
}

void gme_delete(Music_Emu* e) {
    if (e == nullptr) {
        return;
    }
    e->player.Stop();
    e->player.UnloadFile();
    e->player.UnregisterAllPlayers();
    if (e->loader != nullptr) {
        DataLoader_Deinit(e->loader);
    }
    delete e;
}
