/* I1440u: sel_library_open on a path whose drawer does not exist makes the drawer and the database, instead of failing. */
#include <selecta/host.h>
#include <selecta/machine.h>
#include "db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    const sel_host* host = sel_host_default();
    char root[] = "/tmp/selecta_drawer_XXXXXX";
    char dbp[400], cmd[460];
    sel_library* lib;
    int rc = 0;

    if (mkdtemp(root) == NULL) return 2;
    snprintf(dbp, sizeof dbp, "%s/cfg/library.db", root);
    lib = sel_library_open(host, sel_machine_probe(host), 0, dbp);
    if (lib == NULL) {
        printf("FAIL open in a missing drawer\n");
        rc = 1;
    } else {
        sel_library_close(lib);
    }
    snprintf(cmd, sizeof cmd, "rm -rf %s", root);
    if (system(cmd) != 0) rc = 1;
    if (rc == 0) printf("library_missing_drawer_ok\n");
    return rc;
}
