#include "host_internal.h"

#include <stdio.h>

void sel_host_log(const sel_host* h, int level, const char* msg) {
    static const char* const k_levels[] = {"[ERROR]", "[WARN]", "[INFO]", "[DEBUG]"};
    (void)h;
    if (level < 0 || level > SEL_LOG_DEBUG) {
        level = SEL_LOG_INFO;
    }
    fprintf(stderr, "%s %s\n", k_levels[level], msg);
}
