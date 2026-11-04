/* AdPlug wrapper (row I696): CAdPlug::factory over an in-memory image, Nuked OPL3 as the chip emulator. The player
 * ticks at its own refresh rate; the OPL is run for one tick of samples between update() calls (the same loop as
 * AdPlug's own players). */
#include "shim.h"

#include <string.h>
#include <string>
#include <vector>

#include "adplug.h"
#include "nemuopl.h"
#include "fprovide.h"
#include "binstr.h"

namespace {

/* The song itself comes from memory; companion files named by the player come from the song's directory. */
class MemoryProvider : public CFileProvider {
public:
    MemoryProvider(const std::vector<uint8_t>& image, const std::string& name) : image_(image), name_(name) {}
    binistream* open(std::string filename) const override {
        if (filename == name_) {
            binisstream* st = new binisstream(const_cast<uint8_t*>(image_.data()), (unsigned long)image_.size());
            st->setFlag(binio::BigEndian, false);
            st->setFlag(binio::FloatIEEE);
            return st;
        }
        return CProvider_Filesystem().open(filename);
    }
    void close(binistream* f) const override { delete f; }

private:
    const std::vector<uint8_t>& image_;
    std::string name_;
};

void copy_text(char* dst, size_t cap, const std::string& src) {
    strncpy(dst, src.c_str(), cap - 1u);
    dst[cap - 1u] = '\0';
}

} // namespace

struct adplug_shim {
    std::vector<uint8_t> image;
    std::string name;
    CNemuopl* opl = nullptr;
    CPlayer* player = nullptr;
    uint32_t rate = 0;
    double per_tick = 0, accum = 0;
    bool ended = false;
    char info[4][128];
    uint32_t subsongs = 1;
};

static void drop_player(adplug_shim* s) {
    delete s->player;
    s->player = nullptr;
    delete s->opl;
    s->opl = nullptr;
}

/* A player is bound to its chip at construction, so a new rate means a new chip and a re-parse of the image. */
static bool load_player(adplug_shim* s, uint32_t rate) {
    drop_player(s);
    s->opl = new CNemuopl((int)rate);
    MemoryProvider provider(s->image, s->name);
    s->player = CAdPlug::factory(s->name, s->opl, CAdPlug::players, provider);
    if (s->player == nullptr) {
        delete s->opl;
        s->opl = nullptr;
        return false;
    }
    s->rate = rate;
    return true;
}

adplug_shim* adplug_shim_open(const uint8_t* data, size_t size, const char* path) {
    if (data == nullptr || size == 0u || size > 0x7fffffffu || path == nullptr) {
        return nullptr;
    }
    adplug_shim* s = new adplug_shim();
    memset(s->info, 0, sizeof s->info);
    s->image.assign(data, data + size);
    s->name = path;
    if (!load_player(s, 44100u)) {
        adplug_shim_close(s);
        return nullptr;
    }
    copy_text(s->info[0], sizeof s->info[0], s->player->gettitle());
    copy_text(s->info[1], sizeof s->info[1], s->player->getauthor());
    copy_text(s->info[2], sizeof s->info[2], s->player->getdesc());
    copy_text(s->info[3], sizeof s->info[3], s->player->gettype());
    s->subsongs = s->player->getsubsongs();
    if (s->subsongs == 0u) {
        s->subsongs = 1u;
    }
    return s;
}

int adplug_shim_start(adplug_shim* s, uint32_t rate, uint32_t subsong) {
    if (s == nullptr || rate == 0u || subsong >= s->subsongs) {
        return 0;
    }
    if (rate != s->rate && !load_player(s, rate)) {
        return 0;
    }
    s->opl->init();
    s->player->rewind((int)subsong);
    float refresh = s->player->getrefresh();
    s->per_tick = (double)rate / (refresh > 0.0f ? (double)refresh : 70.0);
    s->accum = 0;
    s->ended = false;
    return 1;
}

const char* adplug_shim_info(const adplug_shim* s, uint32_t index) { return index < 4u ? s->info[index] : ""; }

uint32_t adplug_shim_subsongs(const adplug_shim* s) { return s->subsongs; }

uint32_t adplug_shim_render(adplug_shim* s, int16_t* out, uint32_t frames) {
    uint32_t done = 0;
    while (done < frames && !s->ended) {
        uint32_t n = (uint32_t)(s->per_tick - s->accum);
        if (n == 0u) {
            n = 1u;
        }
        if (n > frames - done) {
            n = frames - done;
        }
        s->opl->update(out + 2u * done, (int)n);
        done += n;
        s->accum += n;
        if (s->accum >= s->per_tick) {
            s->accum -= s->per_tick;
            if (!s->player->update()) {
                s->ended = true;
            }
            float refresh = s->player->getrefresh();
            if (refresh > 0.0f) {
                s->per_tick = (double)s->rate / (double)refresh;
            }
        }
    }
    return done;
}

void adplug_shim_close(adplug_shim* s) {
    if (s != nullptr) {
        drop_player(s);
        delete s;
    }
}
