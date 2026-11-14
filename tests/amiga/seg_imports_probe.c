/* I1378a seg-imports-unit: linked only into the test variants of sel_pxtone (plugins/replayers/pxtone/CMakeLists.txt).
 * The constructor calls sel_tag_find, a core function the module does not link, so the call goes through the generated
 * thunk and the host's import table (cmake/amiga_seg_imports.cmake). A wrong answer calls exit(), which seg_crt turns
 * into "module init failed", so the render shows whether the import works. */
#include <selecta/tags.h>
#include <stdlib.h>

static const sel_tag k_probe_tags[] = {{0x1234u, 77u}, {0x5678u, 42u}, {SEL_TAG_END, 0u}};

__attribute__((constructor)) static void seg_imports_probe(void) {
    if (sel_tag_find(k_probe_tags, 0x5678u, 0u) != 42u || sel_tag_find(k_probe_tags, 0x9999u, 5u) != 5u) {
        exit(1);
    }
}
