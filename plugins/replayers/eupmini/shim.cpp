// C ABI over eupmini (third_party/eupmini, GPL-2.0, Tomoaki Hayasaka's EUPPlayer) for the plain C adapter.
// The player renders a tick at a time into a FILE (output2File mode); a tmpfile is rewound before every tick, so it never
// holds more than one tick, and the tick is moved into a small FIFO the adapter drains. Pattern: the DEViLBOX harness.
#include "eupplayer.hpp"
#include "eupplayer_townsEmulator.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

struct pcm_struct pcm; // required by the library

namespace {
const uint32_t kHeader = 2048;

const uint8_t kDefaultFm[] = {
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', 17, 33, 10, 17, 25, 10, 57, 0, 154, 152, 218, 216, 15, 12, 7, 12,
    0, 5, 3, 5, 38, 40, 70, 40, 20, 0xc0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};
} // namespace

struct eupshim {
    EUPPlayer* player;
    EUP_TownsEmulator* dev;
    uint8_t* data;
    FILE* tick;
    std::vector<int16_t> fifo; // interleaved stereo
    size_t at;
};

extern "C" {

struct eupshim* eupshim_open(const uint8_t* song, uint32_t size, uint32_t rate) {
    if (size < kHeader + 6u) {
        return nullptr;
    }
    eupshim* s = new eupshim();
    s->at = 0;
    s->data = static_cast<uint8_t*>(malloc(size));
    s->tick = tmpfile();
    if (s->data == nullptr || s->tick == nullptr) {
        if (s->tick != nullptr) fclose(s->tick);
        free(s->data);
        delete s;
        return nullptr;
    }
    memcpy(s->data, song, size);
    s->dev = new EUP_TownsEmulator;
    s->player = new EUPPlayer;
    s->dev->outputSampleUnsigned(false);
    s->dev->outputSampleLSBFirst(true);
    s->dev->outputSampleSize(2);
    s->dev->outputSampleChannels(2);
    s->dev->rate(static_cast<int>(rate));
    s->dev->output2File(true);
    s->dev->outputStream(s->tick);
    s->player->outputDevice(s->dev);
    const uint8_t* buf = s->data;
    for (int trk = 0; trk < 32; trk++) s->player->mapTrack_toChannel(trk, buf[0x394 + trk]);
    for (int i = 0; i < 6; i++) s->dev->assignFmDeviceToChannel(buf[0x6D4 + i]);
    for (int i = 0; i < 8; i++) s->dev->assignPcmDeviceToChannel(buf[0x6DA + i]);
    s->player->tempo(buf[0x805] + 30);
    // No companion banks (.FMB/.PMB are not loaded): every FM slot keeps eupplay's default voice, PCM stays empty.
    for (int n = 0; n < 128; n++) s->dev->setFmInstrumentParameter(n, kDefaultFm);
    memset(&pcm, 0, sizeof pcm);
    s->player->startPlaying(s->data + kHeader + 6);
    return s;
}

// Fills up to `frames` stereo frames; returns the number produced (0 when the song has ended).
uint32_t eupshim_render(struct eupshim* s, int16_t* out, uint32_t frames) {
    uint32_t done = 0;
    while (done < frames) {
        if (s->at >= s->fifo.size()) {
            if (!s->player->isPlaying()) break;
            rewind(s->tick);
            s->player->nextTick();
            fflush(s->tick);
            long bytes = ftell(s->tick);
            rewind(s->tick);
            s->fifo.assign(static_cast<size_t>(bytes > 0 ? bytes : 0) / 2, 0);
            if (bytes > 0 && fread(s->fifo.data(), 2, s->fifo.size(), s->tick) != s->fifo.size()) s->fifo.clear();
            s->at = 0;
            continue;
        }
        size_t n = (s->fifo.size() - s->at) / 2;
        if (n > frames - done) n = frames - done;
        memcpy(out + static_cast<size_t>(done) * 2, s->fifo.data() + s->at, n * 4);
        s->at += n * 2;
        done += static_cast<uint32_t>(n);
    }
    return done;
}

const char* eupshim_title(struct eupshim* s) { return reinterpret_cast<const char*>(s->data); }

void eupshim_close(struct eupshim* s) {
    if (s == nullptr) return;
    s->player->stopPlaying();
    delete s->player;
    delete s->dev;
    fclose(s->tick);
    free(s->data);
    delete s;
}

} // extern "C"
