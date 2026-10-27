/* Build-time icon baker of the desktop UI (DUI-P3-1, plan 2.3). Host tool, not part of src/gui (it uses floating
 * point, like the stb_truetype atlas builder of DUI-P2-3): reads the icon list from include/selecta/gui/gui_icons.h,
 * flattens the Lucide SVG elements (path, polygon, polyline, line, circle, rect) of third_party/lucide/icons into
 * polylines on the 24x24 grid, strokes them (width 2, round caps and joins) with 4x4 supersampling at each size and
 * writes the alpha masks as a C file. The UI then needs only integer mask blits.
 * Usage: gui_iconbake <gui_icons.h> <svg dir> <out.c> */
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <selecta/gui/gui_icons.h>

#define MAXSEG 4096
#define MAXICON 128
/* Every logical size 16, 20, 24 and 32 times the integer scales 1, 2 and 3 (DUI-P2-8), ascending. */
static const int k_sizes[] = {16, 20, 24, 32, 40, 48, 60, 64, 72, 96};
#define NSIZE 10
static int g_nsize = NSIZE; /* I584: sizes up to the optional max px argument */

typedef struct seg {
    double x0, y0, x1, y1;
} seg;
static seg g_seg[MAXSEG];
static int g_nseg;

static void add_seg(double x0, double y0, double x1, double y1) {
    if (g_nseg >= MAXSEG) {
        fprintf(stderr, "gui_iconbake: too many segments\n");
        exit(1);
    }
    g_seg[g_nseg].x0 = x0;
    g_seg[g_nseg].y0 = y0;
    g_seg[g_nseg].x1 = x1;
    g_seg[g_nseg].y1 = y1;
    g_nseg++;
}

static void bezier(double x0, double y0, double x1, double y1, double x2, double y2, double x3, double y3) {
    int i;
    double px = x0, py = y0;
    for (i = 1; i <= 12; i++) {
        double t = i / 12.0, u = 1 - t;
        double x = u * u * u * x0 + 3 * u * u * t * x1 + 3 * u * t * t * x2 + t * t * t * x3;
        double y = u * u * u * y0 + 3 * u * u * t * y1 + 3 * u * t * t * y2 + t * t * t * y3;
        add_seg(px, py, x, y);
        px = x;
        py = y;
    }
}

/* SVG endpoint arc to centre form, flattened. */
static void arc(double x1, double y1, double rx, double ry, double phi_deg, int fa, int fs, double x2, double y2) {
    double phi = phi_deg * M_PI / 180.0, cp = cos(phi), sp = sin(phi);
    double dx = (x1 - x2) / 2, dy = (y1 - y2) / 2;
    double x1p = cp * dx + sp * dy, y1p = -sp * dx + cp * dy;
    double lam, num, den, co, cxp, cyp, cx, cy, th1, dth;
    int i;
    if (rx == 0 || ry == 0) {
        add_seg(x1, y1, x2, y2);
        return;
    }
    rx = fabs(rx);
    ry = fabs(ry);
    lam = x1p * x1p / (rx * rx) + y1p * y1p / (ry * ry);
    if (lam > 1) {
        rx *= sqrt(lam);
        ry *= sqrt(lam);
    }
    num = rx * rx * ry * ry - rx * rx * y1p * y1p - ry * ry * x1p * x1p;
    den = rx * rx * y1p * y1p + ry * ry * x1p * x1p;
    co = den == 0 ? 0 : sqrt(num / den > 0 ? num / den : 0);
    if (fa == fs) {
        co = -co;
    }
    cxp = co * rx * y1p / ry;
    cyp = -co * ry * x1p / rx;
    cx = cp * cxp - sp * cyp + (x1 + x2) / 2;
    cy = sp * cxp + cp * cyp + (y1 + y2) / 2;
    th1 = atan2((y1p - cyp) / ry, (x1p - cxp) / rx);
    dth = atan2((-y1p - cyp) / ry, (-x1p - cxp) / rx) - th1;
    if (!fs && dth > 0) {
        dth -= 2 * M_PI;
    } else if (fs && dth < 0) {
        dth += 2 * M_PI;
    }
    {
        double px = x1, py = y1;
        for (i = 1; i <= 16; i++) {
            double th = th1 + dth * i / 16.0;
            double x = cp * rx * cos(th) - sp * ry * sin(th) + cx;
            double y = sp * rx * cos(th) + cp * ry * sin(th) + cy;
            add_seg(px, py, x, y);
            px = x;
            py = y;
        }
    }
}

