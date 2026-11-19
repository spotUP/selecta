/* I1470: no secret store on this platform; the DEViLBOX token stays in its 0600 file (keystore.h). */
#include "keystore.h"

int sel_keystore_available(void) {
    return 0;
}

int sel_keystore_has(const char* name) {
    (void)name;
    return 0;
}

int sel_keystore_get(const char* name, char* out, size_t cap) {
    (void)name;
    if (out != NULL && cap > 0u) out[0] = '\0';
    return 0;
}

int sel_keystore_put(const char* name, const char* secret) {
    (void)name;
    (void)secret;
    return 0;
}

int sel_keystore_del(const char* name) {
    (void)name;
    return 0;
}
