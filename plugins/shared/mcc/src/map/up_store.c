/* I685n: the owner side of the theme store (header up_store.h): the token array in force and the registry of rethemable objects.
 * Compiled into sel_mcc_static (Selecta plugin, host tests, and through it up-mcc.library); the class libraries do not link it. */
#include "map/up_store.h"
#include "map/mui_theme.h"

#include <stdlib.h>
#include <string.h>

#if defined(__amigaos__) || defined(AMIGA)
#include <exec/memory.h>
#include <exec/semaphores.h>
#include <proto/exec.h>
#define UP_ALLOC(n) AllocVec((n), MEMF_PUBLIC | MEMF_CLEAR)
#define UP_FREE(p) FreeVec(p)
static struct SignalSemaphore g_lock;
static int g_lock_ready;
static void lock(void) {
    if (g_lock_ready) {
        ObtainSemaphore(&g_lock);
    }
}
static void unlock(void) {
    if (g_lock_ready) {
        ReleaseSemaphore(&g_lock);
    }
}
#else
#define UP_ALLOC(n) calloc(1, (n))
#define UP_FREE(p) free(p)
static void lock(void) {}
static void unlock(void) {}
#endif

/* I676: the one store of the tokens in force. mui_theme_init stores the resolved theme here; every drawing class (aa_setup, the
 * balance fallback) reads it, so no class builds its own built-in copy and ignores the theme file. Unset: the built-in tokens.
 * I685n: moved here from mui_theme.c so the class libraries, which link mui_theme.c, carry no copy of it. */
static txt_theme g_tokens_theme;
static int g_tokens_set;

void mui_theme_tokens_set(const uint32_t* tokens) {
    if (tokens != NULL) {
        memcpy(g_tokens_theme.token, tokens, sizeof g_tokens_theme.token);
        g_tokens_set = 1;
    }
}

const uint32_t* mui_theme_tokens(void) {
    if (!g_tokens_set) {
        txt_theme_default(&g_tokens_theme);
        g_tokens_set = 1;
    }
    return g_tokens_theme.token;
}

typedef struct up_node {
    struct up_node* next;
    void *key, *obj, *app, *hook;
} up_node;
static up_node* g_nodes;
static uint32_t g_node_count;

void up_registry_init(void) {
#if defined(__amigaos__) || defined(AMIGA)
    if (!g_lock_ready) {
        InitSemaphore(&g_lock);
        g_lock_ready = 1;
    }
#endif
}

int up_registry_attach(void* key, void* obj, void* app, void* hook) {
    up_node* n;
    if (key == NULL || hook == NULL) {
        return -1;
    }
    n = (up_node*)UP_ALLOC(sizeof *n);
    if (n == NULL) {
        return -1;
    }
    n->key = key;
    n->obj = obj;
    n->app = app;
    n->hook = hook;
    lock();
    n->next = g_nodes;
    g_nodes = n;
    g_node_count++;
    unlock();
    return 0;
}

int up_registry_detach(void* key) {
    up_node **p, *n = NULL;
    lock();
    for (p = &g_nodes; *p != NULL; p = &(*p)->next) {
        if ((*p)->key == key) {
            n = *p;
            *p = n->next;
            g_node_count--;
            break;
        }
    }
    unlock();
    if (n == NULL) {
        return -1;
    }
    UP_FREE(n);
    return 0;
}

uint32_t up_registry_count(void) {
    return g_node_count;
}

void up_registry_each(up_registry_visit fn, void* user) {
    up_node* n;
    lock();
    for (n = g_nodes; n != NULL; n = n->next) {
        fn(user, n->key, n->obj, n->app, n->hook);
    }
    unlock();
}

void up_registry_free(void) {
    up_node* n;
    lock();
    while (g_nodes != NULL) {
        n = g_nodes;
        g_nodes = n->next;
        UP_FREE(n);
    }
    g_node_count = 0;
    unlock();
}
