/* I1383b: friendly text for a failed open (moved from the desktop toast, I695b / I1383). */
#include <selecta/uimodel/failtext.h>

#include <ctype.h>
#include <stdio.h>
#include <string.h>

int sel_uim_open_fail_text(const char* code, const char* detail, char* out, size_t cap) {
    int status = 0, used = 0;

    if (code == NULL || out == NULL || cap == 0u) {
        return 0;
    }
    if (detail == NULL) {
        detail = "";
    }
    if (strcmp(code, "no_replayer") == 0) {
        /* the engine's "format=<ext> <key>"; never the source id of the key */
        char fmt[16];
        size_t i;

        if (sscanf(detail, "format=%15s", fmt) != 1 || strcmp(fmt, "unknown") == 0) {
            snprintf(out, cap, "No player for that file type yet");
        } else {
            for (i = 0; fmt[i] != '\0'; i++) {
                fmt[i] = (char)toupper((unsigned char)fmt[i]);
            }
            snprintf(out, cap, "No player for %s yet", fmt);
        }
    } else if (strcmp(code, "open_failed") == 0 || strcmp(code, "unsupported_kind") == 0 ||
               strcmp(code, "replayer_incomplete") == 0 || strcmp(code, "replayer_load_failed") == 0) {
        if (sscanf(detail, "http_status=%d %n", &status, &used) >= 1 && used > 0 &&
            strstr(detail + used, "blocks downloads of this song") != NULL) {
            /* I1072d: the engine names the source that refuses the download: show its words. */
            snprintf(out, cap, "That song could not be downloaded: %s (HTTP %d)", detail + used, status);
        } else if (sscanf(detail, "http_status=%d", &status) == 1) {
            snprintf(out, cap, "That song could not be downloaded (HTTP %d)", status);
        } else {
            snprintf(out, cap, "That song cannot be played");
        }
    } else {
        return 0;
    }
    return 1;
}
