// SPDX-License-Identifier: GPL-2.0-only
// In-process TIATracker engine (row I429). The song check, the length measure and the TIA-to-stereo mix are bbmp's
// (bbmp-tiatracker.cpp, GPL-2.0-only), kept in step with it so output is the same; only the stdout protocol is gone.
#include "engine.h"

#include <cstring>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "qtshim.h"
#include "mainwindow.h"
#include "track/track.h"
#include "emulation/player.h"
#include "emulation/TIASnd.h"

QString MainWindow::lastMessage;

namespace {
const size_t MAX_FILE = 2u << 20;
const int MAX_MINUTES = 30;
std::set<std::pair<int, int>> g_rows;
bool g_watch = false, g_again = false;
}  // namespace

void Emulation::Player::newPlayerPos(int pos1, int pos2) {
    if (g_watch && !g_rows.insert({pos1, pos2}).second) g_again = true;
}
void Emulation::Player::invalidNoteFound(int, int, int, QString) {}

struct tia_engine {
    Track::Track track;
    std::string title, author;
    int fps = 60;
    long frames = 0;
    std::unique_ptr<Emulation::Player> player;
    std::unique_ptr<Emulation::TIASound> tia;
    uint32_t rate = 0;
    long frame = 0;
    long long sampleAt = 0;
    double dc[2] = {0, 0};
    std::vector<int16_t> pending;
    size_t pendingPos = 0;
};

