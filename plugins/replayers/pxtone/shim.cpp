// C ABI over pxtone (third_party/pxtone, Studio Pixel's pxtone play sample sources) for the plain C adapter.
// Memory-backed pxtnService I/O; the song is rendered once to its end (no loop), master volume 0.8 like the reference player.
#include "pxtone/pxtnService.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>

struct pxshim {
    pxtnService* svc;
    const uint8_t* data;
    int32_t size;
    int32_t pos;
    char title[64];
};

namespace {
bool io_read(void* user, void* dst, int32_t size, int32_t num) {
    pxshim* m = (pxshim*)user;
    int64_t total = (int64_t)size * num;
    if (total < 0 || m->pos + total > m->size) return false;
    memcpy(dst, m->data + m->pos, (size_t)total);
    m->pos += (int32_t)total;
    return true;
}
bool io_write(void*, const void*, int32_t, int32_t) { return false; }
bool io_seek(void* user, int32_t mode, int32_t val) {
    pxshim* m = (pxshim*)user;
    int64_t p;
    switch (mode) {
    case SEEK_SET: p = val; break;
    case SEEK_CUR: p = (int64_t)m->pos + val; break;
    case SEEK_END: p = (int64_t)m->size + val; break;
    default: return false;
    }
    if (p < 0 || p > m->size) return false;
    m->pos = (int32_t)p;
    return true;
}
bool io_pos(void* user, int32_t* out) {
    *out = ((pxshim*)user)->pos;
    return true;
}
} // namespace

extern "C" {

// `data` must stay valid until pxshim_close.
struct pxshim* pxshim_open(const uint8_t* data, uint32_t size, uint32_t rate) {
    pxshim* m = new (std::nothrow) pxshim();
    if (m == NULL) return NULL;
    memset(m->title, 0, sizeof m->title);
    m->data = data;
    m->size = (int32_t)size;
    m->pos = 0;
    m->svc = new (std::nothrow) pxtnService(io_read, io_write, io_seek, io_pos);
    pxtnVOMITPREPARATION prep;
    if (m->svc == NULL || m->svc->init() != pxtnOK || m->svc->read(m) != pxtnOK || m->svc->tones_ready() != pxtnOK ||
        !m->svc->set_destination_quality(2, (int32_t)rate)) {
        delete m->svc;
        delete m;
        return NULL;
    }
    memset(&prep, 0, sizeof prep);
    prep.master_volume = 0.8f;
    if (!m->svc->moo_preparation(&prep)) {
        delete m->svc;
        delete m;
        return NULL;
    }
    int32_t n = 0;
    const char* name = m->svc->text->get_name_buf(&n);
    if (name != NULL && n > 0) {
        if (n > (int32_t)sizeof m->title - 1) n = (int32_t)sizeof m->title - 1;
        memcpy(m->title, name, (size_t)n);
    }
    return m;
}

// Interleaved stereo S16; returns frames written, fewer than asked at the song's end.
uint32_t pxshim_render(struct pxshim* m, int16_t* out, uint32_t frames) {
    uint32_t done = 0;
    while (done < frames && !m->svc->moo_is_end_vomit()) {
        uint32_t chunk = frames - done > 1024u ? 1024u : frames - done;
        if (!m->svc->Moo(out + (size_t)done * 2u, (int32_t)(chunk * 4u))) {
            break;
        }
        done += chunk;
    }
    return done;
}

const char* pxshim_title(const struct pxshim* m) { return m->title; }

void pxshim_close(struct pxshim* m) {
    if (m != NULL) {
        delete m->svc;
        delete m;
    }
}

} // extern "C"