static const char* skip_sep(const char* p) {
    while (*p == ' ' || *p == ',' || *p == '\n' || *p == '\t') {
        p++;
    }
    return p;
}

static double num(const char** pp) {
    char* e;
    double v;
    const char* p = skip_sep(*pp);
    v = strtod(p, &e);
    *pp = e;
    return v;
}

static void path(const char* d) {
    const char* p = d;
    char cmd = 0;
    double x = 0, y = 0, sx = 0, sy = 0;
    for (;;) {
        p = skip_sep(p);
        if (*p == '\0') {
            return;
        }
        if (isalpha((unsigned char)*p)) {
            cmd = *p++;
            if (cmd == 'Z' || cmd == 'z') {
                add_seg(x, y, sx, sy);
                x = sx;
                y = sy;
                continue;
            }
        }
        {
            int rel = islower((unsigned char)cmd) != 0;
            char up = (char)toupper((unsigned char)cmd);
            if (up == 'M' || up == 'L') {
                double nx = num(&p), ny = num(&p);
                if (rel) {
                    nx += x;
                    ny += y;
                }
                if (up == 'L') {
                    add_seg(x, y, nx, ny);
                } else {
                    sx = nx;
                    sy = ny;
                    cmd = rel ? 'l' : 'L';
                }
                x = nx;
                y = ny;
            } else if (up == 'H') {
                double nx = num(&p);
                nx = rel ? x + nx : nx;
                add_seg(x, y, nx, y);
                x = nx;
            } else if (up == 'V') {
                double ny = num(&p);
                ny = rel ? y + ny : ny;
                add_seg(x, y, x, ny);
                y = ny;
            } else if (up == 'C') {
                double a = num(&p), b = num(&p), c = num(&p), dd = num(&p), e = num(&p), f = num(&p);
                if (rel) {
                    a += x;
                    b += y;
                    c += x;
                    dd += y;
                    e += x;
                    f += y;
                }
                bezier(x, y, a, b, c, dd, e, f);
                x = e;
                y = f;
            } else if (up == 'A') {
                double rx = num(&p), ry = num(&p), rot = num(&p);
                int fa = (int)num(&p), fs = (int)num(&p);
                double nx = num(&p), ny = num(&p);
                if (rel) {
                    nx += x;
                    ny += y;
                }
                arc(x, y, rx, ry, rot, fa, fs, nx, ny);
                x = nx;
                y = ny;
            } else {
                fprintf(stderr, "gui_iconbake: unsupported path command '%c'\n", cmd);
                exit(1);
            }
        }
    }
}

/* Value of attribute `name` inside the tag text [t, e), or NULL. The value is copied to buf. */
static const char* attr(const char* t, const char* e, const char* name, char* buf, size_t cap) {
    char key[32];
    const char* p = t;
    size_t n;
    snprintf(key, sizeof key, "%s=\"", name);
    while ((p = strstr(p, key)) != NULL && p < e) {
        const char* v = p + strlen(key);
        const char* q;
        if (p == t || !isspace((unsigned char)p[-1])) { /* "rx=" must not match "x=" */
            p++;
            continue;
        }
        q = strchr(v, '"');
        n = (size_t)(q - v);
        if (n >= cap) {
            n = cap - 1;
        }
        memcpy(buf, v, n);
        buf[n] = '\0';
        return buf;
    }
    return NULL;
}

static double anum(const char* t, const char* e, const char* name, double dflt) {
    char b[64];
    return attr(t, e, name, b, sizeof b) != NULL ? atof(b) : dflt;
}

static void poly(const char* pts, int closed) {
    const char* p = pts;
    double fx = 0, fy = 0, px = 0, py = 0;
    int n = 0;
    while (*skip_sep(p) != '\0') {
        double x = num(&p), y = num(&p);
        if (n == 0) {
            fx = x;
            fy = y;
        } else {
            add_seg(px, py, x, y);
        }
        px = x;
        py = y;
        n++;
    }
    if (closed && n > 2) {
        add_seg(px, py, fx, fy);
    }
}

