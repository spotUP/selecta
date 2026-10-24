/* I222 default output: sel_registry_default_output picks the first registered entry of the
 * machine's SEL_MACHINE_AUDIO list, "null" when none is. Hand-built machine tag lists. */
#include <selecta/registry.h>

#include <stdio.h>
#include <string.h>

static int failures;

static void expect(const char* audio, const char* want) {
    sel_tag list[2];
    const char* got;
    list[0].tag = SEL_MACHINE_AUDIO;
    list[0].value = (uintptr_t)audio;
    list[1].tag = SEL_TAG_END;
    list[1].value = 0;
    got = sel_registry_default_output(audio ? list : &list[1]);
    if (strcmp(got, want) != 0) {
        failures++;
        printf("FAIL audio '%s': got '%s', want '%s'\n", audio ? audio : "(none)", got, want);
    }
}

int main(void) {
    sel_registry_init(sel_host_default());
    expect("wav", "wav");
    expect("paula,ahi", "null"); /* neither is registered on the host */
    expect("wav-not-an-entry,null", "null");
    expect("nope,wav,null", "wav"); /* first registered entry wins, not the first entry */
    expect("", "null");
    expect(NULL, "null");
#ifdef __APPLE__
    expect("coreaudio", "coreaudio");
#endif
    return failures ? 1 : 0;
}
