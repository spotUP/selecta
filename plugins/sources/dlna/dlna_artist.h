/* I1410: the artist of a DLNA item. Servers (Synology, Minidlna) often leave upnp:artist / dc:creator empty for
 * module files; the title then holds "Artist_-_Title" or "Artist - Title". Never the server name. */
#ifndef SELECTA_DLNA_ARTIST_H
#define SELECTA_DLNA_ARTIST_H

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define SEL_DLNA_UNKNOWN_ARTIST "- unknown" /* the value asma / aminet / sndh use */

/* Fills artist and title (each `cap` bytes). A real `tag_artist` wins and the title stays as is. Else a title of the
 * form "A - B" (underscores count as spaces, both parts non-empty) gives artist A, title B; else the unknown value. */
static void sel_dlna_artist(const char* tag_artist, const char* raw_title, char* artist, char* title, size_t cap) {
    char tmp[512];
    char* sep;
    size_t i;

    if (tag_artist != NULL && tag_artist[0] != '\0') {
        snprintf(artist, cap, "%s", tag_artist);
        snprintf(title, cap, "%s", raw_title);
        return;
    }
    snprintf(tmp, sizeof tmp, "%s", raw_title);
    for (i = 0; tmp[i] != '\0'; i++) {
        if (tmp[i] == '_') {
            tmp[i] = ' ';
        }
    }
    sep = strstr(tmp, " - ");
    while (sep != NULL && sep == tmp) { /* leading " - ": look for a later separator */
        sep = strstr(sep + 3, " - ");
    }
    if (sep != NULL && sep[3] != '\0') {
        *sep = '\0';
        snprintf(artist, cap, "%s", tmp);
        snprintf(title, cap, "%s", sep + 3);
        return;
    }
    snprintf(artist, cap, "%s", SEL_DLNA_UNKNOWN_ARTIST);
    snprintf(title, cap, "%s", raw_title);
}

#endif
