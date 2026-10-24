/* Default integration: a player started without --integration opens the first registered entry
 * of the OS preference list, "none" when none is built. Regression: the Amiga build preferred
 * "mpris", which is never built there, and printed "integration 'mpris' is not available". */
#include <selecta/registry.h>

#include <stdio.h>
#include <string.h>

static int failures;

static void expect(const char* preferred, const char* want) {
    const char* got = sel_registry_default_integration(preferred);
    if (strcmp(got, want) != 0) {
        failures++;
        printf("FAIL preferred '%s': got '%s', want '%s'\n", preferred ? preferred : "(none)", got, want);
    }
}

int main(void) {
    sel_registry_init(sel_host_default());
#ifndef SEL_HAVE_MPRIS
    expect("mpris", "none"); /* preferred but not built: no error at start */
#endif
    expect("nowplaying-test", "nowplaying-test");
    expect("mpris-not-an-entry,nowplaying-test", "nowplaying-test"); /* first registered entry wins */
    expect("wav", "none");                                            /* an output, not an integration */
    expect("", "none");
    expect(NULL, "none");
    return failures ? 1 : 0;
}
