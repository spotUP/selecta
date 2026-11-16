/* I1223h2: the adplug module claims .ksm through RP_CONTENT_COMPANIONS and names insts.dat, so the cache fetches it beside
 * a song opened alone; other extensions are not claimed. usage: adplug_ksm_companion_test <sel_module_adplug file> */
#include <selecta/plugin.h>
#include <selecta/replayer.h>

#include <dlfcn.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char** argv) {
    const sel_tag* (*entry)(uint32_t);
    sel_rp_content_companions_fn fn;
    const sel_tag* desc;
    const sel_tag* hooks;
    char out[2][SEL_RP_COMPANION_NAME_MAX];
    const uint8_t b[4] = {0};
    void* h = argc > 1 ? dlopen(argv[1], RTLD_NOW) : NULL;

    if (h == NULL || (entry = (const sel_tag* (*)(uint32_t))dlsym(h, "sel_plugin_entry")) == NULL) {
        fprintf(stderr, "cannot load module\n");
        return 1;
    }
    desc = entry(1);
    hooks = (const sel_tag*)sel_tag_find(desc, SEL_PLUGIN_TAGS, 0);
    fn = (sel_rp_content_companions_fn)sel_tag_find(hooks, SEL_RP_CONTENT_COMPANIONS, 0);
    if (fn == NULL || fn("labsng05.ksm", NULL, 0, NULL, 0, NULL) != 1 || fn("SONG.KSM", NULL, 0, NULL, 0, NULL) != 1 ||
        fn("song.mod", NULL, 0, NULL, 0, NULL) != 0 || fn("x.ksm", b, sizeof b, out, 2, NULL) != 1 || strcmp(out[0], "insts.dat") != 0) {
        fprintf(stderr, "ksm companion hook wrong\n");
        return 1;
    }
    puts("adplug_ksm_companion_ok");
    return 0;
}