namespace {
bool speedOk(int s) { return s >= 1 && s <= 255; }

// What the replay code would trust. true when the song can be played.
bool check(Track::Track& t) {
    for (int c = 0; c < 2; ++c) {
        const auto& seq = t.channelSequences[c].sequence;
        if (seq.size() == 0) return false;
        if (t.startPatterns[c] < 0 || t.startPatterns[c] >= seq.size()) return false;
        for (int e = 0; e < seq.size(); ++e) {
            int p = seq[e].patternIndex;
            if (p < 0 || p >= t.patterns.size()) return false;
            const auto& notes = t.patterns[p].notes;
            if (notes.size() == 0 || notes.size() > 4096) return false;
            for (const auto& n : notes) {
                if (n.type == Track::Note::instrumentType::Instrument
                        && (n.instrumentNumber < 0 || n.instrumentNumber >= Track::Track::numInstruments))
                    return false;
                if (n.type == Track::Note::instrumentType::Instrument && (n.value < 0 || n.value > 63)) return false;
                if (n.type == Track::Note::instrumentType::Slide && (n.value < -63 || n.value > 63)) return false;
                if (n.type == Track::Note::instrumentType::Percussion
                        && (n.instrumentNumber < 0 || n.instrumentNumber >= Track::Track::numPercussion))
                    return false;
            }
            if (c == 0 && !t.globalSpeed && (!speedOk(t.patterns[p].evenSpeed) || !speedOk(t.patterns[p].oddSpeed)))
                return false;
        }
    }
    return !(t.globalSpeed && (!speedOk(t.evenSpeed) || !speedOk(t.oddSpeed)));
}

void startPlayer(Emulation::Player& p, Track::Track& t) {
    int r0 = t.channelSequences[0].sequence[t.startPatterns[0]].firstNoteNumber;
    int r1 = t.channelSequences[1].sequence[t.startPatterns[1]].firstNoteNumber;
    p.playTrack(r0, r1);
}

long measure(Track::Track& t, long maxFrames) {
    Emulation::Player p(&t);
    g_rows.clear();
    g_again = false;
    g_watch = true;
    startPlayer(p, t);
    long frames = 0;
    while (frames < maxFrames) {
        p.bbmpFrame();
        if (g_again || !p.bbmpPlaying()) break;
        ++frames;
    }
    g_watch = false;
    return frames;
}

// One frame of the song: the replay code sets the registers, the TIA makes that frame's samples.
void nextFrame(tia_engine* e) {
    e->player->bbmpFrame();
    const uInt8* r = e->player->bbmpSound().registers();
    for (uInt16 a = Emulation::AUDC0; a <= Emulation::AUDV1; ++a) e->tia->set(a, r[a - Emulation::AUDC0]);
    long long next = (long long)((double)(e->frame + 1) * e->rate / e->fps + 0.5);
    uInt32 n = uInt32(next - e->sampleAt);
    e->sampleAt = next;
    std::vector<Int16> buf(size_t(n) * 2, 0);
    e->tia->process(buf.data(), n);
    e->pending.resize(size_t(n) * 2);
    e->pendingPos = 0;
    for (uInt32 k = 0; k < n; ++k) {
        double side[2];
        for (int c = 0; c < 2; ++c) {
            double v = buf[k * 2 + c];
            e->dc[c] += (v - e->dc[c]) * 0.0005;
            side[c] = (v - e->dc[c]) * 1.5;
        }
        for (int c = 0; c < 2; ++c) {
            double v = side[c] * 0.75 + side[1 - c] * 0.25;
            e->pending[k * 2 + c] = int16_t(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
        }
    }
    ++e->frame;
}
}  // namespace

extern "C" {

tia_engine* tia_open(const uint8_t* data, size_t size) {
    if (data == nullptr || size == 0 || size > MAX_FILE) return nullptr;
    try {
        std::string text((const char*)data, size);
        QJsonObject root;
        std::string why;
        if (!bbmpParseJson(text, root, why)) return nullptr;
        if (!root.contains("instruments") || !root.contains("patterns") || !root.contains("channels")) return nullptr;
        std::unique_ptr<tia_engine> e(new tia_engine);
        if (!e->track.fromJson(root) || !check(e->track)) return nullptr;
        e->fps = e->track.getTvMode() == TiaSound::TvStandard::PAL ? 50 : 60;
        e->frames = measure(e->track, long(MAX_MINUTES) * 60 * e->fps);
        if (e->frames <= 0) return nullptr;
        e->title = e->track.metaName.str();
        e->author = e->track.metaAuthor.str();
        return e.release();
    } catch (const std::exception&) {
        return nullptr;
    }
}

void tia_close(tia_engine* e) { delete e; }
const char* tia_title(const tia_engine* e) { return e->title.c_str(); }
const char* tia_author(const tia_engine* e) { return e->author.c_str(); }
uint32_t tia_duration_ms(const tia_engine* e) { return (uint32_t)((long long)e->frames * 1000 / e->fps); }

int tia_start(tia_engine* e, uint32_t rate) {
    if (rate < 8000u || rate > 192000u) return 0;
    try {
        e->rate = rate;
        e->frame = 0;
        e->sampleAt = 0;
        e->dc[0] = e->dc[1] = 0;
        e->pending.clear();
        e->pendingPos = 0;
        e->tia.reset(new Emulation::TIASound((Int32)rate));
        e->tia->channels(2, true);
        e->player.reset(new Emulation::Player(&e->track));
        startPlayer(*e->player, e->track);
        return 1;
    } catch (const std::exception&) {
        return 0;
    }
}

uint32_t tia_render(tia_engine* e, int16_t* out, uint32_t frames) {
    uint32_t done = 0;
    if (!e->player) return 0;
    try {
        while (done < frames) {
            if (e->pendingPos >= e->pending.size()) {
                if (e->frame >= e->frames) break;
                nextFrame(e);
                continue;
            }
            size_t have = (e->pending.size() - e->pendingPos) / 2;
            size_t take = have < frames - done ? have : frames - done;
            std::memcpy(out + size_t(done) * 2, e->pending.data() + e->pendingPos, take * 4);
            e->pendingPos += take * 2;
            done += (uint32_t)take;
        }
    } catch (const std::exception&) {
        e->frame = e->frames;
    }
    return done;
}

}  // extern "C"
