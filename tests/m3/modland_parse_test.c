/* I809: the Modland path parser takes the artist from the right folder. Formats with a tool/subformat folder
 * (Ad Lib, Spectrum, Video Game Music) skip it; every other format keeps <format>/<artist>/... */
#include "../../src/library/source.h"

#include <stdio.h>
#include <string.h>

static int g_bad;

static void expect(const char* path, const char* format, const char* artist) {
    char scratch[512];
    sel_tag tags[8];
    const sel_source* src = sel_source_modland();
    const char* f = NULL;
    const char* a = NULL;
    unsigned i;
    int ok = src != NULL && src->parse(path, scratch, sizeof scratch, tags);

    if (format == NULL) {
        if (ok) {
            printf("FAIL %s accepted\n", path);
            g_bad++;
        }
        return;
    }
    for (i = 0; ok && tags[i].tag != SEL_TAG_END; i++) {
        if (tags[i].tag == SEL_SRC_FORMAT) f = (const char*)tags[i].value;
        if (tags[i].tag == SEL_SRC_ARTIST) a = (const char*)tags[i].value;
    }
    if (!ok || f == NULL || a == NULL || strcmp(f, format) != 0 || strcmp(a, artist) != 0) {
        printf("FAIL %s -> %s / %s (want %s / %s)\n", path, f ? f : "?", a ? a : "?", format, artist);
        g_bad++;
    }
}

int main(void) {
    expect("pub/modules/Ad Lib/Surprise! AdLib Tracker 2.0/Hydra/virgill.sa2", "Ad Lib", "Hydra");
    expect("pub/modules/Spectrum/Pro Tracker 3/Some Artist/x.pt3", "Spectrum", "Some Artist");
    expect("pub/modules/Video Game Music/Sega Megadrive/Some Artist/Some Game/x.vgz", "Video Game Music", "Some Artist");
    expect("pub/modules/Ad Lib/Lonely/file.ext", "Ad Lib", "Lonely");
    expect("pub/modules/Protracker/Virgill/1000-baud.mod", "Protracker", "Virgill");
    expect("pub/modules/Nintendo SPC/Koji Kondo/Some Game/x.spc", "Nintendo SPC", "Koji Kondo");
    expect("pub/other/Protracker/Virgill/x.mod", NULL, NULL);
    expect("pub/modules/Protracker/x.mod", NULL, NULL);
    if (g_bad == 0) printf("modland_parse_ok\n");
    return g_bad != 0;
}
