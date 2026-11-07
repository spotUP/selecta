/* I685n (design 2026-11-06-mui-mcc-package.md section 8.4): the one theme store of all processes. The owner is up-mcc.library
 * (and, in the static build, the Selecta plugin): src/map/up_store.c holds the token array and the registry of rethemable objects.
 * The .mcc/.mcp class libraries do not link that file. They link src/map/up_store_client.c, which reaches the same store through
 * the functions of up-mcc.library (the ops table below; the Amiga binding is in lib/classheader.c). */
#ifndef MCC_UP_STORE_H
#define MCC_UP_STORE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --- owner side (up_store.c) --- */
/* The registry: one node per object that wants a recolour after a theme change. key identifies the node (the object's draw context),
 * obj and app are the MUI object and its application, hook is a struct Hook* the owner pushes to the application's own task. */
int up_registry_attach(void* key, void* obj, void* app, void* hook);
int up_registry_detach(void* key); /* 0, or -1 when key is not attached */
uint32_t up_registry_count(void);
typedef void (*up_registry_visit)(void* user, void* key, void* obj, void* app, void* hook);
/* Calls fn for every node with the registry locked; fn must not attach or detach. */
void up_registry_each(up_registry_visit fn, void* user);
void up_registry_init(void); /* library open: sets up the lock */
/* Library open (lib/support_lib.c): up_registry_init plus the theme file's tokens into the store; env_path NULL is ENV:UP-MCC/theme. */
void up_lib_init(const char* env_path);
void up_registry_free(void); /* library expunge: drops leftover nodes */

/* --- client side (up_store_client.c) --- */
typedef struct up_store_ops {
    const uint32_t* (*tokens)(void);
    int (*set_tokens)(const uint32_t* rgb, uint32_t count);
    int (*attach)(void* key, void* obj, void* app, void* hook);
    int (*detach)(void* key);
} up_store_ops;

typedef struct up_store_client {
    const up_store_ops* ops;
    uint32_t calls; /* sentinel: calls that went through the library */
} up_store_client;

void up_store_client_init(up_store_client* c, const up_store_ops* ops);
const uint32_t* up_store_client_tokens(up_store_client* c);
int up_store_client_set(up_store_client* c, const uint32_t* rgb, uint32_t count);
int up_store_client_attach(up_store_client* c, void* key, void* obj, void* app, void* hook);
int up_store_client_detach(up_store_client* c, void* key);
uint32_t up_store_client_calls(const up_store_client* c);

/* The client of the whole library (the class code's mui_theme_tokens and aa_setup go through this one). Bound once at library open. */
void up_store_bind(const up_store_ops* ops);
int up_store_attach(void* key, void* obj, void* app, void* hook);
int up_store_detach(void* key);

#ifdef __cplusplus
}
#endif
#endif