static void elements(const char* svg) {
    const char* p = svg;
    while ((p = strchr(p, '<')) != NULL) {
        const char* e = strchr(p, '>');
        char big[2048];
        if (e == NULL) {
            break;
        }
        if (strncmp(p, "<path", 5) == 0 && attr(p, e, "d", big, sizeof big)) {
            path(big);
        } else if (strncmp(p, "<polygon", 8) == 0 && attr(p, e, "points", big, sizeof big)) {
            poly(big, 1);
        } else if (strncmp(p, "<polyline", 9) == 0 && attr(p, e, "points", big, sizeof big)) {
            poly(big, 0);
        } else if (strncmp(p, "<line", 5) == 0) {
            add_seg(anum(p, e, "x1", 0), anum(p, e, "y1", 0), anum(p, e, "x2", 0), anum(p, e, "y2", 0));
        } else if (strncmp(p, "<circle", 7) == 0) {
            double cx = anum(p, e, "cx", 0), cy = anum(p, e, "cy", 0), r = anum(p, e, "r", 0);
            int i;
            for (i = 0; i < 32; i++) {
                double a0 = i * 2 * M_PI / 32, a1 = (i + 1) * 2 * M_PI / 32;
                add_seg(cx + r * cos(a0), cy + r * sin(a0), cx + r * cos(a1), cy + r * sin(a1));
            }
        } else if (strncmp(p, "<rect", 5) == 0) {
            double x = anum(p, e, "x", 0), y = anum(p, e, "y", 0), w = anum(p, e, "width", 0), h = anum(p, e, "height", 0);
            double rx = anum(p, e, "rx", 0), ry = anum(p, e, "ry", rx);
            if (rx <= 0 || ry <= 0) {
                rx = ry = 0;
            }
            rx = rx > w / 2 ? w / 2 : rx;
            ry = ry > h / 2 ? h / 2 : ry;
            add_seg(x + rx, y, x + w - rx, y);
            add_seg(x + w, y + ry, x + w, y + h - ry);
            add_seg(x + w - rx, y + h, x + rx, y + h);
            add_seg(x, y + h - ry, x, y + ry);
            if (rx > 0) { /* rounded corners: quarter ellipses, 8 segments each */
                static const double cxs[4] = {1, 1, 0, 0}, cys[4] = {0, 1, 1, 0}; /* corner centre selectors */
                int c, i;
                for (c = 0; c < 4; c++) {
                    double ccx = x + (cxs[c] != 0 ? w - rx : rx), ccy = y + (cys[c] != 0 ? h - ry : ry);
                    double base = (c - 1) * M_PI / 2; /* corner 0 (top right) sweeps -90..0 degrees */
                    for (i = 0; i < 8; i++) {
                        double a0 = base + i * M_PI / 16, a1 = base + (i + 1) * M_PI / 16;
                        add_seg(ccx + rx * cos(a0), ccy + ry * sin(a0), ccx + rx * cos(a1), ccy + ry * sin(a1));
                    }
                }
            }
        }
        p = e;
    }
}

static double dist2(double px, double py, const seg* s) {
    double dx = s->x1 - s->x0, dy = s->y1 - s->y0, l2 = dx * dx + dy * dy, t = 0;
    if (l2 > 0) {
        t = ((px - s->x0) * dx + (py - s->y0) * dy) / l2;
        t = t < 0 ? 0 : t > 1 ? 1 : t;
    }
    dx = s->x0 + t * dx - px;
    dy = s->y0 + t * dy - py;
    return dx * dx + dy * dy;
}

static void raster(int size, unsigned char* out) {
    double k = size / 24.0, half = 1.0 * (size < 20 ? 1.1 : 1.0); /* half stroke in 24-grid units; thicker at 16 */
    int x, y, sx, sy, i;
    for (y = 0; y < size; y++) {
        for (x = 0; x < size; x++) {
            int hit = 0;
            for (sy = 0; sy < 4; sy++) {
                for (sx = 0; sx < 4; sx++) {
                    double px = (x + (sx + 0.5) / 4) / k, py = (y + (sy + 0.5) / 4) / k;
                    for (i = 0; i < g_nseg; i++) {
                        if (dist2(px, py, &g_seg[i]) <= half * half) {
                            hit++;
                            break;
                        }
                    }
                }
            }
            out[y * size + x] = (unsigned char)(hit * 255 / 16);
        }
    }
}

