/* I685n (design 2026-11-06-mui-mcc-package.md section 8.4, gate item 2): one theme store for all processes. Two class-library
 * clients (src/map/up_store_client.c, the code every .mcc/.mcp links instead of its own store) are bound to ONE library, here the
 * host build of the library functions of lib/support_lib.c in sel_mcc_static. Setting tokens through client A must be seen by
 * client B (same array, same values) and the registry the library owns must count the objects both clients attached. */
#include "map/up_store.h"

#include <mcc/up.h>

#include <stdio.h>
#include <string.h>

#define CHECK(c, ...)                                  \
    do {                                               \
        if (!(c)) {                                    \
            fprintf(stderr, "FAIL line %d: ", __LINE__); \
            fprintf(stderr, __VA_ARGS__);              \
            fprintf(stderr, "\n");                     \
            return 1;                                  \
        }                                              \
    } while (0)

/* The host stand-in for opening up-mcc.library: the ops call the library functions directly, exactly as the Amiga build calls them
 * through the library base. Both clients get the same ops table because both open the same library. */
static const uint32_t* h_tokens(void) {
    return (const uint32_t*)(const void*)UP_ThemeTokens();
}
static int h_set(const uint32_t* rgb, uint32_t count) {
    return (int)UP_ThemeSetTokens((const ULONG*)(const void*)rgb, count);
}
static int h_attach(void* key, void* obj, void* app, void* hook) {
    return (int)UP_ThemeAttach(key, obj, app, hook);
}
static int h_detach(void* key) {
    return (int)UP_ThemeDetach(key);
}
static const up_store_ops g_lib_ops = {h_tokens, h_set, h_attach, h_detach};

int main(void) {
    up_store_client a, b;
    uint32_t known[SEL_GUI_TOKEN_COUNT];
    int obj_a, obj_b, app, hook;
    up_store_client_init(&a, &g_lib_ops);
    up_store_client_init(&b, &g_lib_ops);
    for (uint32_t i = 0; i < SEL_GUI_TOKEN_COUNT; i++) {
        known[i] = 0x203040u + i * 0x010203u;
    }
    CHECK(up_store_client_tokens(&a) == up_store_client_tokens(&b), "clients see two token arrays");
    CHECK(up_store_client_set(&a, known, SEL_GUI_TOKEN_COUNT) == 0, "set through client A refused");
    const uint32_t* tb = up_store_client_tokens(&b);
    CHECK(tb == up_store_client_tokens(&a), "client B reads another array than client A");
    CHECK(memcmp(tb, known, sizeof known) == 0, "client B does not see the tokens client A set");
    CHECK(tb == (const uint32_t*)(const void*)UP_ThemeTokens(), "clients do not read the library's array");
    /* the registry the library owns */
    uint32_t base = UP_ThemeObjects();
    CHECK(up_store_client_attach(&a, &obj_a, &obj_a, &app, &hook) == 0, "attach from A refused");
    CHECK(up_store_client_attach(&b, &obj_b, &obj_b, &app, &hook) == 0, "attach from B refused");
    CHECK(UP_ThemeObjects() == base + 2, "registry count %lu, wanted %lu", (unsigned long)UP_ThemeObjects(), (unsigned long)(base + 2));
    CHECK(up_store_client_detach(&a, &obj_a) == 0, "detach A refused");
    CHECK(UP_ThemeObjects() == base + 1, "registry count after detach %lu", (unsigned long)UP_ThemeObjects());
    CHECK(up_store_client_detach(&b, &obj_b) == 0, "detach B refused");
    CHECK(UP_ThemeObjects() == base, "registry not empty again");
    CHECK(up_store_client_detach(&b, &obj_b) != 0, "double detach accepted");
    /* sentinel: the shared path ran through the library, not a private copy */
    CHECK(up_store_client_calls(&a) >= 3 && up_store_client_calls(&b) >= 3, "client calls not counted (%u, %u)", up_store_client_calls(&a), up_store_client_calls(&b));
    return 0;
}
