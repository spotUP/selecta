/* `selecta-cli machine [--tier <name>]`: runs the probe once and prints the sentinel
 * counter, then one NAME=value line per SEL_MACHINE_* tag. */
#include <selecta/machine.h>
#include <selecta/registry.h>

#include "cli.h"

#include <stdio.h>
#include <string.h>

#define MACHINE_PREFIX "MACHINE_"

int sel_cli_machine(int argc, char** argv) {
    const sel_tag* list;
    const sel_tag* iter;
    const sel_tag* t;
    int a;

    for (a = 0; a < argc; a++) {
        if (strcmp(argv[a], "--tier") == 0 && a + 1 < argc) {
            int tier = sel_machine_tier_by_name(argv[++a]);
            if (tier < 0) {
                fprintf(stderr, "unknown tier %s (text, planar, rtg, gpu)\n", argv[a]);
                return 1;
            }
            sel_machine_set_tier_override(tier);
        }
    }
    list = sel_machine_probe(sel_host_default());
    iter = list;
    sel_registry_init_machine(sel_host_default(), list);
    printf("default_output=%s\n", sel_registry_default_output(list));

    printf("probe_calls=%u\n", (unsigned)sel_machine_probe_calls());
    printf("clock.source=%s\n", sel_clock_source());
#if defined(__amigaos__)
    {
        uint32_t bytes, version;
        sel_machine_ahi_request(&bytes, &version);
        printf("ahi.request=%lu/%lu\n", (unsigned long)bytes, (unsigned long)version);
    }
#endif
    while ((t = sel_tag_next(&iter)) != NULL) {
        const char* name = sel_tag_name(t->tag);
        sel_tag_type type = SEL_TT_INT;

        if (name == NULL || strncmp(name, MACHINE_PREFIX, sizeof(MACHINE_PREFIX) - 1) != 0) {
            continue;
        }
        name += sizeof(MACHINE_PREFIX) - 1;
        (void)sel_tag_type_of(t->tag, &type);
        if (t->tag == SEL_MACHINE_CPU) {
            printf("%s=%s\n", name, sel_machine_cpu_name((uint32_t)t->value));
        } else if (t->tag == SEL_MACHINE_TIER) {
            printf("%s=%s\n", name, sel_machine_tier_name((uint32_t)t->value));
        } else if (type == SEL_TT_STR) {
            printf("%s=%s\n", name, (const char*)t->value);
        } else {
            printf("%s=%lu\n", name, (unsigned long)t->value);
        }
    }
    return 0;
}