int main(int argc, char** argv) {
    static char names[MAXICON][64];
    static unsigned char masks[MAXICON][NSIZE][96 * 96];
    char line[512];
    int n = 0, s, i, j;
    FILE* f;
    FILE* o;
    if (argc != 4 && argc != 5) {
        fprintf(stderr, "usage: gui_iconbake <gui_icons.h> <svg dir> <out.c> [max px]\n");
        return 2;
    }
    if (argc == 5) {
        int maxpx = atoi(argv[4]);
        g_nsize = 0;
        while (g_nsize < NSIZE && k_sizes[g_nsize] <= maxpx) {
            g_nsize++;
        }
        if (g_nsize == 0) {
            fprintf(stderr, "gui_iconbake: max px %s is below the smallest size\n", argv[4]);
            return 2;
        }
    }
    f = fopen(argv[1], "r");
    if (f == NULL) {
        fprintf(stderr, "gui_iconbake: cannot read %s\n", argv[1]);
        return 1;
    }
    while (fgets(line, sizeof line, f) != NULL) {
        char* p = strstr(line, "    X(");
        char* q;
        if (p == NULL || (q = strchr(p, '"')) == NULL) {
            continue;
        }
        q++;
        j = (int)strcspn(q, "\"");
        memcpy(names[n], q, (size_t)j);
        names[n][j] = '\0';
        n++;
    }
    fclose(f);
    for (i = 0; i < n; i++) {
        static char svg[8192];
        char path_[512];
        size_t len;
        snprintf(path_, sizeof path_, "%s/%s.svg", argv[2], names[i]);
        f = fopen(path_, "r");
        if (f == NULL) {
            fprintf(stderr, "gui_iconbake: cannot read %s\n", path_);
            return 1;
        }
        len = fread(svg, 1, sizeof svg - 1, f);
        fclose(f);
        svg[len] = '\0';
        g_nseg = 0;
        elements(svg);
        if (g_nseg == 0) {
            fprintf(stderr, "gui_iconbake: %s has no drawable element\n", path_);
            return 1;
        }
        for (s = 0; s < g_nsize; s++) {
            raster(k_sizes[s], masks[i][s]);
        }
    }
    o = fopen(argv[3], "w");
    if (o == NULL) {
        return 1;
    }
    fprintf(o, "/* Generated by tools/gui_iconbake.c from third_party/lucide (ISC). Do not edit. */\n#include <selecta/gui/gui_icons.h>\n\n");
    for (s = 0; s < g_nsize; s++) {
        size_t sq = (size_t)k_sizes[s] * (size_t)k_sizes[s];
        static uint8_t buf[2 * 96 * 96];
        uint32_t off = 0;
        uint32_t offs[MAXICON + 1];
        fprintf(o, "static const uint8_t k_pack%d[] = {", k_sizes[s]);
        for (i = 0; i < n; i++) {
            size_t len = sel_gui_icon_pack(masks[i][s], sq, buf);
            static uint8_t chk[96 * 96];
            sel_gui_icons_packed one;
            const int32_t one_size = k_sizes[s];
            const uint8_t* one_stream = buf;
            const uint32_t one_off[2] = {0u, (uint32_t)len};
            const uint32_t* one_offp = one_off;
            one.count = 1u;
            one.size_n = 1u;
            one.sizes = &one_size;
            one.stream = &one_stream;
            one.off = &one_offp;
            if (!sel_gui_icon_unpack(&one, 0u, 0u, chk) || memcmp(chk, masks[i][s], sq) != 0) {
                fprintf(stderr, "gui_iconbake: pack round trip failed for %s at %d\n", names[i], k_sizes[s]);
                return 1;
            }
            offs[i] = off;
            for (j = 0; j < (int)len; j++) {
                fprintf(o, "%s%u,", (off + (uint32_t)j) % 24u == 0u ? "\n" : "", (unsigned)buf[j]);
            }
            off += (uint32_t)len;
        }
        offs[n] = off;
        fprintf(o, "\n};\nstatic const uint32_t k_off%d[] = {", k_sizes[s]);
        for (i = 0; i <= n; i++) {
            fprintf(o, "%u,", (unsigned)offs[i]);
        }
        fprintf(o, "};\n");
    }
    fprintf(o, "static const int32_t k_sizes[] = {");
    for (s = 0; s < g_nsize; s++) {
        fprintf(o, "%d,", k_sizes[s]);
    }
    fprintf(o, "};\nstatic const uint8_t* const k_stream[] = {");
    for (s = 0; s < g_nsize; s++) {
        fprintf(o, "k_pack%d,", k_sizes[s]);
    }
    fprintf(o, "};\nstatic const uint32_t* const k_offs[] = {");
    for (s = 0; s < g_nsize; s++) {
        fprintf(o, "k_off%d,", k_sizes[s]);
    }
    fprintf(o, "};\nconst sel_gui_icons_packed sel_gui_icons_baked = {%d, %d, k_sizes, k_stream, k_offs};\n", n, g_nsize);
    fclose(o);
    return 0;
}
