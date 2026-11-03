/* I222 default output: sel_registry_default_output picks the first registered entry of the
 * machine's SEL_MACHINE_AUDIO list, "null" when none is. Hand-built machine tag lists. */
#include <selecta/machine.h>
#include <selecta/output.h>
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

static uint32_t half_rate(const sel_tag* machine) {
    return sel_tag_find(machine, SEL_MACHINE_CPU, 0) == SEL_CPU_68020 ? 28150u : 0u;
}

/* The app rate: the output's SEL_OUT_RATE_FOR for the machine, 44100 when it has none (or answers 0). */
static void check_rates(void) {
    sel_tag machine[2], none[1], with[2];
    machine[0].tag = SEL_MACHINE_CPU;
    machine[0].value = SEL_CPU_68020;
    machine[1].tag = SEL_TAG_END;
    machine[1].value = 0;
    none[0].tag = SEL_TAG_END;
    none[0].value = 0;
    with[0].tag = SEL_OUT_RATE_FOR;
    with[0].value = SEL_TAG_FN(half_rate);
    with[1].tag = SEL_TAG_END;
    with[1].value = 0;
    if (sel_registry_output_rate(none, machine) != 44100u || sel_registry_output_rate(NULL, machine) != 44100u) {
        failures++;
        puts("FAIL output without SEL_OUT_RATE_FOR must give 44100");
    }
    if (sel_registry_output_rate(with, machine) != 28150u) {
        failures++;
        puts("FAIL SEL_OUT_RATE_FOR not honoured");
    }
    machine[0].value = SEL_CPU_68060;
    if (sel_registry_output_rate(with, machine) != 44100u) {
        failures++;
        puts("FAIL rate 0 must fall back to 44100");
    }
}

/* I1023: a launch with no --ui picks the machine default: desktop (and its sdl backend) on macOS when the desktop plugin is
 * registered, text everywhere else. */
static void check_default_ui(void) {
    sel_tag none[1];
    const char* ui;
    const char* backend;
    int have_desktop = sel_registry_find(SEL_PLUGIN_KIND_UI, "desktop") != NULL;
    int want_desktop = 0;
#ifdef __APPLE__
    want_desktop = have_desktop;
#endif
    none[0].tag = SEL_TAG_END;
    none[0].value = 0;
    ui = sel_registry_default_ui(none);
    backend = sel_registry_default_backend(none);
    if (strcmp(ui, want_desktop ? "desktop" : "text") != 0) {
        failures++;
        printf("FAIL default ui '%s' (desktop registered: %d)\n", ui, have_desktop);
    }
    if (want_desktop ? (backend == NULL || strcmp(backend, "sdl") != 0) : backend != NULL) {
        failures++;
        puts("FAIL default backend must be sdl exactly when the default ui is desktop");
    }
}

int main(void) {
    check_rates();
    sel_registry_init(sel_host_default());
    check_default_ui();
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
