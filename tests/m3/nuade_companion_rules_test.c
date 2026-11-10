/* I1242f: the companion name rules of the nuade replayers, where they live (plugins/replayers/nuade/companion_rules.h).
 * For a song key of every family that keeps samples in a second file the library cache's rule matcher
 * (sel_companion_paths, the function sel_cache_fetch_song uses) yields the companion key the player opens; a family
 * without companions yields none. Names per player: third_party/nuade/players/<player>. */
#include "cache.h"
#include "../../plugins/replayers/nuade/companion_rules.h"

#include <stdio.h>
#include <string.h>

static int bad;

/* want: the companions that must be named, ';' separated, in order; "" = none. */
static void check(const char* song, const char* want) {
    static char out[SEL_CACHE_COMPANIONS_MAX][SEL_CACHE_PATH_MAX];
    char got[2048] = "";
    unsigned n = sel_companion_paths(g_companions, song, out, SEL_CACHE_COMPANIONS_MAX), i;
    for (i = 0; i < n; i++) {
        if (i != 0) {
            strcat(got, ";");
        }
        strcat(got, out[i]);
    }
    if (strcmp(got, want) != 0) {
        printf("FAIL %s: got [%s] want [%s]\n", song, got, want);
        bad = 1;
    }
}

int main(void) {
    check("Jason Page/Jason Page/jpn.empiresoccer94", "Jason Page/Jason Page/smp.empiresoccer94");
    check("Jason Page/X/jpnd.song", "Jason Page/X/jpns.song");
    check("Mark Cooksey Old/Mark Cooksey/mcr.aquablast", "Mark Cooksey Old/Mark Cooksey/mcs.aquablast");
    check("Pierre Adane Packer/A/pap.song", "Pierre Adane Packer/A/smp.song;Pierre Adane Packer/A/smp.set");
    check("Pierre Adane Packer/A/Pang/pang - ingame 01.pap", "Pierre Adane Packer/A/Pang/smp.set");
    check("PokeyNoise/Adam Gilmore/pn.zybex", "PokeyNoise/Adam Gilmore/pn.zybex.info");
    check("Synth Dream/Laurens Tummers/sdr.nobuddiesland hiscore",
          "Synth Dream/Laurens Tummers/smp.nobuddiesland hiscore;Synth Dream/Laurens Tummers/smp.set");
    check("SoundPlayer/Scott Johnston/sjs.awesome", "SoundPlayer/Scott Johnston/smp.awesome");
    check("Dirk Bialluch/Dirk Bialluch/tpu.timelock title", "Dirk Bialluch/Dirk Bialluch/smp.timelock title");
    check("ProTracker/x/mod.song", "");
    if (bad) {
        return 1;
    }
    puts("nuade_companion_rules_ok");
    return 0;
}
