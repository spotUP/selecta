#include "license_name.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

int sel_license_is_open(const char* url) {
    return url != NULL && (strstr(url, "creativecommons.org/") != NULL || strstr(url, "/publicdomain/") != NULL);
}

void sel_license_name(const char* url, char* out, size_t n) {
    const char* p = strstr(url, "/licenses/");
    char kind[24], ver[12];
    size_t i = 0;

    snprintf(out, n, "Creative Commons");
    if (strstr(url, "/publicdomain/zero/") != NULL) {
        snprintf(out, n, "CC0 1.0");
        return;
    }
    if (strstr(url, "/publicdomain/") != NULL) {
        snprintf(out, n, "Public Domain");
        return;
    }
    if (p == NULL) {
        return;
    }
    p += 10;
    while (*p != '\0' && *p != '/' && i + 1 < sizeof kind) {
        kind[i++] = (char)toupper((unsigned char)*p++);
    }
    kind[i] = '\0';
    ver[0] = '\0';
    if (*p == '/') {
        p++;
        for (i = 0; *p != '\0' && *p != '/' && i + 1 < sizeof ver;) {
            ver[i++] = *p++;
        }
        ver[i] = '\0';
    }
    if (kind[0] != '\0') {
        snprintf(out, n, "CC %s%s%s", kind, ver[0] != '\0' ? " " : "", ver);
    }
}
