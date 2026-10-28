/* I584: the run-length packing of the baked icon masks. The masks are anti-aliased line art with a few alpha levels and
 * long runs of 0, so a PackBits-style code keeps about a fifth of the bytes. The packer runs in the host baker
 * (tools/gui_iconbake.c), the unpacker at desktop start on every platform; integer only. */
#include <selecta/gui/gui_icons.h>

size_t sel_gui_icon_pack(const uint8_t* src, size_t n, uint8_t* dst) {
    size_t i = 0, o = 0;
    while (i < n) {
        size_t j = i;
        while (j < n && src[j] == src[i] && j - i < 129u) {
            j++;
        }
        if (j - i >= 2u) {
            dst[o++] = (uint8_t)(126u + (j - i));
            dst[o++] = src[i];
            i = j;
        } else {
            size_t k = i + 1u;
            while (k < n && k - i < 128u && !(k + 1u < n && src[k] == src[k + 1u])) {
                k++;
            }
            dst[o++] = (uint8_t)(k - i - 1u);
            while (i < k) {
                dst[o++] = src[i++];
            }
        }
    }
    return o;
}

int sel_gui_pack_decode(const uint8_t* s, size_t n, uint8_t* dst, size_t total) {
    const uint8_t* end = s + n;
    size_t o = 0;
    while (o < total && s < end) {
        unsigned c = *s++;
        if (c < 128u) {
            size_t len = (size_t)c + 1u;
            if ((size_t)(end - s) < len || total - o < len) {
                return 0;
            }
            while (len-- > 0u) {
                dst[o++] = *s++;
            }
        } else {
            size_t len = (size_t)c - 126u;
            if (s >= end || total - o < len) {
                return 0;
            }
            while (len-- > 0u) {
                dst[o++] = *s;
            }
            s++;
        }
    }
    return o == total && s == end;
}

int sel_gui_icon_unpack(const sel_gui_icons_packed* p, uint32_t size_idx, uint32_t id, uint8_t* dst) {
    size_t total;
    if (p == NULL || size_idx >= p->size_n || id >= p->count) {
        return 0;
    }
    total = (size_t)p->sizes[size_idx] * (size_t)p->sizes[size_idx];
    return sel_gui_pack_decode(p->stream[size_idx] + p->off[size_idx][id], p->off[size_idx][id + 1u] - p->off[size_idx][id],
                               dst, total);
}
