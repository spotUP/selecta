/* libvgm wrapper (row I431): PlayerA with the VGM, S98, DRO and GYM engines over an in-memory image. */
#include "shim.h"

#include <string.h>
#include <vector>

#include "player/playera.hpp"
#include "player/vgmplayer.hpp"
#include "player/s98player.hpp"
#include "player/droplayer.hpp"
#include "player/gymplayer.hpp"
#include "utils/DataLoader.h"
#include "utils/MemoryLoader.h"

struct vgm_shim {
    PlayerA player;
    std::vector<uint8_t> image;
    DATA_LOADER* loader = nullptr;
    uint32_t rate = 0, total = 0;
    char info[4][128];
    char format[16];
};

static void copy_text(char* dst, size_t cap, const char* src) {
    strncpy(dst, src != nullptr ? src : "", cap - 1u);
    dst[cap - 1u] = '\0';
}

vgm_shim* vgm_shim_open(const uint8_t* data, size_t size) {
    if (data == nullptr || size == 0u || size > 0x7fffffffu) {
        return nullptr;
    }
    vgm_shim* s = new vgm_shim();
    memset(s->info, 0, sizeof s->info);
    memset(s->format, 0, sizeof s->format);
    s->image.assign(data, data + size);
    s->player.RegisterPlayerEngine(new VGMPlayer);
    s->player.RegisterPlayerEngine(new S98Player);
    s->player.RegisterPlayerEngine(new DROPlayer);
    s->player.RegisterPlayerEngine(new GYMPlayer);
    s->loader = MemoryLoader_Init(s->image.data(), (UINT32)s->image.size());
    if (s->loader == nullptr) {
        vgm_shim_close(s);
        return nullptr;
    }
    DataLoader_SetPreloadBytes(s->loader, 0x100);
    if (DataLoader_Load(s->loader) != 0 || s->player.LoadFile(s->loader) != 0) {
        vgm_shim_close(s);
        return nullptr;
    }
    PlayerBase* eng = s->player.GetPlayer();
    copy_text(s->format, sizeof s->format, eng->GetPlayerName());
    for (const char* const* t = eng->GetTags(); t != nullptr && t[0] != nullptr; t += 2) {
        if (strcmp(t[0], "TITLE") == 0) {
            copy_text(s->info[0], sizeof s->info[0], t[1]);
        } else if (strcmp(t[0], "ARTIST") == 0) {
            copy_text(s->info[1], sizeof s->info[1], t[1]);
        } else if (strcmp(t[0], "GAME") == 0) {
            copy_text(s->info[2], sizeof s->info[2], t[1]);
        } else if (strcmp(t[0], "SYSTEM") == 0) {
            copy_text(s->info[3], sizeof s->info[3], t[1]);
        }
    }
    return s;
}

int vgm_shim_start(vgm_shim* s, uint32_t rate) {
    if (s == nullptr || rate == 0u) {
        return 0;
    }
    // No Stop() here: libvgm's Stop() tears the device tree down (the VGM engine clears its device configs), so a
    // Start() after it plays silence. A fresh handle is started once; the engine opens a new handle to replay.
    if (s->player.SetOutputSettings(rate, 2, 16, 4096) != 0) {
        return 0;
    }
    PlayerA::Config c = s->player.GetConfiguration();
    c.masterVol = 0x10000;
    c.loopCount = 1;
    c.fadeSmpls = 0;
    c.endSilenceSmpls = 0;
    c.pbSpeed = 1.0;
    s->player.SetConfiguration(c);
    if (s->player.Start() != 0) {
        return 0;
    }
    PlayerBase* eng = s->player.GetPlayer();
    s->rate = rate;
    s->total = eng->Tick2Sample(eng->GetTotalPlayTicks(1));
    return 1;
}

const char* vgm_shim_info(const vgm_shim* s, uint32_t index) { return index < 4u ? s->info[index] : ""; }

const char* vgm_shim_format(const vgm_shim* s) { return s->format; }

uint32_t vgm_shim_total_frames(const vgm_shim* s) { return s->total; }

uint32_t vgm_shim_render(vgm_shim* s, int16_t* out, uint32_t frames) {
    uint32_t done = 0;
    while (done < frames) {
        uint32_t pos = s->player.GetCurPos(PLAYPOS_SAMPLE);
        if (pos >= s->total) {
            break;
        }
        uint32_t want = frames - done;
        if (want > 4096u) {
            want = 4096u;
        }
        if (want > s->total - pos) {
            want = s->total - pos;
        }
        uint32_t got = s->player.Render(want * 4u, out + 2u * done) / 4u;
        done += got;
        if (got == 0u) {
            break;
        }
    }
    return done;
}

void vgm_shim_close(vgm_shim* s) {
    if (s == nullptr) {
        return;
    }
    s->player.Stop();
    s->player.UnloadFile();
    s->player.UnregisterAllPlayers();
    if (s->loader != nullptr) {
        DataLoader_Deinit(s->loader);
    }
    delete s;
}
