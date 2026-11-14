/* I1378a seg-imports-unit: linked only into the test variants of sel_pxtone (plugins/replayers/pxtone/CMakeLists.txt).
 * The constructor calls sel_tag_find, a core function the module does not link, so the call goes through the generated
 * thunk and the host's import table (cmake/amiga_seg_imports.cmake). A wrong answer calls exit(), which seg_crt turns
 * into "module init failed", so the render shows whether the import works. */
#include <selecta/tags.h>
#include <stdlib.h>
#include <string.h>

static char* seg_probe_heap;

static const sel_tag k_probe_tags[] = {{0x1234u, 77u}, {0x5678u, 42u}, {SEL_TAG_END, 0u}};

__attribute__((constructor)) static void seg_imports_probe(void) {
    if (sel_tag_find(k_probe_tags, 0x5678u, 0u) != 42u || sel_tag_find(k_probe_tags, 0x9999u, 5u) != 5u) {
        exit(1);
    }
    /* I1378g: the heap goes through the host's import table too. strdup is libnix's (it calls _malloc, the thunk), realloc
     * and free are thunks; the block lives until the destructor, which runs in the module's fini (EXIT_LIST). */
    seg_probe_heap = strdup("selecta heap probe");
    if (seg_probe_heap == NULL || (seg_probe_heap = realloc(seg_probe_heap, 4096)) == NULL || strcmp(seg_probe_heap, "selecta heap probe") != 0) {
        exit(1);
    }
}

__attribute__((destructor)) static void seg_imports_probe_fini(void) {
    free(seg_probe_heap);
    seg_probe_heap = NULL;
}
