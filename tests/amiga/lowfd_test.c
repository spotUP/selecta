/* Amiga test program: the library database opens when libnix hands SQLite a low file descriptor.
 * SQLite's unix VFS refuses a descriptor below SQLITE_MINIMUM_FILE_DESCRIPTOR, closes it and opens
 * "/dev/null" instead; libnix maps that to the volume "dev:" (requester "Please insert volume dev:", and the
 * open fails when it is cancelled or under vamos). Closing descriptor 0 makes the next open() return 0.
 * Usage: selecta-lowfd-test <db path>. Prints low_fd_open=ok, or exits non-zero. */
#include <selecta/host.h>
#include <selecta/machine.h>
#include "amiga_start.h"
#include "db.h"
#include <stdio.h>
#include <unistd.h>

static int app_main(int argc, char** argv) {
    const sel_host* host = sel_host_default();
    sel_library* lib;

    if (argc != 2) {
        fputs("usage: selecta-lowfd-test <db path>\n", stderr);
        return 2;
    }
    close(0);
    lib = sel_library_open(host, sel_machine_probe(host), 0, argv[1]);
    if (lib == NULL) {
        fputs("low_fd_open=failed\n", stdout);
        return 1;
    }
    sel_library_close(lib);
    fputs("low_fd_open=ok\n", stdout);
    return 0;
}

int main(int argc, char** argv) {
    return sel_amiga_run_with_stack(app_main, argc, argv, SEL_AMIGA_STACK_BYTES);
}
