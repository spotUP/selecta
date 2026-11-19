/* I1470: with the secret store on, an old devilbox.token file moves into the Mac Keychain once, the file goes, the token
 * reads back unlocked (no Touch ID: the move keeps it unlocked for the run) and log out deletes the item. The item is
 * keyed by a fresh temp config dir, so the user's real saved login is never touched. Exit 77 (skip) when this machine
 * has no usable login Keychain (locked CI runner). */
#include "net/devilbox.h"
#include "net/keystore.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(void) {
    char dir[] = "/tmp/selecta-keystore-XXXXXX";
    char path[300], token[SEL_DEVILBOX_TOKEN_MAX];
    FILE* f;
    int bad = 0;

    if (mkdtemp(dir) == NULL) return 1;
    snprintf(path, sizeof path, "%s/devilbox.token", dir);
    f = fopen(path, "wb");
    if (f == NULL) return 1;
    fputs("0123456789abcdef0123456789abcdef\n", f);
    fclose(f);
    sel_devilbox_set_config_dir(dir);
    sel_devilbox_use_keystore(1);

    if (!sel_devilbox_token(token, sizeof token) || strcmp(token, "0123456789abcdef0123456789abcdef") != 0) {
        printf("FAIL migration read '%s'\n", token);
        bad = 1;
    } else if (access(path, F_OK) == 0) {
        puts("SKIP the Keychain refused the item (no usable login Keychain)");
        remove(path);
        rmdir(dir);
        return 77;
    }
    if (!sel_devilbox_has_token() || !sel_keystore_has(dir)) {
        puts("FAIL no Keychain item after the move");
        bad = 1;
    }
    memset(token, 0, sizeof token);
    if (!sel_devilbox_token(token, sizeof token) || strcmp(token, "0123456789abcdef0123456789abcdef") != 0) {
        printf("FAIL unlocked read '%s'\n", token);
        bad = 1;
    }
    (void)sel_devilbox_logout();
    if (sel_devilbox_has_token() || sel_keystore_has(dir)) {
        puts("FAIL log out left the Keychain item");
        bad = 1;
    }
    rmdir(dir);
    if (!bad) puts("devilbox_keystore_ok");
    return bad;
}
