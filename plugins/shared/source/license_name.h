/* Short display name of a Creative Commons / public-domain licence URL, shared by the streaming sources (jamendo,
 * archiveorg): "http://creativecommons.org/licenses/by-nc-sa/3.0/" -> "CC BY-NC-SA 3.0",
 * ".../publicdomain/zero/1.0/" -> "CC0 1.0", other /publicdomain/ URLs -> "Public Domain".
 * Anything else -> "Creative Commons". */
#ifndef SELECTA_PLUGINS_SHARED_LICENSE_NAME_H
#define SELECTA_PLUGINS_SHARED_LICENSE_NAME_H

#include <stddef.h>

void sel_license_name(const char* url, char* out, size_t n);

/* 1 when `url` is a Creative Commons or public-domain licence URL (host creativecommons.org, or a /publicdomain/
 * path), the only licences the Internet Archive source ships (I725d). */
int sel_license_is_open(const char* url);

#endif
