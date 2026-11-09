/* I685n: the class-library side of the theme store (header up_store.h). Every .mcc/.mcp links this file in place of src/map/up_store.c;
 * the ops table is bound at library open to the functions of up-mcc.library (lib/classheader.c), so all processes read and write the
 * one store the library owns. With -DUP_CLASS_LIBRARY the file also defines mui_theme_tokens / mui_theme_tokens_set, which the class
 * code (aa_setup, balance, mui_theme.c) calls; without it only the instance API exists (host test with two clients). */
#include "map/up_store.h"

#include <stddef.h>

void up_store_client_init(up_store_client* c, const up_store_ops* ops) {
    c->ops = ops;
    c->calls = 0;
}

const uint32_t* up_store_client_tokens(up_store_client* c) {
    c->calls++;
    return c->ops->tokens();
}

int up_store_client_set(up_store_client* c, const uint32_t* rgb, uint32_t count) {
    c->calls++;
    return c->ops->set_tokens(rgb, count);
}

int up_store_client_attach(up_store_client* c, void* key, void* obj, void* app, void* hook) {
    c->calls++;
    return c->ops->attach(key, obj, app, hook);
}

int up_store_client_detach(up_store_client* c, void* key) {
    c->calls++;
    return c->ops->detach(key);
}

uint32_t up_store_client_calls(const up_store_client* c) {
    return c->calls;
}

#ifdef UP_CLASS_LIBRARY
#include "map/mui_theme.h"
#include <selecta/uimodel/theme.h>

static up_store_client g_lib_client;

void up_store_bind(const up_store_ops* ops) {
    up_store_client_init(&g_lib_client, ops);
}

int up_store_attach(void* key, void* obj, void* app, void* hook) {
    return g_lib_client.ops != NULL ? up_store_client_attach(&g_lib_client, key, obj, app, hook) : -1;
}

int up_store_detach(void* key) {
    return g_lib_client.ops != NULL ? up_store_client_detach(&g_lib_client, key) : -1;
}

/* Before the library is open (never in a loaded .mcc) there is no store: the built-in tokens, so a stray call cannot crash. */
const uint32_t* mui_theme_tokens(void) {
    static txt_theme fallback;
    static int ready;
    if (g_lib_client.ops != NULL) {
        return up_store_client_tokens(&g_lib_client);
    }
    if (!ready) {
        txt_theme_default(&fallback);
        ready = 1;
    }
    return fallback.token;
}

void mui_theme_tokens_set(const uint32_t* tokens) {
    if (tokens != NULL && g_lib_client.ops != NULL) {
        (void)up_store_client_set(&g_lib_client, tokens, SEL_GUI_TOKEN_COUNT);
    }
}
#endif
