/* xxh32 against published vectors (xxHash reference, seed 0 unless noted) and the UADE key layout. */
#include "xxh32.h"

#include <stdio.h>
#include <string.h>

static int failures;

static void expect32(const char* what, uint32_t got, uint32_t want) {
    if (got != want) {
        printf("FAIL %s: got %08x want %08x\n", what, (unsigned)got, (unsigned)want);
        failures++;
    }
}

int main(void) {
    char key[SEL_UADE_KEY_CHARS + 1u];
    static unsigned char big[300000];
    size_t i;

    expect32("empty seed 0", sel_xxh32("", 0, 0), 0x02CC5D05u);
    expect32("a", sel_xxh32("a", 1, 0), 0x550D7456u);
    expect32("abc", sel_xxh32("abc", 3, 0), 0x32D153FFu);
    expect32("empty seed prime", sel_xxh32("", 0, 2654435761u), 0x36B78AE7u);
    /* 16 byte and longer inputs take the four-lane path; the 4-byte tail path is covered by "abc" and "a". */
    expect32("Nobody inspects the spammish repetition", sel_xxh32("Nobody inspects the spammish repetition", 39, 0),
             0xE2293B2Fu);

    /* key = hex8(xxh32(first 262144 bytes)) + hex4(len & 0xFFFF) */
    sel_uade_key("", 0, 0, key);
    if (strcmp(key, "02cc5d050000") != 0) {
        printf("FAIL empty key: %s\n", key);
        failures++;
    }
    for (i = 0; i < sizeof big; i++) {
        big[i] = (unsigned char)(i * 7u + 1u);
    }
    sel_uade_key(big, SEL_UADE_KEY_HASH_BYTES, sizeof big, key);
    {
        char want[13];

        snprintf(want, sizeof want, "%08x%04x", (unsigned)sel_xxh32(big, SEL_UADE_KEY_HASH_BYTES, 0),
                 (unsigned)(sizeof big & 0xFFFFu));
        if (strcmp(key, want) != 0) {
            printf("FAIL long key: %s want %s\n", key, want);
            failures++;
        }
        /* bytes past 262144 do not change the hash part */
        sel_uade_key(big, sizeof big, sizeof big, key);
        if (strcmp(key, want) != 0) {
            printf("FAIL hash window: %s want %s\n", key, want);
            failures++;
        }
    }
    if (failures == 0) {
        puts("xxh32_ok");
    }
    return failures != 0;
}
